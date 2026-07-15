/*
 * screen_draw.c
 *
 * Draw one 40x30 filled rectangle at a random position directly onto the
 * default public screen (normally Workbench) -- with NO window of our own.
 *
 * The point of the experiment: we render into the SCREEN's RastPort
 * (&scr->RastPort), whose Layer is NULL. Graphics primitives on a layer-less
 * RastPort write straight to the bitmap, ignoring the layers library -- so the
 * rectangle lands on top of whatever windows happen to be there. Boing does the
 * same thing; it just owns the whole screen instead of vandalising someone
 * else's.
 *
 * Why the rectangle mangles when drawn over the Shell:
 * We wrote unowned pixels into a shared bitmap. The Shell's layer has no record
 * of them, so when the following printf() pushes the cursor down, the console
 * ScrollRaster()s its whole region up one text line -- a plain blit that carries
 * our pixels along, because to it they are just bits. That is why old rectangles
 * shift upward instead of being erased. The colour breaks up because pen 3 on a
 * 4-colour screen is BOTH planes set (11); as the console repaints text it
 * rewrites the planes unevenly, so 11 decays locally to 01 (black) or 10 (white),
 * or is overwritten outright by glyph pixels -- hence the black/white/mixed mess.
 * On the bare Workbench backdrop, or when the print triggers no scroll, nothing
 * repaints the area afterward and the fill stays solid blue. The Delay() below
 * just buys a second to see it clean before the scroll happens -- it postpones
 * the damage, it doesn't prevent it. The only real cure is to own the drawing
 * surface (a custom screen, or a window whose layer manages refresh).
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>          /* DateStamp(); DOSBase is opened by startup */

#include <stdio.h>
#include <stdlib.h>             /* rand(), srand() */

#define RECT_W 40
#define RECT_H 30
#define PEN     3               /* WB pen 3 (blue on the default 4-colour WB) */

/* We open these two ourselves, at V37, so the program refuses to run on a
 * pre-2.0 machine instead of failing later inside a missing function.
 * (SysBase and DOSBase are provided by the SAS/C startup -- we don't touch
 *  them, and we must NOT redeclare them.) */
struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase       *GfxBase       = NULL;

int main(void)
{
    struct Screen   *scr = NULL;
    struct RastPort *rp;
    struct DateStamp ds;
    WORD minY, maxX, maxY, x, y;
    int rc = RETURN_FAIL;

    IntuitionBase = (struct IntuitionBase *)
                        OpenLibrary("intuition.library", 37L);
    if (IntuitionBase == NULL) {
        printf("Need intuition.library V37+\n");
        goto cleanup;
    }

    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37L);
    if (GfxBase == NULL) {
        printf("Need graphics.library V37+\n");
        goto cleanup;
    }

    /* Lock the default public screen (Workbench). The lock stops the screen
     * from being closed out from under us while we draw. V36+ API. */
    scr = LockPubScreen(NULL);
    if (scr == NULL) {
        printf("No default public screen -- is Workbench open?\n");
        goto cleanup;
    }

    /* Seed the RNG. DateStamp() fills days/minute/tick since the epoch; the
     * tick field (1/50 s) changes run-to-run, which is plenty for a demo. */
    DateStamp(&ds);
    srand((unsigned int)(ds.ds_Tick + ds.ds_Minute));

    /* Random top-left, keeping the whole rect on-screen and clear of the
     * screen title/menu bar. scr->BarHeight is the bar height; usable area
     * starts one line below it. */
    minY = (WORD)(scr->BarHeight + 1);
    maxX = (WORD)(scr->Width  - RECT_W);
    maxY = (WORD)(scr->Height - RECT_H);
    if (maxX < 0)     maxX = 0;
    if (maxY < minY)  maxY = minY;

    x = (WORD)(rand() % (maxX + 1));
    y = (WORD)(minY + rand() % (maxY - minY + 1));

    /* THE point of the exercise: the screen's own RastPort has Layer == NULL,
     * so this fill bypasses the layers library and hits the bitmap directly,
     * landing over any windows. rp->Mask is 0xFF by default (all planes). */
    rp = &scr->RastPort;

    SetAPen(rp, PEN);
    RectFill(rp, x, y, x + RECT_W - 1, y + RECT_H - 1);  /* inclusive coords */

    /* RectFill uses the blitter and may return before it finishes. We don't
     * free or re-read anything here, so it isn't strictly required -- but
     * waiting for the blitter before we walk away is the tidy habit. */
    WaitBlit();

    /* Hold the fill on screen for ~1 s BEFORE we print. The scroll is caused by
     * printf() advancing the cursor; delaying first lets the rectangle sit
     * uniform for a moment, then the scroll (and the recolouring) happens.
     * 50 ticks = 1 s on PAL (TICKS_PER_SECOND, <dos/dos.h>). Delay() is a
     * Wait()-based sleep -- it blocks this process without burning CPU. */
    Delay(50L);

    printf("Drew a %dx%d rect at (%d,%d) on '%s' (%dx%d, bar=%d)\n",
           RECT_W, RECT_H, x, y,
           scr->Title ? (char *)scr->Title : "public screen",
           scr->Width, scr->Height, scr->BarHeight);

    rc = RETURN_OK;

cleanup:
    /* Reverse order. UnlockPubScreen only drops our lock; it frees nothing and
     * does NOT erase what we drew -- the rectangle stays on the bitmap until
     * something (a moving window, a Workbench refresh) repaints that area. */
    if (scr)           UnlockPubScreen(NULL, scr);
    if (GfxBase)       CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);

    return rc;
}
