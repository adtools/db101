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

enum
{
	SYMBOL_BUTTON = 1,
	SYMBOL_LISTBROWSER
};


int nosymbols = 0;
char *symlist[1024];
uint32 symval[1024];


static ULONG amigaos_symbol_callback(struct Hook *, struct Task *, struct SymbolMsg *);


struct Hook symbol_hook;

ULONG amigaos_symbol_callback(struct Hook *hook, struct Task *task, struct SymbolMsg *symbolmsg)
{
	if (symbolmsg->Name)
	{
		char *storage = IExec->AllocMem(1024, MEMF_ANY|MEMF_CLEAR);
		if (!storage)
			printf("AllocMem failed!\n");
		strcpy (storage, symbolmsg->Name);
		symlist[nosymbols] = storage;
		symval[nosymbols] = symbolmsg->AbsValue;
		nosymbols++;
	}
	return 1;
}

Elf32_Handle open_elfhandle()
{
	BPTR exec_seglist = IDOS->GetProcSegList (process, GPSLF_SEG);

	if (!exec_seglist)
	{
		printf("Couldn't get seglist!\n");
		return (0);
	}

    IDOS->GetSegListInfoTags(exec_seglist, 
							 GSLI_ElfHandle,      &exec_elfhandle,
							 TAG_DONE);

	if (exec_elfhandle == NULL)
	{
		printf ("not a PowerPC executable\n");
		return (0);
    }
 
    exec_elfhandle = IElf->OpenElfTags(OET_ElfHandle, exec_elfhandle, TAG_DONE);

	if (exec_elfhandle == NULL)
	{
		printf ("couldn't open elfhandle\n");
		return (0);
    }
	return exec_elfhandle;
}

void close_elfhandle (Elf32_Handle handle)
{
	IElf->CloseElfTags (exec_elfhandle, CET_CloseInput, TRUE, TAG_DONE);
}


void get_symbols ()
{
    symbol_hook.h_Entry = (ULONG (*)())amigaos_symbol_callback;
    symbol_hook.h_Data =  NULL;

	//printf("calling ScanSymbolTable...\n");
	IElf->ScanSymbolTable (exec_elfhandle, &symbol_hook, NULL);
	//printf("done!\n");
}

void free_symbols ()
{
	int i;
	for (i = 0; i < nosymbols; i++)
		IExec->FreeMem (symlist[i], 1024);
	nosymbols = 0;
}

void list_symbols ()
{
	int i;
	for ( i = 0; i < nosymbols; i++)
		printf (" \"%s\" = 0x%x\n", symlist[i], symval[i]);
}

uint32 get_symval_from_name (char *name)
{
	int i;
	for (i= 0; i < nosymbols; i++)
		if(!strcmp(symlist[i], name))
			return symval[i];
	return -1;
}

struct stab_function * select_symbol()
{
	struct stab_function * ret = NULL;

    Object *WinObj, *ButtonObj, *ListBrowserObj;
    struct Window *win;

	//Elf32_Handle elfhandle = open_elfhandle();
	//get_symbols(exec_elfhandle);
	//close_elfhandle(exec_elfhandle);

	stabs_make_function_list();

	struct List symbollist;
	struct Node *node;
	IExec->NewList (&symbollist);
	int i;
	for (i = 0; i < numberoffunctions; i++)
	{
		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, function_names[i],
            									TAG_DONE))
        							{
							            IExec->AddTail(&symbollist, node);
									}
	}

	//free_symbols();

    /* Create the window object. */
    if(( WinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "SelectSymbol",
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

	            LAYOUT_AddChild, ListBrowserObj = ListBrowserObject,
    	            GA_ID,                     SYMBOL_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &symbollist,
//                    LISTBROWSER_ColumnInfo,    columninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

                LAYOUT_AddChild, ButtonObj = ButtonObject,
                    GA_ID, SYMBOL_BUTTON,
					GA_RelVerify, TRUE,
                    GA_Text, "Select",
                ButtonEnd,
                CHILD_WeightedHeight, 0,
			EndMember,
        EndWindow))
    {
        /*  Open the window. */
        if( win = (struct Window *) RA_OpenWindow(WinObj) )
        {   
            ULONG wait, signal, result, done = FALSE;
            WORD Code;
            CONST_STRPTR hintinfo;
			uint32 selected;
            
            /* Obtain the window wait signal mask. */
            IIntuition->GetAttr( WINDOW_SigMask, WinObj, &signal );
            
            /* Input Event Loop */
            while( !done )
            {
                wait = IExec->Wait(signal|SIGBREAKF_CTRL_C);
                
                if (wait & SIGBREAKF_CTRL_C) done = TRUE;
                else
                while ((result = RA_HandleInput(WinObj, &Code)) != WMHI_LASTMSG)
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
                                    /* if the user has entered a new text for the buttons
                                    ** help text, get it, and set it
                                    */
                                    IIntuition->GetAttrs( ListBrowserObj, LISTBROWSER_Selected, &selected, TAG_DONE );
									//printf("selected = 0x%x\n", selected);

									if (selected != 0xffffffff)
									{
										//ret = stabs_interpret_function (symlist[selected], symval[selected] );
										ret = stabs_interpret_function (function_names[selected],
																		function_addresses[selected]);

										if (! ret )
											printf("Error interpreting symbol (not a function)\n");
									}

									done = TRUE;
                                    break;
                            }
                            break;
                    }
                }
            }
        }
        
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( WinObj );
    }
    
    return ret;

}

