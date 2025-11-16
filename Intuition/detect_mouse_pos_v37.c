#include <exec/types.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <libraries/dos.h> // for RETURN_WARN, RETURN_OK
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pragmas/exec_pragmas.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/graphics_pragmas.h>

#define INTUITION_REV 37 // This revision is the latest I can use for my kickstart version (2.05)
#define GRAPHICS_REV 37

struct Library *IntuitionBase = NULL;
struct GfxBase *GfxBase;

struct TagItem windowTagItems[] = {
        {WA_Left,       20},
        {WA_Top,        20},
        {WA_MinWidth,   80},
        {WA_MinHeight,  20},
        {WA_MaxWidth,   ~0}, // Large as the screen width
        {WA_MaxHeight,  ~0}, // Large as the screen height
        {WA_Width,     400},
        {WA_Height,    150},
                        
        // System Gadgets
        {WA_CloseGadget, TRUE},
        {WA_SizeGadget,  TRUE},
        {WA_DepthGadget, TRUE},
        {WA_DragBar,     TRUE},
                        
        // Other attributes
        {WA_Activate,     TRUE},
        {WA_SmartRefresh, TRUE}, // read Amiga_ROM_Kernel_..._v2.04.pdf (p. 93)
        {WA_Title,        (ULONG)"Mouse events tracking"},
        
        // (together with MOUSEMOVE in IDCMPFlags) to be notified about mouse moving (even when outside active window)
        {WA_ReportMouse,   TRUE},
        
        {WA_GimmeZeroZero, FALSE},
                                        
        // Or of events we want to listen for
        {WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW},

        {TAG_DONE, NULL}
};

int windowRefreshCount = 0;

VOID cleanExit(struct Window *, LONG);
BOOL handleIDCMP(struct Window *, struct IntuiText *, BOOL);

int main(int argc, char **argv)
{
        BOOL useGZZ = FALSE;
        BOOL useBorderless = FALSE;
        int paramsI;
               
        struct Window *window;
        struct IntuiText intuiText;
        struct DrawInfo *screenDrawInfo;
        struct TextAttr textAttr;
                
        ULONG signalMask, winSignal, signals;
        
        ULONG screenTextPen, screenBackgroundPen;
        
        BOOL done = FALSE;
        
        for (paramsI=1; paramsI<argc; paramsI++)
        {
                if (stricmp(argv[paramsI], "gzz") == 0)
                {
                        useGZZ = TRUE;
                }
                
                if (stricmp(argv[paramsI], "borderless") == 0)
                {
                        useBorderless = TRUE;
                }
        }
        
        printf("useGZZ: %d, useBorderless: %d\n", useGZZ, useBorderless);
        
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
        
        window = OpenWindowTagList(NULL, windowTagItems);
        
        /**
        // Alternative to OpenWindowTagList(...)
        window = OpenWindowTags(NULL, // You could also pass a NewWindow struct instead of listing all params... (similar to old API)
                        WA_Left, 20,
                        WA_Top,  20,
                        WA_MinWidth,  80,
                        WA_MinHeight, 20,
                        WA_MaxWidth,  ~0, // Large as the screen width
                        WA_MaxHeight, ~0, // Large as the screen height
                        WA_Width, 400,
                        WA_Height, 150,
                        
                        // System Gadgets
                        WA_CloseGadget, TRUE,
                        WA_SizeGadget,  TRUE,
        
                        WA_DepthGadget, TRUE,
                        WA_DragBar,     TRUE,
                        
                        // Other attributes
                        WA_Activate,     TRUE,
                        WA_SmartRefresh, TRUE, // read Amiga_ROM_Kernel_..._v2.04.pdf (p. 93)
                        WA_Title,        "Mouse events tracking",
        
                        // (together with MOUSEMOVE in IDCMPFlags) to be notified about mouse moving (even when outside active window)
                        WA_ReportMouse,  TRUE,
        
                        WA_GimmeZeroZero, FALSE,
                        WA_Borderless,    FALSE,
                                        
                        // Or of events we want to listen for
                        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW,
                        
                        TAG_DONE);
        */
                
        // WA_GimmeZeroZero: will set the 0 coordinate below the titlebar, otherwise y=0 will mean you are drawing on top of it. I didn't understand if something really change about x...
        // WA_Borderless: doesn't draw window's borders
        if (useGZZ) {
                // 16 the index of WA_GimmeZeroZero
                windowTagItems[16].ti_Data = TRUE;
        }
        if (useBorderless) {
                windowTagItems[17].ti_Data = TRUE;
        } 
             
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
        // even using WA_GimmeZeroZero.      
        intuiText.LeftEdge = 0;
        intuiText.TopEdge = 0;
        intuiText.FrontPen = screenTextPen;
        intuiText.BackPen = screenBackgroundPen;
        intuiText.DrawMode = JAM1; // FrontPen color used
        intuiText.IText = (UBYTE *)"This is a longer string displayed with IntuiText gadget, it should redraw automatically and should not overlap with window's gadgets. Does it?";
        intuiText.ITextFont = &textAttr;
        intuiText.NextText = NULL;
        PrintIText(window->RPort, &intuiText, 20, 90);
        
        Move(window->RPort, 10, 20);
        Text(window->RPort, "Move the mouse inside the window...", 36);
        
        // Set up the signals for the events we want to hear about...
        winSignal = 1L << window->UserPort->mp_SigBit;
        signalMask = winSignal;
        
        while (!done)
        {
                signals = Wait(signalMask);
                
                // An event occurred - now act on the signal(s) we received.
                if (signals & winSignal)
                        done = handleIDCMP(window, &intuiText, useGZZ); // done if close gadget
        }
        
        cleanExit(window, RETURN_OK);
}

BOOL handleIDCMP(struct Window *window, struct IntuiText *intuiText, BOOL useGZZ)
{
        struct IntuiMessage *message;
        BOOL done = FALSE;
        
        char mousePosStr[40];
        
        char windowRefreshCountStr[17];

        while (message = (struct IntuiMessage *)GetMsg(window->UserPort))
        {       
                // Be sure to reply to every message received as soon as possible         
                ReplyMsg((struct Message *)message);
                
                switch (message->Class) 
                {
                        case IDCMP_REFRESHWINDOW:
                                windowRefreshCount += 1;
                                BeginRefresh(window);
                                sprintf(windowRefreshCountStr, "refreshCount: %d", windowRefreshCount);
                                printf("%s\n", windowRefreshCountStr);
                                Move(window->RPort, 10, 20);
                                Text(window->RPort, "Move the mouse inside the window...", 36);
                                PrintIText(window->RPort, intuiText, 20, 90);
                                EndRefresh(window, TRUE);
                                break;
                                
                        case IDCMP_MOUSEMOVE:
                                if (useGZZ)
                                {
                                        sprintf(mousePosStr, "Mouse %3d, %3d", window->GZZMouseX, window->GZZMouseY);
                                }
                                else
                                {
                                        sprintf(mousePosStr, "Mouse %3d, %3d", message->MouseX, message->MouseY);
                                }
                                Move(window->RPort, 10, 60);
                                Text(window->RPort, mousePosStr, strlen(mousePosStr));
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
        if (window)
                CloseWindow(window);
                
        if (GfxBase)
                CloseLibrary((struct Library *)GfxBase);
                
        if (IntuitionBase)
                CloseLibrary(IntuitionBase);
                
        exit(returnValue);       
}
