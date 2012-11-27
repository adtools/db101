/*breakpointswindow.c*/


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
//#include <proto/asl.h>
#include <proto/elf.h>
#include <proto/space.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>
#include <gadgets/space.h>
#include <gadgets/string.h>

#include <reaction/reaction_macros.h>

//#include <libraries/asl.h>
//#include <interfaces/asl.h>

#include <libraries/elf.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "suspend.h"
#include "stabs.h"
#include "breakpoints.h"
#include "breakpointswindow.h"
#include "symbols.h"

enum
{
    BP_DONE_BUTTON = 1,
    BP_SYMBOLS_LISTBROWSER,
    BP_BREAKPOINTS_LISTBROWSER,
    BP_SYMBOL_ADD_BUTTON,
    BP_ADDRESS_ADD_BUTTON,
    BP_ADDRESS_STRING,
    BP_REMOVE_BUTTON
};

Object *BPWinObj;
Object *BPDoneButtonObj, *BPSymbolsListBrowserObj, *BPBreakpointsListBrowserObj;
Object *BPSymbolAddButtonObj, *BPAddressAddButtonObj, *BPAddressStringObj, *BPRemoveButtonObj;
struct Window *breakpointswin = NULL;

struct List bp_breakpointslist;
struct List bp_symbolslist;

struct ColumnInfo *breakpointscolumninfo = NULL;
char bp_address_buffer[10] = "";


void breakpoints_makebplist()
{
	IIntuition->SetAttrs(BPBreakpointsListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
	
    if (!IsListEmpty(&bp_breakpointslist))
        IListBrowser->FreeListBrowserList(&bp_breakpointslist);
	IExec->NewList(&bp_breakpointslist);

    IExec->NewList (&bp_breakpointslist);
	struct breakpoint *br = (struct breakpoint *)IExec->GetHead(&breakpoint_list);
	int i = 0;
	while(br)
	{
		struct Node *node;
		char buf[1024];
		switch(br->type)
		{
			case BR_NORMAL_FUNCTION:
				sprintf(buf, "break %d: (function) %s (line %d)", i, br->function->name, br->line);
				break;
			case BR_NORMAL_SYMBOL:
				sprintf(buf, "break %d: (symbol) %s", i, br->symbol->name);
				break;
			case BR_NORMAL_ABSOLUTE:
				sprintf(buf, "break %d: (absolute) 0x%x", i, br->address);
				break;
			default:
				continue;
		}
        if (node = IListBrowser->AllocListBrowserNode(1,
        										LBNA_UserData, br,
                                                LBNA_Column, 0,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, buf,
                                                TAG_DONE))
                                    {
                                        IExec->AddTail(&bp_breakpointslist, node);
                                    }
		br = (struct breakpoint *)IExec->GetSucc((struct Node *)br);
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)BPBreakpointsListBrowserObj, breakpointswin, NULL,
												LISTBROWSER_Labels, &bp_breakpointslist,
												TAG_END);
}

void breakpoints_makesymbollist()
{
	IIntuition->SetAttrs(BPSymbolsListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
	
    if (!IsListEmpty(&bp_symbolslist))
        IListBrowser->FreeListBrowserList(&bp_symbolslist);
	IExec->NewList(&bp_symbolslist);

	struct amigaos_symbol *sym = (struct amigaos_symbol *)IExec->GetHead(&symbols_list);
	while(sym)
	{
		struct Node *node;
        if (node = IListBrowser->AllocListBrowserNode(1,
        										LBNA_UserData, sym,
                                                LBNA_Column, 0,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, sym->name,
                                                TAG_DONE))
                                    {
                                        IExec->AddTail(&bp_symbolslist, node);
                                    }
		sym = (struct amigaos_symbol *)IExec->GetSucc((struct Node *)sym);
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)BPSymbolsListBrowserObj, breakpointswin, NULL,
												LISTBROWSER_Labels, &bp_symbolslist,
												TAG_END);
}

#define SPACE LAYOUT_AddChild, SpaceObject, End

void breakpoints_open_window()
{
    if (breakpointswin)
        return;

	IExec->NewList(&bp_breakpointslist);
	IExec->NewList(&bp_symbolslist);
   
    /* Create the window object. */
    if(( BPWinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Select breakpoint",
            WA_Width, 600,
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
                
                LAYOUT_AddChild, HLayoutObject,
                	LAYOUT_AddChild, BPSymbolsListBrowserObj = ListBrowserObject,
                    	GA_ID,                     BP_SYMBOLS_LISTBROWSER,
                    	GA_RelVerify,              TRUE,
                    	GA_TabCycle,               TRUE,
                    	LISTBROWSER_AutoFit,       TRUE,
                    	LISTBROWSER_Labels,        &bp_symbolslist,
	                    LISTBROWSER_ShowSelected,  TRUE,
    	                LISTBROWSER_Striping,      LBS_ROWS,
	                ListBrowserEnd,
	           		
	           		LAYOUT_AddChild, HLayoutObject,
	           			SPACE,
	           			
		           		LAYOUT_AddChild, VLayoutObject,
		           			SPACE,
	           			
			           		LAYOUT_AddChild, BPSymbolAddButtonObj = ButtonObject,
		    	       			GA_ID, BP_SYMBOL_ADD_BUTTON,
	    	    	            GA_RelVerify, TRUE,
    	    	    	        GA_Text, "Add",
							ButtonEnd,
                			CHILD_WeightedHeight, 0,
                			CHILD_WeightedWidth, 0,
                		
                			SPACE,
                		EndMember,
                		SPACE,
                	EndMember,
               		CHILD_WeightedWidth, 0,

               	  	LAYOUT_AddChild, VLayoutObject,
                		LAYOUT_AddChild, BPBreakpointsListBrowserObj = ListBrowserObject,
                    		GA_ID,                     BP_BREAKPOINTS_LISTBROWSER,
                    		GA_RelVerify,              TRUE,
                    		GA_TabCycle,               TRUE,
                    		LISTBROWSER_AutoFit,       TRUE,
                    		LISTBROWSER_Labels,        &bp_breakpointslist,
                    		LISTBROWSER_ShowSelected,  TRUE,
                    		LISTBROWSER_Striping,      LBS_ROWS,
	                	ListBrowserEnd,
	                	
	                	LAYOUT_AddChild, BPRemoveButtonObj = ButtonObject,
	                		GA_ID, BP_REMOVE_BUTTON,
                    		GA_RelVerify, TRUE,
                    		GA_Text, "Remove",
                		ButtonEnd,
                		CHILD_WeightedHeight, 0,
	                	ButtonEnd,
                EndMember,
                
                LAYOUT_AddChild, HLayoutObject,
                	LAYOUT_AddChild, BPAddressStringObj = StringObject,
						GA_ID, BP_ADDRESS_STRING,
						GA_RelVerify, TRUE,
						STRINGA_MinVisible,	9,
						STRINGA_MaxChars,	9,
						STRINGA_Buffer,		bp_address_buffer,
						STRINGA_HookType,	SHK_HEXADECIMAL,
                	StringEnd,
					CHILD_WeightedHeight, 0,
					CHILD_Label, LabelObject, LABEL_Text, "Address:", LabelEnd,

                	LAYOUT_AddChild, BPRemoveButtonObj = ButtonObject,
	                	GA_ID, BP_ADDRESS_ADD_BUTTON,
                    	GA_RelVerify, TRUE,
                    	GA_Text, "Add",
                	ButtonEnd,
                	CHILD_WeightedHeight, 0,
                	
	                LAYOUT_AddChild, BPDoneButtonObj = ButtonObject,
    	                GA_ID, BP_DONE_BUTTON,
        	            GA_RelVerify, TRUE,
            	        GA_Text, "Done",
                	ButtonEnd,
                	CHILD_WeightedHeight, 0,
                EndMember,
                CHILD_WeightedHeight, 0,
            EndGroup,
        EndWindow))
    {
        /*  Open the window. */
        breakpointswin = (struct Window *) RA_OpenWindow(BPWinObj);

		breakpoints_makebplist();
		breakpoints_makesymbollist();
    }
}


uint32 breakpoints_obtain_window_signal()
{
    uint32 signal = 0x0;
    if (breakpointswin)
        IIntuition->GetAttr( WINDOW_SigMask, BPWinObj, &signal );
        
    return signal;
}



void breakpoints_event_handler()
{
    ULONG wait, signal, result, relevent;
    BOOL done = FALSE;
    WORD Code;
    CONST_STRPTR hintinfo;
    struct stab_function *f;

    /* Input Event Loop */
    while ((result = RA_HandleInput(BPWinObj, &Code)) != WMHI_LASTMSG)
    {
        switch (result & WMHI_CLASSMASK)
        {
            case WMHI_CLOSEWINDOW:
                done = TRUE;
                break;
             case WMHI_GADGETUP:
                switch(result & WMHI_GADGETMASK)
                {
					case BP_SYMBOL_ADD_BUTTON:
					{
						struct Node *node;
						struct amigaos_symbol *s;
						IIntuition->GetAttr(LISTBROWSER_SelectedNode, BPSymbolsListBrowserObj, (ULONG *) &node);
						if(!node)
							return;
						IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &s, TAG_DONE);
						if(s)
						{
							insert_breakpoint(s->value, BR_NORMAL_SYMBOL, (APTR)s, 0);
							breakpoints_makebplist();
						}
					}
					break;
						
					case BP_ADDRESS_ADD_BUTTON:
					case BP_ADDRESS_STRING:
					{
						uint32 addr;
						sscanf(bp_address_buffer, "%x", &addr);
						if(addr)
						{
							insert_breakpoint(addr, BR_NORMAL_ABSOLUTE, NULL, 0);
							breakpoints_makebplist();
						}
					}
					break;
					
					case BP_REMOVE_BUTTON:
					{
						struct Node *node;
						IIntuition->GetAttr(LISTBROWSER_SelectedNode, BPBreakpointsListBrowserObj, (ULONG *) &node);
						if(node)
						{
							struct breakpoint *br;
							IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &br, TAG_DONE);
							if(br)
							{
								remove_breakpoint_node(br);
								breakpoints_makebplist();
							}
						}
					}
					break;
											
					case BP_DONE_BUTTON:
	                    done = TRUE;
    	                break;
                }
                break;
        }
        
        if( done )
        {
            breakpoints_close_window();
        }
    }
}

void breakpoints_close_window()
{
    if (breakpointswin)
    {
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( BPWinObj );
	    if (!IsListEmpty(&bp_breakpointslist))
    	    IListBrowser->FreeListBrowserList(&bp_breakpointslist);
	    if (!IsListEmpty(&bp_symbolslist))
    	    IListBrowser->FreeListBrowserList(&bp_symbolslist);

        breakpointswin = NULL;
    }
}

