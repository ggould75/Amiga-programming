#include <exec/types.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <stdlib.h>
#include <pragmas/exec_pragmas.h>
#include <pragmas/intuition_pragmas.h>

struct IntuitionBase *IntuitionBase;

#define INTUITION_REV 0
#define MILLION 1000000

main()
{
        struct NewWindow NewWindow;
        struct Window *Window;
        LONG i;
        
        IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", INTUITION_REV);
        if (IntuitionBase == NULL)
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
        NewWindow.Type = WBENCHSCREEN;
        NewWindow.FirstGadget = NULL;
        NewWindow.CheckMark = NULL;
        NewWindow.Screen = NULL;
        NewWindow.BitMap = NULL;
        NewWindow.MinWidth = 100;
        NewWindow.MinHeight = 25;
        NewWindow.MaxWidth = 640;
        NewWindow.MaxHeight = 200;
        
        if ((Window = (struct Window *)OpenWindow(&NewWindow)) == NULL)
                exit(FALSE);
                
        Wait(1 << Window->UserPort->mp_SigBit);
        CloseWindow(Window);
        exit(TRUE);
}
