/* gui for db101 */

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
#include <proto/requester.h>

#include <classes/window.h>
#include <classes/requester.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

#include <libraries/asl.h>
#include <interfaces/asl.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "suspend.h"
#include "stabs.h"
#include "symbols.h"
#include "variables.h"
#include "gui.h"
#include "disassembler.h"
#include "breakpoints.h"
#include "stacktrace.h"

enum
{
    GAD_FILENAME_BUTTON = 1,
	GAD_START_BUTTON,
	GAD_PAUSE_BUTTON,
	GAD_STEP_BUTTON,
	GAD_KILL_BUTTON,
	GAD_CRASH_BUTTON,
	GAD_SETBREAK_BUTTON,
	GAD_X_BUTTON,
	GAD_HEX_BUTTON,
	GAD_VARIABLES_BUTTON,
	GAD_REGISTERS_BUTTON,
	GAD_DISASSEMBLE_BUTTON,
	GAD_STACKTRACE_BUTTON,
	GAD_GLOBALS_BUTTON,
	GAD_LISTBROWSER,
	GAD_TEXT
};

struct List listbrowser_list;
struct ColumnInfo *columninfo = NULL;
struct Window *mainwin;

Object *MainWinObj;
BOOL main_window_is_open = FALSE;

Object *SelectButtonObj, *FilenameStringObj, *ListBrowserObj;
Object *StartButtonObj, *PauseButtonObj, *StepButtonObj, *KillButtonObj, *CrashButtonObj, *SetBreakButtonObj, *XButtonObj;
Object *HexviewButtonObj, *VariablesButtonObj, *RegisterviewButtonObj, *DisassembleviewButtonObj, *StacktraceButtonObj;
Object *GlobalsButtonObj;

char lastdir[1024] = "";
char filename[1024] = "";
char childpath[1024] = "";

struct stab_function *current_function = NULL;
BOOL hasfunctioncontext = FALSE;


char *request_file (struct Window *win, char **path)
{
	struct FileRequester *req = IAsl->AllocAslRequestTags(ASL_FileRequest,
													  ASLFR_Window, win,
													  ASLFR_TitleText, "Hello world!",
													  ASLFR_InitialDrawer, lastdir,
													  TAG_DONE);
	char *ret;
	if (!req)
	{
		printf("Couldn't allocate requester!\n");	
		return NULL;
	}
	if (!(IAsl->AslRequest (req, NULL)))
	{
		printf("User aborted!\n");
		return NULL;
	}

	ret = (char *)IExec->AllocMem (1024, MEMF_CLEAR|MEMF_ANY);
	*path = (char *)IExec->AllocMem (1024, MEMF_ANY|MEMF_CLEAR);

//	if (strlen(req->fr_Drawer) > 0)
//		sprintf (ret, "%s/%s", req->fr_Drawer, req->fr_File);
//	else

	strcpy (ret, req->fr_File);
	if (!strlen(req->fr_Drawer))
		strcpy (*path, "");
	else
	{
		strcpy (lastdir, req->fr_Drawer);
		strcpy (*path, req->fr_Drawer);
	}

	IAsl->FreeAslRequest(req);
	return ret;
}

char * request_arguments()
{
	Object *reqobj;

	char *ret = IExec->AllocMem(1024, MEMF_ANY|MEMF_CLEAR);	

	reqobj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
            									REQ_Type,       REQTYPE_STRING,
												REQ_TitleText,  "Specify arguments",
												REQS_Buffer,	ret,
												REQS_MaxChars,	1024,
												REQS_AllowEmpty, TRUE,
									            REQ_BodyText,   "Please specify command arguments",
									            //REQ_Image,      REQ_IMAGE,
												//REQ_TimeOutSecs, 10,
									            REQ_GadgetText, "Ok",
									            TAG_END
        );

	if ( reqobj )
	{
		IIntuition->IDoMethod( reqobj, RM_OPENREQ, NULL, mainwin, NULL, TAG_END );
		IIntuition->DisposeObject( reqobj );
	}

	return ret;
}

void strip_for_tabs (char *str)
{
	if (!str) return;
	char *ptr = str;
	while (*ptr != '\0')
	{
		if (*ptr == '\t' || *ptr == '\n')
			*ptr = ' ';
		ptr++;
	}
}

char *getlinefromfile (int line)
{
	char *ret = NULL;
	char fullpath[1024];

	if (!hasfunctioncontext)
		return ret;

	if (strlen (current_function->sourcename) == 0)
		return ret;
	if (strlen (childpath) == 0)
		strcpy (fullpath, current_function->sourcename);
	else if (childpath[strlen(childpath)-1] == '/')
		sprintf(fullpath, "%s%s", childpath, current_function->sourcename);
	else
		sprintf(fullpath, "%s/%s", childpath, current_function->sourcename);


	FILE *stream = fopen (fullpath, "r");
	if (stream)
	{
		ret = IExec->AllocMem(1024, MEMF_ANY|MEMF_CLEAR);

		int i;
		for(i = 1; ; i++)
		{
			if (0 >= fgets (ret, 1024, stream))
				break;
			if (i == line)
				break;
		}
		fclose (stream);
	}
	else
		return  "-- no source file --";

	strip_for_tabs(ret);
	return ret;
}


void button_setdisabled (Object *g, BOOL disabled)
{
	IIntuition->SetGadgetAttrs((struct Gadget *)g, mainwin, NULL,
												GA_Disabled, disabled,
												TAG_DONE);
}

void output_statement (char *statement)
{
	struct Node *node;
	char *str = strdup (statement);
	if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 1,
                								LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&listbrowser_list, node);
									}
	IIntuition->RefreshGadgets ((struct Gadget *)ListBrowserObj, mainwin, NULL);
}

void output_lineinfile (uint32 line)
{
	struct Node *node;
	char *strline = getlinefromfile(line);
	char *numberstr = malloc (256);
	sprintf(numberstr, "%d", line);

	if (current_function->currentline == 0)
		output_statement (current_function->sourcename);

	if (node = IListBrowser->AllocListBrowserNode(2,
												LBNA_Column, 0,
												LBNCA_Text, numberstr,
            									LBNA_Column, 1,
                								LBNCA_Text, strline,
            									TAG_DONE))
        							{
							            IExec->AddTail(&listbrowser_list, node);
									}
	IIntuition->RefreshGadgets ((struct Gadget *)ListBrowserObj, mainwin, NULL);
}


void free_list()
{
		/* yeah I know, this is not the right way to do things...*/

		IExec->NewList (&listbrowser_list);
		IIntuition->RefreshGadgets ((struct Gadget *)ListBrowserObj, mainwin, NULL);
}

uint32 main_obtain_window_signal()
{
	uint32 signal = 0x0;
	if (main_window_is_open)
		IIntuition->GetAttr( WINDOW_SigMask, MainWinObj, &signal );

	return signal;
}

uint32 obtain_all_signals()
{
	uint32 signal = 0x0;

	signal |= main_obtain_window_signal();
	signal |= registers_obtain_window_signal();
	signal |= hex_obtain_window_signal();
	signal |= disassembler_obtain_window_signal();
	signal |= locals_obtain_window_signal();
	signal |= stacktrace_obtain_window_signal();
	signal |= globals_obtain_window_signal();
	signal |= debug_sigfield;

	return signal;
}

void main_open_window()
{
    columninfo = IListBrowser->AllocLBColumnInfo(2,
        LBCIA_Column, 0,
            LBCIA_Title, "Next Line    ",
            LBCIA_Width, 80,
        LBCIA_Column, 1,
            LBCIA_Title, "Statement",
        TAG_DONE);

	IExec->NewList (&listbrowser_list);


    /* Create the window object. */
    if(( MainWinObj = WindowObject,
            WA_ScreenTitle, "DEbug 101",
            WA_Title, "DEbug 101 v0.7.1",
            WA_Width, 500,
			WA_Height, 500,
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
				LAYOUT_AddChild, HLayoutObject,
	                LAYOUT_AddChild, SelectButtonObj = ButtonObject,
	                    GA_ID, GAD_FILENAME_BUTTON,
	                    GA_Text, "Select file",
	                    GA_RelVerify, TRUE,
	                    GA_HintInfo, "Push to select file",
	                ButtonEnd,
	                CHILD_WeightedWidth, 0,
	                
	                LAYOUT_AddChild, FilenameStringObj = StringObject,
	                    GA_ID, GAD_TEXT,
	                    GA_RelVerify, TRUE,
	                    GA_HintInfo, "Filename",
						GA_Disabled, TRUE,
						GA_ReadOnly, TRUE,
						STRINGA_TextVal, "Select File",
	                StringEnd,

				EndMember,
				CHILD_WeightedHeight, 0,
				LAYOUT_AddChild, HLayoutObject,
					LAYOUT_AddChild, StartButtonObj = ButtonObject,
						GA_ID, GAD_START_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Start",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                
					LAYOUT_AddChild, PauseButtonObj = ButtonObject,
						GA_ID, GAD_PAUSE_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Pause",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                
					LAYOUT_AddChild, StepButtonObj = ButtonObject,
						GA_ID, GAD_STEP_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Step",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, KillButtonObj = ButtonObject,
						GA_ID, GAD_KILL_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Kill",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
#if 0
					LAYOUT_AddChild, CrashButtonObj = ButtonObject,
						GA_ID, GAD_CRASH_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Crash",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
#endif
					LAYOUT_AddChild, SetBreakButtonObj = ButtonObject,
						GA_ID, GAD_SETBREAK_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Set Break",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, XButtonObj = ButtonObject,
						GA_ID, GAD_X_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "X",
						GA_Disabled, FALSE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
					CHILD_WeightedWidth, 0,
				EndMember,
				CHILD_WeightedHeight, 0,

	            LAYOUT_AddChild, ListBrowserObj = ListBrowserObject,
    	            GA_ID,                     GAD_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &listbrowser_list,
                    LISTBROWSER_ColumnInfo,    columninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

				LAYOUT_AddChild, HLayoutObject,
					LAYOUT_AddChild, HexviewButtonObj = ButtonObject,
						GA_ID, GAD_HEX_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "HexView",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, VariablesButtonObj = ButtonObject,
						GA_ID, GAD_VARIABLES_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Locals",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, RegisterviewButtonObj = ButtonObject,
						GA_ID, GAD_REGISTERS_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Registers",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, DisassembleviewButtonObj = ButtonObject,
						GA_ID, GAD_DISASSEMBLE_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Disassemble",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

	            EndMember,
				CHILD_WeightedHeight, 0,

				LAYOUT_AddChild, HLayoutObject,
					LAYOUT_AddChild, StacktraceButtonObj = ButtonObject,
						GA_ID, GAD_STACKTRACE_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Stacktrace",
						GA_Disabled, FALSE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, GlobalsButtonObj = ButtonObject,
						GA_ID, GAD_GLOBALS_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Globals",
						GA_Disabled, FALSE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
				EndMember,
				CHILD_WeightedHeight, 0,

	        EndMember,

        EndWindow))
    {
        /*  Open the window. */
        if( mainwin = (struct Window *) RA_OpenWindow(MainWinObj) )
			main_window_is_open = TRUE;
	}

	return;
}

BOOL done = FALSE;

void event_loop()
{	
            ULONG wait, signal;
			BOOL shouldplay = FALSE;

            /* Obtain the window wait signal mask. */
            //IIntuition->GetAttr( WINDOW_SigMask, MainWinObj, &signal );
            
            /* Input Event Loop */
            while( !done )
            {
				signal = obtain_all_signals();

                wait = IExec->Wait(signal|SIGBREAKF_CTRL_C|SIGF_CHILD);
                
                if (wait & SIGBREAKF_CTRL_C) done = TRUE;
				if(wait & debug_sigfield)
				{
					BOOL tracing = FALSE;

					//signal from debugger means TRAP
					task_playing = FALSE;
					suspend_all_breakpoints();

					if (asm_trace)
					{
						asmstep_remove();
						tracing = TRUE;
					}

					if (should_continue)
					{
						install_all_breakpoints();
						shouldplay = TRUE;
						//play();
						should_continue = FALSE;
					}
					else
					{

						//context_ptr->ip = context_copy.ip;
	
						button_setdisabled (StartButtonObj, FALSE);
						button_setdisabled (PauseButtonObj, TRUE);
						button_setdisabled (StepButtonObj, FALSE);
						button_setdisabled (VariablesButtonObj, FALSE);
						button_setdisabled (KillButtonObj, FALSE);
						button_setdisabled (CrashButtonObj, FALSE);
						button_setdisabled (SelectButtonObj, TRUE);
						button_setdisabled (DisassembleviewButtonObj, FALSE);

						remove_line_breakpoints();

						if (hasfunctioncontext)
						{
							int nline = get_nline_from_address (context_copy.ip);

							if (nline != -1)
							{
								current_function->currentline = nline;
								output_lineinfile (current_function->lineinfile[current_function->currentline]);
							}
							else
								if (!tracing)
									output_statement ("Function overload error!");
						}
						else
							output_statement ("Program has reached an unknown TRAP");

						registers_update_window();
						locals_update_window();
						globals_update_window();
						disassembler_makelist();
					}
				}
                if(wait & SIGF_CHILD)
				{
					task_exists = FALSE;
					task_playing = FALSE;
					should_continue = FALSE;
					asm_trace = FALSE;
					hasfunctioncontext = FALSE;
					current_function = NULL;
					breakpoints_installed = FALSE;

					button_setdisabled (StartButtonObj, TRUE);
					button_setdisabled (PauseButtonObj, TRUE);
					button_setdisabled (StepButtonObj, TRUE);
					button_setdisabled (KillButtonObj, TRUE);
					button_setdisabled (CrashButtonObj, TRUE);
					button_setdisabled (SelectButtonObj, FALSE);
					button_setdisabled (SetBreakButtonObj, TRUE);
					button_setdisabled (FilenameStringObj, TRUE);
					button_setdisabled (HexviewButtonObj, TRUE);
					button_setdisabled (VariablesButtonObj, TRUE);
					button_setdisabled (RegisterviewButtonObj, TRUE);
					button_setdisabled (DisassembleviewButtonObj, TRUE);
					IIntuition->RefreshGadgets ((struct Gadget *)FilenameStringObj, mainwin, NULL);

					//close_elfhandle(exec_elfhandle);
					globals_destroy_list();
					registers_close_window();
					hex_close_window();
					disassembler_close_window();
					locals_close_window();

					output_statement ("Program has ended");
				}
				if (wait & registers_obtain_window_signal())
				{
					registers_event_handler();
				}
				if (wait & main_obtain_window_signal())
				{
					main_event_handler();
				}
				if (wait & hex_obtain_window_signal())
				{
					hex_event_handler();
				}
				if (wait & disassembler_obtain_window_signal())
				{
					disassembler_event_handler();
				}
				if (wait & locals_obtain_window_signal())
				{
					locals_event_handler();
				}
				if (wait & stacktrace_obtain_window_signal())
				{
					stacktrace_event_handler();
				}
				if (wait & globals_obtain_window_signal())
				{
					globals_event_handler();
				}
				if (shouldplay)
				{
					play();
					task_playing = TRUE;
				}
				shouldplay = FALSE;
			}


			return;
}

void cleanup()
{
	remove_hook();
	killtask();
	registers_close_window();
	locals_close_window();
	hex_close_window();
	stacktrace_close_window();
	globals_close_window();
	main_close_window();
}



void main_event_handler()
{
			ULONG result;
            WORD Code;
            char *strinfo, *args;
			char *path;
			int line=0;
			int *iptr;
            struct Node *node;
			uint32 addr;


                while ((result = RA_HandleInput(MainWinObj, &Code)) != WMHI_LASTMSG && done != TRUE)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;

                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {
                                case GAD_FILENAME_BUTTON:

									strinfo = request_file(mainwin, &path);
									if (!strinfo)
										break;

									strcpy (filename, strinfo);
									if (strlen(path) > 0)
										strcpy (childpath, path);

									args = request_arguments();

									if (!load_inferior (strinfo, path, args))
									{
										output_statement ("New process loaded");

										init_breakpoints();
										stabs_make_function_list();
										stabs_interpret_typedefs();
										get_symbols();
										stabs_interpret_globals();

										button_setdisabled (SelectButtonObj, TRUE);
										button_setdisabled (StartButtonObj, FALSE);
										button_setdisabled (KillButtonObj, FALSE);
										button_setdisabled (CrashButtonObj, TRUE);
										button_setdisabled (SetBreakButtonObj, FALSE);
										button_setdisabled (FilenameStringObj, FALSE);
										button_setdisabled (HexviewButtonObj, FALSE);
										button_setdisabled (RegisterviewButtonObj, FALSE);

										IIntuition->SetGadgetAttrs((struct Gadget *)FilenameStringObj, mainwin, NULL,
														STRINGA_TextVal, strinfo,
														TAG_DONE);
									}

                                    break;

								case GAD_START_BUTTON:

									if (is_breakpoint_at(context_copy.ip))
									{
										should_continue = TRUE;
										asmstep();
										break;
									}
									install_all_breakpoints();

									button_setdisabled (StartButtonObj, TRUE);
									button_setdisabled (PauseButtonObj, FALSE);
									button_setdisabled (StepButtonObj, TRUE);
									button_setdisabled (KillButtonObj, FALSE);
									button_setdisabled (CrashButtonObj, TRUE);
									button_setdisabled (SelectButtonObj, TRUE);

									play();

									break;

								case GAD_PAUSE_BUTTON:
									button_setdisabled (StartButtonObj, FALSE);
									button_setdisabled (PauseButtonObj, TRUE);
									button_setdisabled (StepButtonObj, FALSE);

									pause();

									registers_update_window();

									break;

								case GAD_STEP_BUTTON:

									button_setdisabled (StepButtonObj, TRUE);

									if (hasfunctioncontext)
									{
										step();
									
										//if (current_function->currentline == current_function->numberoflines - 1)
											//button_setdisabled (StepButtonObj, TRUE);
									}

									break;

								case GAD_KILL_BUTTON:
									button_setdisabled (StartButtonObj, TRUE);
									button_setdisabled (PauseButtonObj, TRUE);
									button_setdisabled (StepButtonObj, TRUE);
									button_setdisabled (KillButtonObj, TRUE);
									button_setdisabled (CrashButtonObj, TRUE);
									button_setdisabled (SelectButtonObj, FALSE);
									button_setdisabled (FilenameStringObj, TRUE);
									IIntuition->RefreshGadgets ((struct Gadget *)FilenameStringObj, mainwin, NULL);

									output_statement ("Program has been killed");
									
									killtask();

									break;

								case GAD_CRASH_BUTTON:

									crash();

									break;

								case GAD_SETBREAK_BUTTON:

									if (task_playing)
										pause();

									current_function = select_symbol();
									if (current_function)
									{
										button_setdisabled (StepButtonObj, TRUE);

										//output_statement (current_function->sourcename);
										insert_breakpoint(current_function->address, BR_NORMAL);

										hasfunctioncontext = TRUE;
									}
									break;

								case GAD_X_BUTTON:

									free_list();
									break;

								case GAD_HEX_BUTTON:

									hex_open_window();
									break;

								case GAD_VARIABLES_BUTTON:

									locals_open_window();
									break;

								case GAD_REGISTERS_BUTTON:

									registers_open_window();
									break;

								case GAD_DISASSEMBLE_BUTTON:

									disassembler_open_window();
									break;

								case GAD_STACKTRACE_BUTTON:

									stacktrace_open_window();
									break;

								case GAD_GLOBALS_BUTTON:

									globals_open_window();
									break;
                            }
                            break;
                    }
                }

			return;

}

void main_close_window()
{
	if (main_window_is_open)
	{

        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( MainWinObj );

		main_window_is_open = FALSE;
    }
    
    return;
}