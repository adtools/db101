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
#include "console.h"
#include "attach.h"

enum
{
	ATTACH_BUTTON = 1,
	ATTACH_LISTBROWSER
};


int noprocesses = 0;
char *processnamelist[1024];
struct Process *processstructlist[1024];


static ULONG amigaos_process_callback(struct Hook *, CONST_APTR, struct Process *);


struct Hook process_hook;

ULONG amigaos_process_callback(struct Hook *hook, CONST_APTR userdata, struct Process *process)
{
	if (process->pr_Task.tc_Node.ln_Name)
	{
		char *storage = IExec->AllocMem(1024, MEMF_ANY|MEMF_CLEAR);
		if (!storage)
			printf("AllocMem failed!\n");
		strcpy (storage, process->pr_Task.tc_Node.ln_Name);
		processnamelist[noprocesses] = storage;
		processstructlist[noprocesses] = process;
		noprocesses++;
	}
	return 0;
}


void get_process_list ()
{
    process_hook.h_Entry = (ULONG (*)())amigaos_process_callback;
    process_hook.h_Data =  NULL;

	//printf("calling ProcessScan...\n");
	IDOS->ProcessScan (&process_hook, NULL, 0);
	//printf("done!\n");
}

void free_process_list ()
{

}

struct Process *attach_select_process()
{
	struct Process * ret = NULL;

    Object *WinObj, *ButtonObj, *ListBrowserObj;
    struct Window *win;

	get_process_list();


	struct List attachlist;
	struct Node *node;
	IExec->NewList (&attachlist);
	int i;
	for (i = 0; i < noprocesses; i++)
	{
		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, processnamelist[i],
            									TAG_DONE))
        							{
							            IExec->AddTail(&attachlist, node);
									}
	}

    /* Create the window object. */
    if(( WinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Attach to process",
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
    	            GA_ID,                     ATTACH_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &attachlist,
//                    LISTBROWSER_ColumnInfo,    columninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

                LAYOUT_AddChild, ButtonObj = ButtonObject,
                    GA_ID, ATTACH_BUTTON,
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
                                case ATTACH_BUTTON:
                                    /* if the user has entered a new text for the buttons
                                    ** help text, get it, and set it
                                    */
                                    IIntuition->GetAttrs( ListBrowserObj, LISTBROWSER_Selected, &selected, TAG_DONE );
									//printf("selected = 0x%x\n", selected);

									if (selected != 0xffffffff)
									{
										ret = processstructlist[selected];
										console_printf(OUTPUT_SYSTEM, "attaching to process with name %s\n", processnamelist[selected]);
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
		IListBrowser->FreeListBrowserList(&attachlist);
    }

	//free_process_list();

    
    return ret;

}

