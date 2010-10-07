/* hexview.c */

#include <exec/types.h>
#include <exec/lists.h>

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
#include <proto/chooser.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <gadgets/chooser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

#include <libraries/asl.h>
#include <interfaces/asl.h>

#include <libraries/elf.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "suspend.h"
#include "symbols.h"
#include "registers.h"

enum
{
	REGISTERS_BUTTON = 1,
	REGISTERS_LISTBROWSER
};

Object *RegisterWinObj, *RegisterButtonObj, *RegisterListBrowserObj;

struct Window *registerwin;

BOOL registers_window_is_open = FALSE;


extern struct ExceptionContext *context_ptr;


struct List reglist;
struct ColumnInfo *regcolumninfo = NULL;
char regstr[73][256];
char gprstr[32][256];
char fprstr[32][256];

void add_register( char *reg, char *value)
{
	struct Node *node;
	if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, reg,
												LBNA_Column, 1,
												LBNCA_Text, value,
            									TAG_DONE))
        							{
							            IExec->AddTail(&reglist, node);
									}
}


void registers_update_window()
{
	if (!registers_window_is_open)
		return;

	IExec->NewList (&reglist);

	sprintf(regstr[0], "0x%x", context_copy.msr);
		add_register ("msr", regstr[0]);
	sprintf(regstr[1], "0x%x", context_copy.ip);
		add_register ("ip", regstr[1]);
	sprintf(regstr[2], "0x%x", context_copy.cr);
		add_register ("cr", regstr[2]);
	sprintf(regstr[3], "0x%x", context_copy.xer);
		add_register ("xer", regstr[3]);
	sprintf(regstr[4], "0x%x", context_copy.ctr);
		add_register ("ctr", regstr[4]);
	sprintf(regstr[5], "0x%x", context_copy.lr);
		add_register ("lr", regstr[5]);

	int i;
	for (i = 0; i < 32; i++)
	{
		sprintf(regstr[i+6], "0x%x", context_copy.gpr[i]);
		sprintf(gprstr[i], "gpr%d", i);
			add_register (gprstr[i], regstr[i+6]);
	}
	for (i = 0; i < 32; i++)
	{
		sprintf(regstr[i+38], "%e", context_copy.fpr[i]);
		sprintf(fprstr[i], "fpr%d", i);
			add_register (fprstr[i], regstr[i+38]);
	}

	if (registers_window_is_open)
		IIntuition->RefreshGadgets ((struct Gadget *)RegisterListBrowserObj, registerwin, NULL);
}

void registers_open_window()
{
	if (registers_window_is_open)
		return;

	IExec->NewList (&reglist);

    regcolumninfo = IListBrowser->AllocLBColumnInfo(2,
        LBCIA_Column, 0,
            LBCIA_Title, "Register",
            LBCIA_Width, 80,
        LBCIA_Column, 1,
            LBCIA_Title, "Value",
        TAG_DONE);

    /* Create the window object. */
    if(( RegisterWinObj = WindowObject,
            WA_ScreenTitle, "DEbug 101",
            WA_Title, "Registers",
            WA_Width, 200,
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

	            LAYOUT_AddChild, RegisterListBrowserObj = ListBrowserObject,
    	            GA_ID,                     REGISTERS_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &reglist,
                    LISTBROWSER_ColumnInfo,    regcolumninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

                LAYOUT_AddChild, RegisterButtonObj = ButtonObject,
                    GA_ID, REGISTERS_BUTTON,
					GA_RelVerify, TRUE,
                    GA_Text, "Done",
                ButtonEnd,
                CHILD_WeightedHeight, 0,
			EndMember,
        EndWindow))

      /*  Open the window. */
        if( registerwin = (struct Window *) RA_OpenWindow(RegisterWinObj) )
		{
			registers_window_is_open = TRUE;
			registers_update_window();
		}

	return;
}



uint32 registers_obtain_window_signal()
{
	uint32 signal = 0x0;
	if (registers_window_is_open)
		IIntuition->GetAttr( WINDOW_SigMask, RegisterWinObj, &signal );

	return signal;
}

void registers_event_handler()
{	
            ULONG wait, signal, result, done = FALSE;
            WORD Code;
            CONST_STRPTR hintinfo;
			uint32 selected;
            
            /* Obtain the window wait signal mask. */
            //IIntuition->GetAttr( WINDOW_SigMask, RegisterWinObj, &signal );
			//signal = registers_obtain_window_signal();
            
            /* Input Event Loop */
                while ((result = RA_HandleInput(RegisterWinObj, &Code)) != WMHI_LASTMSG)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;
                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {

                                case REGISTERS_BUTTON:
                                    /* if the user has entered a new text for the buttons
                                    ** help text, get it, and set it
                                    */
									done = TRUE;
                                    break;
                            }
                            break;
                    }
					if (done)
					{
						registers_close_window();
						break;
					}
                }
}

void registers_close_window()
{
	if (registers_window_is_open)
	{
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( RegisterWinObj );
	
		registers_window_is_open = FALSE;
    }
    return;
}
