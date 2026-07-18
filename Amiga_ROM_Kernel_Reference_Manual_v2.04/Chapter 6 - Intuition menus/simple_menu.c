/* simplemenu.c - Execute me to compile me with SAS C 5.10
LC -b1 -cfistq -v -y -j73 simplemenu.c
Blink FROM LIB:c.o,simplemenu.o TO simplemenu LIBRARY LIB:LC.lib,LIB:Amiga.lib
quit
** simplemenu.c:  how to use the menu system with a window under all OS versions.
*/

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>

#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>
#include <string.h>

#ifdef LATTICE
int CXBRK(void)    { return(0); }  /* Disable Lattice CTRL/C handling */
int chkabort(void) { return(0); }  /* really */
#endif

/* These values are based on the ROM font Topaz8.  Adjust these */
/* values to correctly handle the screen's current font.        */
#define MENWIDTH  (56+8)   /* Longest menu item name * font width */
                           /* + 8 pixels for trim                 */

// Font height + 2 pixels.
// Literally the height of each item in the menu; the highlighted area on
// mouse over will be set to this height.
// You have to account for the height of the font used!
// If you set it too high, the highlighted area will be higher than the
// vertical space taken by the item, if you set it too light items will overlap!
#define MENHEIGHT (10)

struct Library *GfxBase;
struct Library *IntuitionBase;

/* To keep this example simple, we'll hard-code the font used for menu */
/* items.  Algorithmic layout can be used to handle arbitrary fonts.   */
/* Under Release 2, GadTools provides font-sensitive menu layout.      */
/* Note that we still must handle fonts for the menu headers.          */

struct TextAttr Topaz80 =
{
    "topaz.font", 8, 0, 0
};

struct IntuiText menuIText[] =
{
    { 0, 1, JAM2, 0, 1, &Topaz80, "Open...",  NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "Save",     NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "Print \273", NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "Draft",    NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "NLQ",      NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "Quit",     NULL }
};

struct MenuItem submenu1[] =
{
    {   /* Draft */
        &submenu1[1], MENWIDTH-2, -2 , MENWIDTH, MENHEIGHT,
        ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
        0, (APTR)&menuIText[3], NULL, NULL, NULL, NULL
    },
    {   /* NLQ */
        NULL, MENWIDTH-2, MENHEIGHT-2, MENWIDTH, MENHEIGHT,
        ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
        0, (APTR)&menuIText[4], NULL, NULL, NULL, NULL
    }
};

struct MenuItem menu1[] =
{
    {   /* Open... */
        &menu1[1],  // next menu item
        0,          // left edge (extra left-padding from the leading of the menu)
        0,          // top edge 
        MENWIDTH,   // dimensions of the selected box
        MENHEIGHT,
        ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP, // flags
        0,          // MutualExclude: set bits mean this item excludes that item
        (APTR)&menuIText[0],  // points to Image, IntuiText, or NULL
        NULL,       // Equivalent to previous param, but displayed on selected state (mouse over)
        NULL,       // command 
        NULL,       // a subitem if needed (MenuItem type)
        NULL        // the menu number of the next selected item (when user has drag-selected several items)
    },
    {   /* Save */
        &menu1[2], 0, MENHEIGHT , MENWIDTH, MENHEIGHT,
        ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
        0, (APTR)&menuIText[1], NULL, NULL, NULL, NULL
    },
    {   /* Print */
        &menu1[3], 0, 2*MENHEIGHT , MENWIDTH, MENHEIGHT,
        ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
        0, (APTR)&menuIText[2], NULL, NULL, &submenu1[0] , NULL
    },
    {   /* Quit */
        NULL, 0, 3*MENHEIGHT , MENWIDTH, MENHEIGHT,
        ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
        0, (APTR)&menuIText[5], NULL, NULL, NULL, NULL
    },
};

/* We only use a single menu, but the code is generalizable to */
/* more than one menu.                                         */
#define NUM_MENUS 1

STRPTR menutitle[NUM_MENUS] = { "Project" };

struct Menu menustrip[NUM_MENUS] =
{
    {
        NULL,           /* Next Menu            */
        0, 0,           /* LeftEdge, TopEdge,   */
        0, MENHEIGHT,   /* Width, Height,       */
        MENUENABLED,    /* Flags                */
        NULL,           /* Title                */
        &menu1[0]       /* First item           */
    }
};

struct NewWindow mynewWindow =
{
    40,40, 300,100, 0,1, IDCMP_CLOSEWINDOW | IDCMP_MENUPICK,
    WFLG_DRAGBAR | WFLG_ACTIVATE | WFLG_CLOSEGADGET, NULL,NULL,
    "Menu Test Window", NULL,NULL,0,0,0,0,WBENCHSCREEN
};

/* our function prototypes */
VOID handleWindow(struct Window *win, struct Menu *menuStrip);

/* Main routine. */
VOID main(int argc, char **argv)
{
struct Window *win=NULL;
UWORD left, m;

    /* Open the Graphics Library */
    GfxBase = OpenLibrary("graphics.library",37);
    if (GfxBase)
    {
        /* Open the Intuition Library */
        IntuitionBase = OpenLibrary("intuition.library", 37);
        if (IntuitionBase)
        {
            if ( win = OpenWindow(&mynewWindow) )
            {
                left = 2;
                for (m = 0; m < NUM_MENUS; m++)
                {
                    menustrip[m].LeftEdge = left;
                    menustrip[m].MenuName = menutitle[m];
                    menustrip[m].Width = TextLength(&win->WScreen->RastPort,
                        menutitle[m], strlen(menutitle[m])) + 8;
                    left += menustrip[m].Width;
                }

                if (SetMenuStrip(win, menustrip))
                {
                    handleWindow(win, menustrip);
                    ClearMenuStrip(win);
                }
                CloseWindow(win);
            }
            CloseLibrary(IntuitionBase);
        }
        CloseLibrary(GfxBase);
    }
}

/*
** Wait for the user to select the close gadget.
*/
VOID handleWindow(struct Window *win, struct Menu *menuStrip)
{
struct IntuiMessage *msg;
SHORT done;
ULONG class;
UWORD menuNumber;
UWORD menuNum;
UWORD itemNum;
UWORD subNum;
struct MenuItem *item;

    done = FALSE;
    while (FALSE == done)
    {
        /* we only have one signal bit, so we do not have to check which
        ** bit broke the Wait().
        */
        Wait(1L << win->UserPort->mp_SigBit);

        while ( (FALSE == done) &&
                (msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
        {
            class = msg->Class;
            if(class == IDCMP_MENUPICK) menuNumber = msg->Code;

            switch (class)
            {
                case IDCMP_CLOSEWINDOW:
                    done = TRUE;
                    break;

                case IDCMP_MENUPICK:
                    while ((menuNumber != MENUNULL) && (!done))
                    {
                        item = ItemAddress(menuStrip, menuNumber);

                        /* process this item
                        ** if there were no sub-items attached to that item,
                        ** SubNumber will equal NOSUB.
                        */
                        menuNum = MENUNUM(menuNumber);
                        itemNum = ITEMNUM(menuNumber);
                        subNum  = SUBNUM(menuNumber);

                        /* Note that we are printing all values, even things
                        ** like NOMENU, NOITEM and NOSUB.  An application should
                        ** check for these cases.
                        */
                        printf("IDCMP_MENUPICK: menu %d, item %d, sub %d\n",
                               menuNum, itemNum, subNum);

                        /* This one is the quit menu selection...
                        ** stop if we get it, and don't process any more.
                        */
                        if ((menuNum == 0) && (itemNum == 4))
                            done = TRUE;

                        menuNumber = item->NextSelect;
                    }
                    break;
            }
            ReplyMsg((struct Message *)msg);
        }
    }
}
