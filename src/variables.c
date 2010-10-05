/*variables.c*/


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
#include "gui.h"
#include "variables.h"

enum
{
	SYMBOL_BUTTON = 1,
	SYMBOL_LISTBROWSER
};

Object *LocalsWinObj, *LocalsButtonObj, *LocalsListBrowserObj;
struct Window *localswin;
BOOL locals_window_is_open = FALSE;

struct List variablelist;

char *print_variable_value(struct stab_symbol *s)
{
	char *ret = malloc (256);
	uint32 buffer = 0x0;
	uint32 addr = 0x0;

	if (s->type == NULL)
		return "UNKNOWN";

	switch (s->location)
	{
	case L_STACK:
		{
		uint32 stackp = (uint32)process->pr_Task.tc_SPReg;
		uint32 tmpaddr = stackp + s->offset;

		buffer = *(uint32*)tmpaddr;
		addr = (uint32)&buffer;
		break;
		}

	case L_ABSOLUTE:
		{
		addr = s->address;
		break;
		}

	default:
		printf( "Not a known variable type" );
		break;
	}


	if (s->type->ispointer)
	{
		sprintf( ret, "0x%x", *(uint32*)addr);
	}
	else
	{
		switch (s->type->type)
		{
		case T_32:
			{
			int32 value32 = *(int32*)addr;
			sprintf(ret, "%d", value32);
			}
			break;

		case T_U32:
			{
			uint32 valueu32 = *(uint32*)addr;
			sprintf(ret, "%u", valueu32);
			}
			break;

		case T_16:
			{
			int16 value16 = *(int16*)addr;
			int32 convert16 = value16;
			sprintf(ret, "%d", convert16);
			}
			break;

		case T_U16:
			{
			uint16 valueu16 = *(uint16*)addr;
			uint32 convertu16 = valueu16;
			sprintf(ret, "%u", convertu16);
			}
			break;

		case T_8:
			{
			signed char value8 = *(signed char*)addr;
			int32 convert8 = value8;
			sprintf(ret, "%d", convert8);
			}
			break;

		case T_U8:
			{
			char valueu8 = *(char*)addr;
			uint32 convertu8 = valueu8;
			sprintf(ret, "%u (%c)", convertu8, valueu8);
			}
			break;

		case T_FLOAT32:
			{
			float valuef32 = *(float*)addr;
			sprintf(ret, "%e", valuef32);
			}
			break;

		default:

			strcpy (ret, "UNKNOWN");

			break;
		}
	}
	return ret;
}
	
void locals_update_window()
{
	struct Node *node;

	IExec->NewList (&variablelist);

	struct List *l = &(current_function->symbols);
	struct stab_symbol *s = (struct stab_symbol *)IExec->GetHead (l);

	if (s)
	while (1)
	{
		char *str = print_variable_value(s);
		char *namestr = IExec->AllocMem (1024, MEMF_ANY|MEMF_CLEAR);

		if (s->type && s->type->ispointer)
			sprintf(namestr, "*%s", s->name);
		else
			sprintf(namestr, "%s", s->name);

		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, namestr,
												LBNA_Column, 1,
												LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&variablelist, node);
									}
		if ((struct Node *)s == IExec->GetTail(l))
			break;
		s = (struct stab_symbol *)IExec->GetSucc((struct Node *)s);
	}

	if (locals_window_is_open)
		IIntuition->RefreshGadgets ((struct Gadget *)LocalsListBrowserObj, localswin, NULL);
}

void locals_open_window()
{
	if (locals_window_is_open)
		return;

	if (!hasfunctioncontext)
		return;

	locals_update_window();

    struct ColumnInfo *columninfo = IListBrowser->AllocLBColumnInfo(2,
        LBCIA_Column, 0,
            LBCIA_Title, "Variable",
            LBCIA_Width, 80,
        LBCIA_Column, 1,
            LBCIA_Title, "Value",
        TAG_DONE);


    /* Create the window object. */
    if(( LocalsWinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Locals",
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

	            LAYOUT_AddChild, LocalsListBrowserObj = ListBrowserObject,
    	            GA_ID,                     SYMBOL_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &variablelist,
                    LISTBROWSER_ColumnInfo,    columninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

                LAYOUT_AddChild, LocalsButtonObj = ButtonObject,
                    GA_ID, SYMBOL_BUTTON,
					GA_RelVerify, TRUE,
                    GA_Text, "Done",
                ButtonEnd,
                CHILD_WeightedHeight, 0,
			EndMember,
        EndWindow))
    {
        /*  Open the window. */
        if( localswin = (struct Window *) RA_OpenWindow(LocalsWinObj) )
        	locals_window_is_open = TRUE;
	}
}

uint32 locals_obtain_window_signal()
{
	uint32 signal = 0x0;
	if (locals_window_is_open)
		IIntuition->GetAttr( WINDOW_SigMask, LocalsWinObj, &signal );

	return signal;
}


void locals_event_handler()
{
            ULONG wait, signal, result, done = FALSE;
            WORD Code;
            CONST_STRPTR hintinfo;
			uint32 selected;
            
            /* Obtain the window wait signal mask. */
            //IIntuition->GetAttr( WINDOW_SigMask, WinObj, &signal );
            

                while ((result = RA_HandleInput(LocalsWinObj, &Code)) != WMHI_LASTMSG)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;
                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {
                                case SYMBOL_BUTTON:

									done = TRUE;
                                    break;
                            }
                            break;
                    }
					if (done)
					{
						locals_close_window();
						break;
					}
                }
}

void locals_close_window()
{
	if (locals_window_is_open)
	{
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( LocalsWinObj );

		locals_window_is_open = FALSE;
    }
    
    return;

}

