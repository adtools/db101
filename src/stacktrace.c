/*stacktrace.c*/


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
#include "stacktrace.h"
#include "freemem.h"

enum
{
	STACKTRACE_BUTTON = 1,
	STACKTRACE_LISTBROWSER
};


Object *StacktraceWinObj, *StacktraceButtonObj, *StacktraceListBrowserObj;
struct Window *stacktracewin;
BOOL stacktrace_window_is_open = FALSE;

struct List stacktracelist;

extern struct DebugIFace *IDebug;

#define MAX(a, b)  (a>b?a:b)

struct List stack_freelist;
BOOL has_stack_freelist = FALSE;


void stacktrace_update()
{
	if (!stacktrace_window_is_open)
		return;

	if (!task_exists)
		return;

	if (has_stack_freelist)
		stacktrace_freelist();

	IExec->NewList (&stacktracelist);

	struct Task *t = (struct Task *)process;
	struct Node *node;
	uint32 stacksize = process->pr_StackSize;
	uint32 stackupper = (uint32)t->tc_SPUpper;
	uint32 stackpointer = (uint32)t->tc_SPReg;

	uint32 p = stackpointer;

	int i = 0;
	while (p < stackupper-28)
	{
		i++;
		char *str = (char*)IExec->AllocMem (1024, MEMF_ANY|MEMF_CLEAR);
		add_freelist(&stack_freelist, 1024, str);

		//first try the stabs functions:
		struct stab_function *f = stabs_get_function_from_address (*(uint32*)(p+4));
		char *fname = NULL;
		if (f && f->name)
			fname = f->name;
		if (!fname)
		{
			//try the symbol table:

		}
		if (!fname)
		{
			//try to find the section:
		}
		//if everything fails:
		if (!fname)
			fname = "<unknown function>";

		sprintf(str, "0x%x (%d): %s", p-stackpointer, i, fname);

		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&stacktracelist, node);
									}
		p = *(uint32*)p;
	}

	has_stack_freelist = TRUE;

	if (stacktrace_window_is_open)
		IIntuition->RefreshGadgets ((struct Gadget *)StacktraceListBrowserObj, stacktracewin, NULL);
}

void stacktrace_freelist()
{
	if (has_stack_freelist)
	{
		freelist(&stack_freelist);
		has_stack_freelist = FALSE;
	}
}


void stacktrace_open_window()
{
	if (stacktrace_window_is_open)
		return;

	IExec->NewList (&stack_freelist);
	IExec->NewList (&stacktracelist);

	pause();

    /* Create the window object. */
    if(( StacktraceWinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Stacktrace",
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

	            LAYOUT_AddChild, StacktraceListBrowserObj = ListBrowserObject,
    	            GA_ID,                     STACKTRACE_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
//					GA_TextAttr,			   &courier_font,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &stacktracelist,
//                    LISTBROWSER_ColumnInfo,    columninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

				LAYOUT_AddChild, HLayoutObject,
	                LAYOUT_AddChild, StacktraceButtonObj = ButtonObject,
    	                GA_ID, STACKTRACE_BUTTON,
						GA_RelVerify, TRUE,
            	        GA_Text, "Done",
                	ButtonEnd,
                	CHILD_WeightedHeight, 0,
				EndMember,
                CHILD_WeightedHeight, 0,
			EndMember,
        EndWindow))
    {
        /*  Open the window. */
        if( stacktracewin = (struct Window *) RA_OpenWindow(StacktraceWinObj) )
		{
        	stacktrace_window_is_open = TRUE;
			stacktrace_update();
		}
	}
}

uint32 stacktrace_obtain_window_signal()
{
	uint32 signal = 0x0;
	if (stacktrace_window_is_open)
		IIntuition->GetAttr( WINDOW_SigMask, StacktraceWinObj, &signal );

	return signal;
}


void stacktrace_event_handler()
{

            ULONG wait, signal, result, done = FALSE;
            WORD Code;
            CONST_STRPTR hintinfo;
			uint32 selected;
            
            /* Obtain the window wait signal mask. */
            //IIntuition->GetAttr( WINDOW_SigMask, WinObj, &signal );
            
            /* Input Event Loop */
                while ((result = RA_HandleInput(StacktraceWinObj, &Code)) != WMHI_LASTMSG)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;
                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {
                                case STACKTRACE_BUTTON:

									done = TRUE;
                                    break;
                            }
                            break;
                    }
					if (done)
					{
						stacktrace_close_window();
						break;
					}
                }
}

void stacktrace_close_window()
{
	if (stacktrace_window_is_open)
	{
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( StacktraceWinObj );

		stacktrace_window_is_open = FALSE;
    }
    
    return;
}

