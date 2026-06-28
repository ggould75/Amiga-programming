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
        NewWindow.Flags = SMART_REFRESH | ACTIVATE;
        NewWindow.IDCMPFlags = NULL;
        NewWindow.Type = WBENCHSCREEN;
        NewWindow.FirstGadget = NULL;
        NewWindow.CheckMark = NULL;
        NewWindow.Screen = NULL;
        NewWindow.BitMap = NULL;
        NewWindow.MinWidth = 0;
        NewWindow.MinHeight = 0;
        NewWindow.MaxWidth = 0;
        NewWindow.MaxHeight = 0;
        
        if ((Window = (struct Window *)OpenWindow(&NewWindow)) == NULL)
                exit(FALSE);
                
        for (i=0; i<MILLION; i++);
        
        CloseWindow(Window);
}
