/*symbols.c*/


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

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "suspend.h"
#include "stabs.h"
#include "breakpoints.h"
#include "breakpointswindow.h"

enum
{
	BREAKPOINTS_BUTTON = 1,
	BREAKPOINTS_LISTBROWSER
};

Object *BreakpointsWinObj, *BreakpointsButtonObj, *BreakpointsListBrowserObj;
struct Window *breakpointswin;

struct List breakpointslist;
BOOL breakpoints_window_is_open = FALSE;


struct stab_function *breakpoints_get_selected_function(uint32 selected)
{
	struct stab_function *f = (struct stab_function *)IExec->GetHead(&function_list);
	int i;
	for (i = 0; i < selected; i++)
		f = (struct stab_function *)IExec->GetSucc ((struct Node *)f);

	return f;
}


void breakpoints_makelist()
{
	struct Node *node;
	struct stab_function *f = (struct stab_function *)IExec->GetHead(&function_list);

	IExec->NewList (&breakpointslist);
	while(f)
	{
		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, f->name,
            									TAG_DONE))
        							{
							            IExec->AddTail(&breakpointslist, node);
									}
		f = (struct stab_function *)IExec->GetSucc ((struct Node *)f);
	}
}

void breakpoints_open_window()
{
	if (breakpoints_window_is_open)
		return;

	breakpoints_makelist();

    /* Create the window object. */
    if(( BreakpointsWinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Select breakpoint",
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

	            LAYOUT_AddChild, BreakpointsListBrowserObj = ListBrowserObject,
    	            GA_ID,                     BREAKPOINTS_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &breakpointslist,
//                    LISTBROWSER_ColumnInfo,    columninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

                LAYOUT_AddChild, BreakpointsButtonObj = ButtonObject,
                    GA_ID, BREAKPOINTS_BUTTON,
					GA_RelVerify, TRUE,
                    GA_Text, "Select",
                ButtonEnd,
                CHILD_WeightedHeight, 0,
			EndMember,
        EndWindow))
    {
        /*  Open the window. */
        if( breakpointswin = (struct Window *) RA_OpenWindow(BreakpointsWinObj) )
        {
			breakpoints_window_is_open = TRUE;
		}
	}
}


uint32 breakpoints_obtain_window_signal()
{
	uint32 signal = 0x0;
	if (breakpoints_window_is_open)
		IIntuition->GetAttr( WINDOW_SigMask, BreakpointsWinObj, &signal );

	return signal;
}



void breakpoints_event_handler()
{
            ULONG wait, signal, result, done = FALSE;
            WORD Code;
            CONST_STRPTR hintinfo;
			uint32 selected;
            struct stab_function *f;

            /* Obtain the window wait signal mask. */
            //IIntuition->GetAttr( WINDOW_SigMask, WinObj, &signal );
            
            /* Input Event Loop */

                while ((result = RA_HandleInput(BreakpointsWinObj, &Code)) != WMHI_LASTMSG)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;
                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {
                                case BREAKPOINTS_BUTTON:
                                    /* if the user has entered a new text for the buttons
                                    ** help text, get it, and set it
                                    */
                                    IIntuition->GetAttrs( BreakpointsListBrowserObj,
															LISTBROWSER_Selected, &selected, TAG_DONE );

									if (selected != 0x7fffffff)
									{
										f = breakpoints_get_selected_function(selected);
printf("f->name = %s\n", f->name);
										if (f)
										{
											pause();
											insert_breakpoint (f->address, BR_NORMAL);
										}
									}

									done = TRUE;
                                    break;
                            }
                            break;
                    }
					if (done)
					{
						breakpoints_close_window();
						break;
					}
                }
}

void breakpoints_close_window()
{
	if (breakpoints_window_is_open)
	{
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( BreakpointsWinObj );

		breakpoints_window_is_open = FALSE;
    }
}

