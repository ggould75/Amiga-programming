#include <exec/types.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pragmas/exec_pragmas.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/graphics_pragmas.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

#define INTUITION_REV 37 // This revision is the latest I can use for my kickstart version (2.05)
#define GRAPHICS_REV 0

int main(int argc, char **argv)
{
        BOOL useGZZ = FALSE;
        BOOL useBorderless = FALSE;
        int paramsI;
                        
        struct NewWindow NewWindow;
        struct Window *Window;
        struct IntuiText IntuiTxt;
        struct IntuiMessage *IntuiMsg;
        char mousePosStr[40];
        char refreshCountStr[17];
        int refreshCount = 0;
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
        
        printf("useGZZ: %d, useBorderless: %d", useGZZ, useBorderless);
        
        IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", INTUITION_REV);
        if (IntuitionBase == NULL)
                exit(FALSE);
        
        GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", GRAPHICS_REV);
        if (GfxBase == NULL)
                exit(FALSE);
                
        NewWindow.LeftEdge = 20;
        NewWindow.TopEdge = 20;
        NewWindow.Width = 300;
        NewWindow.Height = 100;
        NewWindow.DetailPen = 0;
        NewWindow.BlockPen = 1;
        NewWindow.Title = "Mouse events tracking";
        // REPORTMOUSE: (together with MOUSEMOVE in IDCMPFlags) to be notified about mouse moving (even when outside active window)
        // SMART_REFRESH: read Amiga_ROM_Kernel_..._v2.04.pdf (p. 93)
        NewWindow.Flags = REPORTMOUSE | WINDOWCLOSE | SMART_REFRESH | ACTIVATE | WINDOWDRAG | WINDOWDEPTH | WINDOWSIZING;
        NewWindow.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEMOVE;
        NewWindow.Type = WBENCHSCREEN;
        NewWindow.FirstGadget = NULL;
        NewWindow.CheckMark = NULL;
        NewWindow.Screen = NULL;
        NewWindow.BitMap = NULL;
        NewWindow.MinWidth = 100;
        NewWindow.MinHeight = 25;
        NewWindow.MaxWidth = 640;
        NewWindow.MaxHeight = 200;
        
        // GIMMEZEROZERO: will set the 0 coordinate below the titlebar, otherwise y=0 will mean you are drawing on top of it. I didn't understand if something really change about x...
        // BORDERLESS: doesn't draw window's borders
        if (useGZZ)
                NewWindow.Flags |= GIMMEZEROZERO;
        if (useBorderless)
                NewWindow.Flags |= BORDERLESS;
                
        if ((Window = (struct Window *)OpenWindow(&NewWindow)) == NULL)
                exit(FALSE);
                
        IntuiTxt.LeftEdge = 10;
        IntuiTxt.TopEdge = 90;
        IntuiTxt.FrontPen = AUTOFRONTPEN;
        IntuiTxt.BackPen = AUTOBACKPEN;
        IntuiTxt.DrawMode = JAM1; // FrontPen color used
        IntuiTxt.IText = (UBYTE *)"This is a longer string displayed with IntuiText gadget, it should redraw automatically and should not overlap with window's gadgets. Does it?";
        IntuiTxt.ITextFont = NULL; // Use default font
        IntuiTxt.NextText = NULL;
        PrintIText(Window->RPort, &IntuiTxt, 20, 90);
        
        Move(Window->RPort, 10, 20);
        Text(Window->RPort, "Move the mouse inside the window...", 36);
        
        while (!done)
        {
                Wait(1 << Window->UserPort->mp_SigBit);
                
                while ((IntuiMsg = (struct IntuiMessage *)GetMsg(Window->UserPort)))
                {
                        switch (IntuiMsg->Class) {
                        case IDCMP_REFRESHWINDOW:
                                refreshCount += 1;
                                BeginRefresh(Window);
                                sprintf(refreshCountStr, "refreshCount: %d", refreshCount);
                                Move(Window->RPort, 10, 40);
                                Text(Window->RPort, refreshCountStr, strlen(refreshCountStr));
                                //PrintIText(Window->RPort, &IntuiTxt, 20, 90);
                                EndRefresh(Window, TRUE);
                                break;
                        case IDCMP_MOUSEMOVE:
                                if (useGZZ)
                                {
                                        sprintf(mousePosStr, "Mouse %3d, %3d", Window->GZZMouseX, Window->GZZMouseY);
                                }
                                else
                                {
                                        sprintf(mousePosStr, "Mouse %3d, %3d", IntuiMsg->MouseX, IntuiMsg->MouseY);
                                }
                                Move(Window->RPort, 10, 60);
                                Text(Window->RPort, mousePosStr, strlen(mousePosStr));
                                break;
                        case IDCMP_CLOSEWINDOW:
                                done = TRUE;
                                break;
                        }
                        
                        ReplyMsg((struct Message *)IntuiMsg);
                }
        }

        CloseWindow(Window);
        exit(TRUE);
}
