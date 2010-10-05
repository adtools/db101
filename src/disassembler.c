/*disassembler.c*/


#include <exec/types.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/button.h>
#include <proto/string.h>
#include <proto/layout.h>
#include <proto/label.h>
#include <proto/listbrowser.h>
#include <proto/asl.h>
#include <proto/elf.h>


#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

#include <libraries/asl.h>
#include <interfaces/asl.h>

#include <libraries/elf.h>

#include <graphics/text.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "suspend.h"
#include "stabs.h"
#include "gui.h"
#include "disassembler.h"

enum
{
	DISASSEMBLER_STEP_BUTTON = 1,
	DISASSEMBLER_SKIP_BUTTON,
	DISASSEMBLER_LISTBROWSER
};


Object *DisassemblerWinObj, *DisassemblerStepButtonObj, *DisassemblerSkipButtonObj, *DisassemblerListBrowserObj;
struct Window *disassemblerwin;
BOOL disassembler_window_is_open = FALSE;

int32 disassembler_selected = 0;

struct List assemblelist;

int noinstructions = 0;
char opcodelist[1024][15];
char operandlist[1024][40];

extern struct stab_function *current_function;
extern struct DebugIFace *IDebug;

#define MAX(a, b)  (a>b?a:b)

void disassembler_show_selected()
{
	if (disassembler_window_is_open && disassembler_selected >= 0)
	{
		int32 top = MAX(disassembler_selected - 3, 0);
		IIntuition->SetGadgetAttrs((struct Gadget *)DisassemblerListBrowserObj, disassemblerwin, NULL,
												LISTBROWSER_Selected, disassembler_selected,
												LISTBROWSER_Top, top,
												TAG_DONE);
	}
}

void disassembler_makelist()
{
	struct Node *node;
	IExec->NewList (&assemblelist);
	int i;
	APTR address = (APTR) current_function->address;

	for (i = 0; i < 1024; i++)
	{
		char *str = IExec->AllocMem (50, MEMF_ANY|MEMF_CLEAR);

		APTR newaddress = IDebug->DisassembleNative (address, opcodelist[i], operandlist[i]);

		sprintf(str, "0x%x (0x%x): %s %s", address, address-current_function->address, opcodelist[i], operandlist[i]);

		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&assemblelist, node);
									}
		if (address == (APTR)context_copy.ip)
			disassembler_selected = i;

		address = newaddress;

		if ((uint32) (address - current_function->address) == current_function->size)
			break;
	}
	if (disassembler_window_is_open)
	{
		IIntuition->RefreshGadgets ((struct Gadget *)DisassemblerListBrowserObj, disassemblerwin, NULL);
		disassembler_show_selected();
	}
}



void disassembler_open_window()
{
	if (disassembler_window_is_open)
		return;

	if(!hasfunctioncontext)
		return;

	disassembler_makelist();

    /* Create the window object. */
    if(( DisassemblerWinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, current_function->name,
            WA_Width, 400,
			WA_Height, 400,
            WA_DepthGadget, TRUE,
			WA_SizeGadget, TRUE,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_Activate, TRUE,
            WINDOW_Position, WPOS_CENTERSCREEN,
            /* there is no HintInfo array set up here, instead we define the 
            ** strings directly when each gadget is created (OM_NEW)
            */
            WINDOW_GadgetHelp, TRUE,
            WINDOW_ParentGroup, VLayoutObject,
                LAYOUT_SpaceOuter, TRUE,
                LAYOUT_DeferLayout, TRUE,

	            LAYOUT_AddChild, DisassemblerListBrowserObj = ListBrowserObject,
    	            GA_ID,                     DISASSEMBLER_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
//					GA_TextAttr,			   &courier_font,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &assemblelist,
//                    LISTBROWSER_ColumnInfo,    columninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

				LAYOUT_AddChild, HLayoutObject,
	                LAYOUT_AddChild, DisassemblerStepButtonObj = ButtonObject,
    	                GA_ID, DISASSEMBLER_STEP_BUTTON,
						GA_RelVerify, TRUE,
            	        GA_Text, "Step",
                	ButtonEnd,
                	CHILD_WeightedHeight, 0,

	                LAYOUT_AddChild, DisassemblerSkipButtonObj = ButtonObject,
    	                GA_ID, DISASSEMBLER_SKIP_BUTTON,
						GA_RelVerify, TRUE,
            	        GA_Text, "Skip",
                	ButtonEnd,
                	CHILD_WeightedHeight, 0,
				EndMember,
                CHILD_WeightedHeight, 0,
			EndMember,
        EndWindow))
    {
        /*  Open the window. */
        if( disassemblerwin = (struct Window *) RA_OpenWindow(DisassemblerWinObj) )
		{
        	disassembler_window_is_open = TRUE;
			disassembler_show_selected();
		}
	}
}

uint32 disassembler_obtain_window_signal()
{
	uint32 signal = 0x0;
	if (disassembler_window_is_open)
		IIntuition->GetAttr( WINDOW_SigMask, DisassemblerWinObj, &signal );

	return signal;
}


void disassembler_event_handler()
{

            ULONG wait, signal, result, done = FALSE;
            WORD Code;
            CONST_STRPTR hintinfo;
			uint32 selected;
            
            /* Obtain the window wait signal mask. */
            //IIntuition->GetAttr( WINDOW_SigMask, WinObj, &signal );
            
            /* Input Event Loop */
                while ((result = RA_HandleInput(DisassemblerWinObj, &Code)) != WMHI_LASTMSG)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;
                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {
                                case DISASSEMBLER_STEP_BUTTON:

									asmstep();
                                    break;

								case DISASSEMBLER_SKIP_BUTTON:

									asmskip();
									break;

								case DISASSEMBLER_LISTBROWSER:

									disassembler_show_selected();
									break;
                            }
                            break;
                    }
					if (done)
					{
						disassembler_close_window();
						break;
					}
                }
}

void disassembler_close_window()
{
	if (disassembler_window_is_open)
	{
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( DisassemblerWinObj );

		disassembler_window_is_open = FALSE;
    }
    
    return;
}

