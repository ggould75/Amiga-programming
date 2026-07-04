#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>      // struct Rectangle (fields used by buildClipRegion)
#include <graphics/regions.h>  // struct Region

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/layers_protos.h>   // InstallClipRegion()

#include <libraries/dos.h> // for RETURN_WARN, RETURN_OK
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pragmas/exec_pragmas.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/graphics_pragmas.h>
#include <pragmas/layers_pragmas.h>

#define INTUITION_REV 37 // This revision is the latest I can use for my kickstart version (2.05)
#define GRAPHICS_REV 37
#define LAYERS_REV 37

struct Library *IntuitionBase = NULL;
struct GfxBase *GfxBase;
struct Library *LayersBase = NULL;

// Reusable interior clip region (borders excluded). Built once, rebuilt on
// resize, installed only transiently around our own drawing. See the
// "Interior clipping helpers" block at the bottom of the file.
struct Region *clipRegion = NULL;

struct TagItem windowTagItems[] = {
    {WA_Left, 20},
    {WA_Top, 20},
    {WA_MinWidth, 80},
    {WA_MinHeight, 20},
    {WA_MaxWidth, ~0}, // Large as the screen width
    {WA_MaxHeight, ~0}, // Large as the screen height
    {WA_Width, 400},
    {WA_Height, 150},
    // System Gadgets
    {WA_CloseGadget, TRUE},
    {WA_SizeGadget, TRUE},
    {WA_DepthGadget, TRUE},
    {WA_DragBar, TRUE},
    // Other attributes
    {WA_Activate, TRUE},
    {WA_SmartRefresh, TRUE}, // read Amiga_ROM_Kernel_..._v2.04.pdf (p. 93)
    {WA_Title, (ULONG)"Mouse events tracking"},
    // (together with MOUSEMOVE in IDCMPFlags) to be notified about mouse moving (even when outside active window)
    {WA_ReportMouse, TRUE},
    // Or of events we want to listen for
    {WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW},
    {TAG_DONE, NULL}
};

int windowRefreshCount = 0;

VOID cleanExit(struct Window *, LONG);
BOOL handleIDCMP(struct Window *, struct IntuiText *);
BOOL buildClipRegion(struct Window *);
VOID installClip(struct Window *);
VOID removeClip(struct Window *);
VOID drawStaticContent(struct Window *, struct IntuiText *);

int main(int argc, char **argv)
{
    struct Window *window;
    struct IntuiText intuiText;
    struct DrawInfo *screenDrawInfo;
    struct TextAttr textAttr;
    ULONG signalMask, winSignal, signals;
    ULONG screenTextPen, screenBackgroundPen;
    BOOL done = FALSE;

    IntuitionBase = OpenLibrary("intuition.library", INTUITION_REV);
    if (IntuitionBase == NULL)
    {
        printf("error opening IntuitionBase!\n");
        cleanExit(NULL, RETURN_WARN);
    }

    // Needed by Move(...), Text(...) as these are low level functions
    // not part of Intuition
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", GRAPHICS_REV);
    if (GfxBase == NULL)
    {
        printf("error opening GfxBase!\n");
        cleanExit(NULL, RETURN_WARN);
    }

    // layers.library is needed for InstallClipRegion(), which we use to clip
    // our own drawing to the window interior so it can't paint over the
    // border gadgets. NewRegion()/OrRectRegion()/ClearRegion()/DisposeRegion()
    // are in graphics.library, opened above.
    LayersBase = OpenLibrary("layers.library", LAYERS_REV);
    if (LayersBase == NULL)
    {
        printf("error opening LayersBase!\n");
        cleanExit(NULL, RETURN_WARN);
    }

    window = OpenWindowTagList(NULL, windowTagItems);

    /**
    // Alternative to OpenWindowTagList(...)
    window = OpenWindowTags(NULL, // You could also pass a NewWindow struct instead of listing all params... (similar to old API)
        WA_Left, 20,
        WA_Top, 20,
        WA_MinWidth, 80,
        WA_MinHeight, 20,
        WA_MaxWidth, ~0, // Large as the screen width
        WA_MaxHeight, ~0, // Large as the screen height
        WA_Width, 400,
        WA_Height, 150,
        // System Gadgets
        WA_CloseGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_DragBar, TRUE,
        // Other attributes
        WA_Activate, TRUE,
        WA_SmartRefresh, TRUE, // read Amiga_ROM_Kernel_..._v2.04.pdf (p. 93)
        WA_Title, "Mouse events tracking",
        // (together with MOUSEMOVE in IDCMPFlags) to be notified about mouse moving (even when outside active window)
        WA_ReportMouse, TRUE,
        // Or of events we want to listen for
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW,
        TAG_DONE);
    */

    if (window == NULL)
        cleanExit(NULL, RETURN_WARN);

    // This is the recommended way to get screen configurations,
    // such as foreground/background Pen colors etc...
    screenDrawInfo = GetScreenDrawInfo(window->WScreen);
    screenTextPen = screenDrawInfo->dri_Pens[TEXTPEN];
    screenBackgroundPen = screenDrawInfo->dri_Pens[BACKGROUNDPEN];
    textAttr.ta_Name = screenDrawInfo->dri_Font->tf_Message.mn_Node.ln_Name;
    textAttr.ta_YSize = screenDrawInfo->dri_Font->tf_YSize;
    textAttr.ta_Style = screenDrawInfo->dri_Font->tf_Style;
    textAttr.ta_Flags = screenDrawInfo->dri_Font->tf_Flags;
    FreeScreenDrawInfo(window->WScreen, screenDrawInfo);

    // Setting proper FrontPen/BackPen color is quite essential,
    // otherwise you may not see anything.
    // Using IntuiText vs Text(window-RPort...) has the advantage of being more configurable (font, drawmode etc...)
    // but overall it doesn't solve the issue of text overflowing on window's widgets, such as vertical scroll bar...
    // The interior clip region below is what actually keeps the text off the gadgets.
    intuiText.LeftEdge = 0;
    intuiText.TopEdge = 0;
    intuiText.FrontPen = screenTextPen;
    intuiText.BackPen = screenBackgroundPen;
    intuiText.DrawMode = JAM1; // FrontPen color used
    intuiText.IText = (UBYTE *)"This is a longer string displayed with IntuiText gadget, it should redraw automatically and should not overlap with window's gadgets. Does it?";
    intuiText.ITextFont = &textAttr;
    intuiText.NextText = NULL;

    // Build the interior clip region for the current window size, then draw the
    // static text through it. With the region installed, the long IntuiText is
    // clipped at the interior right edge instead of overwriting the sizing
    // gadget in the right border.
    if (!buildClipRegion(window))
        printf("warning: could not build clip region; text may overlap gadgets\n");
    drawStaticContent(window, &intuiText);

    // Set up the signals for the events we want to hear about...
    winSignal = 1L << window->UserPort->mp_SigBit;
    signalMask = winSignal;

    while (!done)
    {
        signals = Wait(signalMask);
        // An event occurred - now act on the signal(s) we received.
        if (signals & winSignal)
            done = handleIDCMP(window, &intuiText); // done if close gadget
    }

    cleanExit(window, RETURN_OK);
}

BOOL handleIDCMP(struct Window *window, struct IntuiText *intuiText)
{
    struct IntuiMessage *message;
    BOOL done = FALSE;
    char mousePosStr[40];
    char windowRefreshCountStr[32];   // was [17]: too small once the count passes 99

    while (message = (struct IntuiMessage *)GetMsg(window->UserPort))
    {
        // Be sure to reply to every message received as soon as possible
        ReplyMsg((struct Message *)message);

        switch (message->Class)
        {
            case IDCMP_REFRESHWINDOW:
                windowRefreshCount += 1;
                sprintf(windowRefreshCountStr, "refreshCount: %d", windowRefreshCount);
                printf("%s\n", windowRefreshCountStr);

                // Acknowledge the refresh: this clears the layer's damage list
                // and lets Intuition repaint the window frame (borders +
                // gadgets). We draw nothing of our own inside this pair, and
                // must NOT call InstallClipRegion() here - it is forbidden
                // inside a BeginRefresh()/EndRefresh() pair (RKM Libraries,
                // Layers chapter, "A Warning About InstallClipRegion()").
                BeginRefresh(window);
                EndRefresh(window, TRUE);

                // The window may have just been resized, so rebuild the
                // interior region for the new border sizes (safe here: the
                // region is not installed at this point), then redraw our
                // content through it - all OUTSIDE the refresh pair.
                buildClipRegion(window);
                drawStaticContent(window, intuiText);
                break;

            case IDCMP_MOUSEMOVE:
                sprintf(mousePosStr, "Mouse %3d, %3d", message->MouseX, message->MouseY);

                // Clip this line to the interior too, so it can't spill onto
                // the gadgets if the window is made very narrow. We reuse the
                // single clip region (install/remove around the draw) to avoid
                // allocating a fresh region on every mouse move.
                installClip(window);
                Move(window->RPort, 10, 60);
                Text(window->RPort, mousePosStr, strlen(mousePosStr));
                removeClip(window);
                break;

            case IDCMP_CLOSEWINDOW:
                done = TRUE;
                break;

            default:
                break;
        } // end-of-switch
    } // end-of-while

    return(done);
}

VOID cleanExit(struct Window *window, LONG returnValue)
{
    // Remove our clip region from the layer before the window/layer goes away,
    // then dispose of the reusable region. A region must be taken off the layer
    // with InstallClipRegion(layer, NULL) before that layer is destroyed.
    if (window != NULL && LayersBase != NULL)
        InstallClipRegion(window->WLayer, NULL);
    if (clipRegion != NULL)
    {
        DisposeRegion(clipRegion);
        clipRegion = NULL;
    }

    if (window)
        CloseWindow(window);
    if (LayersBase)
        CloseLibrary(LayersBase);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary(IntuitionBase);
    exit(returnValue);
}

// ---------------------------------------------------------------------------
// Interior clipping helpers
//
// A normal (non-GimmeZeroZero) window is a single layer spanning the whole
// window, borders included, and window->RPort drawing is clipped only to the
// window's OUTER edge. So long text can paint over the border gadgets. To stop
// that we install a clip region covering just the interior (borders excluded)
// AROUND our own drawing, then remove it again so Intuition can still render
// the frame normally. (Leaving a region permanently installed would block the
// frame/gadget repaint, since the frame lies outside the interior.)
//
// Interior geometry is the same as clipWindowToBorders() in the RKM
// "clipping.c" example:
//   (BorderLeft, BorderTop) .. (Width-BorderRight-1, Height-BorderBottom-1)
// The "-1" is the inclusive far-edge adjustment - same idea as the M_R/W_R
// macros in the layers example.
//
// We keep ONE reusable region rather than allocating a new one per draw, since
// IDCMP_MOUSEMOVE fires rapidly. It is rebuilt only when the size may have
// changed (on refresh), and only while it is NOT installed in the layer
// (required: an installed region must not be modified - RKM, "Do Not Modify A
// Region After It Has Been Added").
// ---------------------------------------------------------------------------

// (Re)build clipRegion for the window's current interior. Does NOT install it.
// Returns FALSE if the region could not be allocated or built.
BOOL buildClipRegion(struct Window *win)
{
    struct Rectangle rect;

    if (clipRegion == NULL)
    {
        clipRegion = NewRegion();
        if (clipRegion == NULL)
            return(FALSE);
    }
    else
    {
        ClearRegion(clipRegion);   // empty it before redefining (not installed)
    }

    rect.MinX = win->BorderLeft;
    rect.MinY = win->BorderTop;
    rect.MaxX = win->Width  - win->BorderRight  - 1;
    rect.MaxY = win->Height - win->BorderBottom - 1;

    return(OrRectRegion(clipRegion, &rect));   // TRUE on success
}

// Install the interior clip around a block of our drawing.
VOID installClip(struct Window *win)
{
    if (clipRegion != NULL)
        InstallClipRegion(win->WLayer, clipRegion);
}

// Remove the interior clip (keep the region allocated for reuse).
VOID removeClip(struct Window *win)
{
    if (clipRegion != NULL)
        InstallClipRegion(win->WLayer, NULL);
}

// Draw the two static lines, clipped to the interior.
VOID drawStaticContent(struct Window *win, struct IntuiText *intuiText)
{
    installClip(win);
    Move(win->RPort, 10, 20);
    Text(win->RPort, "Move the mouse inside the window...", 36);
    PrintIText(win->RPort, intuiText, 20, 90);
    removeClip(win);
}
