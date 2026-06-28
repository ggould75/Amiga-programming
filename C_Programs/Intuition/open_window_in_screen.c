#include <exec/types.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <stdlib.h>
#include <pragmas/exec_pragmas.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/graphics_pragmas.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

#define INTUITION_REV 0
#define GRAPHICS_REV 0

main()
{
        struct TextAttr MyFont =
        {
                "topaz.font",
                TOPAZ_SIXTY,    // Font height
                FS_NORMAL,      // Style
                FPF_ROMFONT,    // Preferences
        };
        
        struct NewScreen NewScreen =
        {
                0,
                0,
                320,
                200,
                2,      // Depth (4 colors)
                0, 1,   // DetailPen, BlockPen
                NULL,
                CUSTOMSCREEN,
                NULL,
                "My Own Screen",
                NULL,
                NULL,
        };
        
        struct NewWindow NewWindow;
        struct Window *Window;
        struct Screen *Screen;
        
        IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", INTUITION_REV);
        if (IntuitionBase == NULL)
                exit(FALSE);
        
        GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", GRAPHICS_REV);
        if (GfxBase == NULL)
                exit(FALSE);
        
        NewScreen.Font = &MyFont; // Couldn't compile when defined inline in the above struct
        
        if ((Screen = (struct Screen *)OpenScreen(&NewScreen)) == NULL)
                exit(FALSE);
        
        NewWindow.LeftEdge = 20;
        NewWindow.TopEdge = 20;
        NewWindow.Width = 300;
        NewWindow.Height = 100;
        NewWindow.DetailPen = 0;
        NewWindow.BlockPen = 1;
        NewWindow.Title = "A Simple Window";
        // NOCAREREFRESH means that if window is resized Intuition will take care of refreshing the window content for you
        NewWindow.Flags = WINDOWCLOSE | SMART_REFRESH | ACTIVATE | WINDOWDRAG | WINDOWDEPTH | WINDOWSIZING | NOCAREREFRESH;
        NewWindow.IDCMPFlags = CLOSEWINDOW;
        NewWindow.Type = CUSTOMSCREEN;  // Instead of opening on Workbench
        NewWindow.FirstGadget = NULL;
        NewWindow.CheckMark = NULL;
        NewWindow.Screen = Screen;      // Pass the screen specs created above
        NewWindow.BitMap = NULL;
        NewWindow.MinWidth = 100;
        NewWindow.MinHeight = 25;
        NewWindow.MaxWidth = 640;
        NewWindow.MaxHeight = 200;
        
        if ((Window = (struct Window *)OpenWindow(&NewWindow)) == NULL)
                exit(FALSE);
        
        Move(Window->RPort, 20, 20);
        Text(Window->RPort, "Hello World", 11);
                
        Wait(1 << Window->UserPort->mp_SigBit);
        CloseWindow(Window);
        CloseScreen(Screen);
        exit(TRUE);
}
