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
#include <proto/space.h>

#include <classes/window.h>
#include <classes/requester.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>
#include <gadgets/space.h>

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
#include "attach.h"
#include "breakpointswindow.h"
#include "freemem.h"
#include "arexxport.h"
#include "arexxconsole.h"
#include "unix.h"
#include "console.h"
#include "pipe.h"

enum
{
    GAD_FILENAME_BUTTON = 1,
    GAD_RELOAD_BUTTON,
	GAD_ATTACH_BUTTON,
	GAD_START_BUTTON,
	GAD_PAUSE_BUTTON,
	GAD_STEPOVER_BUTTON,
	GAD_STEPINTO_BUTTON,
	GAD_KILL_BUTTON,
	GAD_CRASH_BUTTON,
	GAD_SETBREAK_BUTTON,
	GAD_X_BUTTON,
	GAD_VARIABLES_LISTBROWSER,
	GAD_STACKTRACE_LISTBROWSER,
	GAD_SOURCE_LISTBROWSER,
	GAD_CONSOLE_LISTBROWSER,
	GAD_HEX_BUTTON,
	GAD_DISASSEMBLE_BUTTON,
	GAD_STACKTRACE_BUTTON,
	GAD_AREXX_BUTTON,
	GAD_TEXT
};

extern struct List variable_list;
extern struct List stacktrace_list;
extern struct List console_list;
extern struct List source_list;

extern struct ColumnInfo *variablescolumninfo;
extern struct ColumnInfo *sourcecolumninfo;

struct Window *mainwin;

Object *MainWinObj;
BOOL main_window_is_open = FALSE;

Object *SelectButtonObj, *ReloadButtonObj, *FilenameStringObj, *AttachButtonObj;
Object *StartButtonObj, *PauseButtonObj, *StepOverButtonObj, *StepIntoButtonObj, *KillButtonObj, *CrashButtonObj, *SetBreakButtonObj, *XButtonObj;
Object *HexviewButtonObj, *DisassembleviewButtonObj, *StacktraceButtonObj;
Object *ArexxButtonObj;
extern Object *VariablesListBrowserObj, *StacktraceListBrowserObj, *FilesListBrowserObj, *ConsoleListBrowserObj, *SourceListBrowserObj;

char lastdir[1024] = "src"; //"qt:examples/widgets/calculator"; //work:code/medium/StangTennis2D";
char filename[1024] = "";
char childpath[1024] = "";

struct stab_function *current_function = NULL;
struct stab_function *old_function = NULL;
BOOL hasfunctioncontext = FALSE;
BOOL isattached = FALSE;

int main_freemem_hook = -1;

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
		console_printf(OUTPUT_WARNING, "Couldn't allocate requester!\n");	
		return NULL;
	}
	if (!(IAsl->AslRequest (req, NULL)))
	{
		return NULL;
	}

	ret = (char *)freemem_malloc(main_freemem_hook, 1024);
	*path = (char *)freemem_malloc(main_freemem_hook, 1024);

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

	char *ret = freemem_malloc(main_freemem_hook, 1024);	

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


void button_setdisabled (Object *g, BOOL disabled)
{
	IIntuition->SetGadgetAttrs((struct Gadget *)g, mainwin, NULL,
												GA_Disabled, disabled,
												TAG_DONE);
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
	signal |= hex_obtain_window_signal();
	signal |= disassembler_obtain_window_signal();
	signal |= breakpoints_obtain_window_signal();
	signal |= arexx_obtain_signal();
	signal |= arexxconsole_obtain_window_signal();
	signal |= pipe_obtain_signal();
	signal |= debug_sigfield;

	return signal;
}

void main_open_window()
{
	main_freemem_hook = freemem_alloc_hook();

	locals_init();
	console_init();
	stacktrace_init();
	source_init();

	pipe_init();

    /* Create the window object. */
    if(( MainWinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Debug 101 v0.9.1",
            WA_Width, 1024,
			WA_Height, 768,
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
	                
	                LAYOUT_AddChild, ReloadButtonObj = ButtonObject,
	                	GA_ID, GAD_RELOAD_BUTTON,
	                	GA_Text, "Reload",
	                	GA_RelVerify, TRUE,
	                	GA_HintInfo, "Push to reload previously executed file",
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

	                LAYOUT_AddChild, AttachButtonObj = ButtonObject,
	                    GA_ID, GAD_ATTACH_BUTTON,
	                    GA_Text, "Attach",
	                    GA_RelVerify, TRUE,
	                    GA_HintInfo, "Push to attach to process",
	                ButtonEnd,
	                CHILD_WeightedWidth, 0,
	
					LAYOUT_AddChild, SpaceObject,
						SPACE_MinWidth, 24,
					SpaceEnd,
					
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
					LAYOUT_AddChild, StepOverButtonObj = ButtonObject,
						GA_ID, GAD_STEPOVER_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Step Over",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                
					LAYOUT_AddChild, StepIntoButtonObj = ButtonObject,
						GA_ID, GAD_STEPINTO_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Step Into",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, SetBreakButtonObj = ButtonObject,
						GA_ID, GAD_SETBREAK_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Set Break",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, SpaceObject,
						SPACE_MinWidth, 24,
					SpaceEnd,
				EndMember,
				CHILD_WeightedHeight, 0,

				LAYOUT_AddChild, HLayoutObject,
		            LAYOUT_AddChild, StacktraceListBrowserObj = ListBrowserObject,
	    	            GA_ID,                     GAD_STACKTRACE_LISTBROWSER,
	        	        GA_RelVerify,              TRUE,
	            	    GA_TabCycle,               TRUE,
	                	LISTBROWSER_AutoFit,       TRUE,
	                    LISTBROWSER_Labels,        &stacktrace_list,
//	                    LISTBROWSER_ColumnInfo,    columninfo,
 	             	    LISTBROWSER_ColumnTitles,  FALSE,
		               	LISTBROWSER_ShowSelected,  FALSE,
	                    LISTBROWSER_Striping,      LBS_ROWS,
					ListBrowserEnd,
				
	                LAYOUT_AddChild, VariablesListBrowserObj = ListBrowserObject,
    	                GA_ID,                      GAD_VARIABLES_LISTBROWSER,
        	            GA_RelVerify,               TRUE,
	                    GA_TabCycle,                TRUE,
						LISTBROWSER_Hierarchical,	TRUE,
	                    //LISTBROWSER_AutoFit,        TRUE,
	                    LISTBROWSER_Labels,         &variable_list,
	                    LISTBROWSER_ColumnInfo,     variablescolumninfo,
	                    LISTBROWSER_ColumnTitles,   TRUE,
	                    //LISTBROWSER_ShowSelected,   TRUE, /* not really needed yet */
	                    LISTBROWSER_Striping,       LBS_ROWS,
	                    LISTBROWSER_SortColumn,     0,
	                    LISTBROWSER_TitleClickable, TRUE,
	                ListBrowserEnd,
	            EndMember,
	            
	            LAYOUT_AddChild, SourceListBrowserObj = ListBrowserObject,
    	            GA_ID,                     GAD_SOURCE_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &source_list,
                    LISTBROWSER_ColumnInfo,    sourcecolumninfo,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
					//LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,
				
				LAYOUT_AddChild, HLayoutObject,
		            LAYOUT_AddChild, ConsoleListBrowserObj = ListBrowserObject,
    		            GA_ID,                     GAD_CONSOLE_LISTBROWSER,
        		        GA_RelVerify,              TRUE,
            		    GA_TabCycle,               TRUE,
                		LISTBROWSER_AutoFit,       TRUE,
                	    LISTBROWSER_Labels,        &console_list,
                		//LISTBROWSER_ShowSelected,  TRUE,
					ListBrowserEnd,

					LAYOUT_AddChild, XButtonObj = ButtonObject,
						GA_ID, GAD_X_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "X",
						GA_Disabled, FALSE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
					CHILD_WeightedWidth, 0,
				EndMember,
				
				LAYOUT_AddChild, HLayoutObject,
					LAYOUT_AddChild, HexviewButtonObj = ButtonObject,
						GA_ID, GAD_HEX_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "HexView",
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

					LAYOUT_AddChild, ArexxButtonObj = ButtonObject,
						GA_ID, GAD_AREXX_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Console",
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
					//printf("SIG from debug hook\n");
					//printf("ip = 0x%08x\n", *((int *)debug_hook.h_Data));

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

						context_ptr->ip = context_copy.ip;
	
						button_setdisabled (StartButtonObj, FALSE);
						button_setdisabled (PauseButtonObj, TRUE);
						button_setdisabled (StepOverButtonObj, FALSE);
						button_setdisabled (StepIntoButtonObj, FALSE);
						//button_setdisabled (VariablesButtonObj, FALSE);
						button_setdisabled (KillButtonObj, FALSE);
						button_setdisabled (CrashButtonObj, FALSE);
						button_setdisabled (SelectButtonObj, TRUE);
						button_setdisabled (DisassembleviewButtonObj, FALSE);

						remove_line_breakpoints();

						current_function = stabs_get_function_from_address (context_copy.ip);
						if (current_function)
							hasfunctioncontext = TRUE;

						if (hasfunctioncontext)
						{
							if (current_function != old_function)
								output_functionheader();
							old_function = current_function;

							int nline = get_nline_from_address (context_copy.ip);

							if (nline != -1)
							{
								current_function->currentline = nline;
								output_lineinfile (current_function->lineinfile[current_function->currentline]);
								source_update();
							}
							else
								if (!tracing)
								{
									console_printf(OUTPUT_WARNING, "Function overload error!");
									//printf("function size: 0x%x function address: 0x%x\n", current_function->size, current_function->address);
								}
						}
						else
							console_printf(OUTPUT_WARNING, "Program has reached an unknown TRAP");

						//registers_update_window();
						locals_update();
						//globals_update_window();
						stacktrace_update();
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
					button_setdisabled (StepOverButtonObj, TRUE);
					button_setdisabled (StepIntoButtonObj, TRUE);
					button_setdisabled (KillButtonObj, TRUE);
					button_setdisabled (CrashButtonObj, TRUE);
					button_setdisabled (SelectButtonObj, FALSE);
					button_setdisabled (SetBreakButtonObj, TRUE);
					button_setdisabled (FilenameStringObj, TRUE);
					button_setdisabled (HexviewButtonObj, TRUE);
					//button_setdisabled (VariablesButtonObj, TRUE);
					//button_setdisabled (RegisterviewButtonObj, TRUE);
					button_setdisabled (DisassembleviewButtonObj, TRUE);
					IIntuition->RefreshGadgets ((struct Gadget *)FilenameStringObj, mainwin, NULL);

					free_symbols();
					stabs_free_stabs();
					free_breakpoints();

					//globals_close_window();
					//registers_close_window();
					hex_close_window();
					disassembler_close_window();
					//locals_close_window();

					console_printf(OUTPUT_SYSTEM, "Program has ended");
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
				if (wait & breakpoints_obtain_window_signal())
				{
					breakpoints_event_handler();
				}
				if (wait & arexx_obtain_signal())
				{
					arexx_event_handler();
				}
				if (wait & arexxconsole_obtain_window_signal())
				{
					arexxconsole_event_handler();
				}
				if(wait & pipe_obtain_signal())
				{
					char buffer[1024];
					int len = pipe_read(buffer);
					console_write_raw_data(OUTPUT_FROM_EXECUTABLE, buffer, len);
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
	if (!isattached)
		killtask();

	free_symbols();
	stabs_free_stabs();

	//registers_close_window();
	//locals_close_window();
	hex_close_window();
	//stacktrace_close_window();
	//globals_close_window();
	disassembler_close_window();
	breakpoints_close_window();
	main_close_window();
	arexxconsole_close_window();
	arexx_close_port();
	
	pipe_cleanup();
	
	locals_cleanup();
	console_cleanup();
	stacktrace_cleanup();
	source_cleanup();
	freemem_free_hook(main_freemem_hook);
}

void main_play()
{
	if (!task_exists)
		return;
	if (is_breakpoint_at(context_copy.ip))
	{
		should_continue = TRUE;
		asmstep();
	}
	else
	{
		install_all_breakpoints();

		button_setdisabled (StartButtonObj, TRUE);
		button_setdisabled (PauseButtonObj, FALSE);
		button_setdisabled (StepOverButtonObj, TRUE);
		button_setdisabled (StepIntoButtonObj, TRUE);		
		button_setdisabled (KillButtonObj, FALSE);
		button_setdisabled (CrashButtonObj, TRUE);
		button_setdisabled (SelectButtonObj, TRUE);

		play();
	}
}

void main_pause()
{
	button_setdisabled (StartButtonObj, FALSE);
	button_setdisabled (PauseButtonObj, TRUE);
	button_setdisabled (StepOverButtonObj, TRUE);
	button_setdisabled (StepIntoButtonObj, TRUE);

	pause();

	//registers_update_window();
}

void main_step()
{
	button_setdisabled (StepOverButtonObj, TRUE);
	button_setdisabled (StepIntoButtonObj, TRUE);

	if (hasfunctioncontext)
	{
		step();
									
		//if (current_function->currentline == current_function->numberoflines - 1)
		//button_setdisabled (StepButtonObj, TRUE);
	}
}


void main_kill()
{
	button_setdisabled (StartButtonObj, TRUE);
	button_setdisabled (PauseButtonObj, TRUE);
	button_setdisabled (StepOverButtonObj, TRUE);
	button_setdisabled (StepIntoButtonObj, TRUE);	
	button_setdisabled (KillButtonObj, TRUE);
	button_setdisabled (CrashButtonObj, TRUE);
	button_setdisabled (SelectButtonObj, FALSE);
	button_setdisabled (FilenameStringObj, TRUE);
	//IIntuition->RefreshGadgets ((struct Gadget *)FilenameStringObj, mainwin, NULL);

	killtask();

	console_printf(OUTPUT_SYSTEM, "Program has been killed");
}

void main_load(char *name, char *path, char *args)
{
	if (!load_inferior (name, path, args))
	{
		console_printf (OUTPUT_SYSTEM, "New process loaded");
		init_breakpoints();
		console_printf(OUTPUT_SYSTEM, "interpreting stabs...");
		get_symbols();
		stabs_interpret_stabs();
		close_elfhandle(exec_elfhandle);
		console_printf(OUTPUT_SYSTEM, "Done!");

		button_setdisabled (SelectButtonObj, TRUE);
		button_setdisabled (StartButtonObj, FALSE);
		button_setdisabled (KillButtonObj, FALSE);
		button_setdisabled (CrashButtonObj, TRUE);
		button_setdisabled (SetBreakButtonObj, FALSE);
		button_setdisabled (FilenameStringObj, FALSE);
		button_setdisabled (HexviewButtonObj, FALSE);
		//button_setdisabled (RegisterviewButtonObj, FALSE);

		IIntuition->SetGadgetAttrs((struct Gadget *)FilenameStringObj, mainwin, NULL,
														STRINGA_TextVal, name,
														TAG_DONE);
	}
}

void main_attach(struct Process *pr)
{
	if (!pr)
		return;
	if (amigaos_attach(pr))
	{
		console_printf(OUTPUT_SYSTEM, "Attached to process");
		isattached = TRUE;

		init_breakpoints();

		stabs_interpret_typedefs();
		stabs_interpret_functions();
		get_symbols();
		stabs_interpret_globals();
		close_elfhandle(exec_elfhandle);

		button_setdisabled (SelectButtonObj, TRUE);
		button_setdisabled (StartButtonObj, FALSE);
		button_setdisabled (KillButtonObj, FALSE);
		button_setdisabled (CrashButtonObj, TRUE);
		button_setdisabled (SetBreakButtonObj, FALSE);
		button_setdisabled (FilenameStringObj, FALSE);
		button_setdisabled (HexviewButtonObj, FALSE);
		//button_setdisabled (RegisterviewButtonObj, FALSE);

										//IIntuition->SetGadgetAttrs((struct Gadget *)FilenameStringObj, mainwin, NULL,
										//				STRINGA_TextVal, strinfo,
										//				TAG_DONE);
	}
}


void main_event_handler()
{
			ULONG result;
            WORD Code;
            static char *strinfo, *args;
			static char *path;
			int line=0;
			int *iptr;
            struct Node *node;
			uint32 addr;
			struct Process *pr;


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
									{
										strcpy (childpath, path);
									}

									args = request_arguments();

									main_load (strinfo, path, args);
									
                                    break;
                                    
								case GAD_RELOAD_BUTTON:
								
									main_load(strinfo, path, args);
									
									break;
									
								case GAD_ATTACH_BUTTON:

									pr = attach_select_process();
									main_attach (pr);

									break;

								case GAD_START_BUTTON:

									main_play();
									break;

								case GAD_PAUSE_BUTTON:

									main_pause();
									break;

								case GAD_STEPOVER_BUTTON:

									main_step();
									break;

								case GAD_KILL_BUTTON:

									main_kill();
									break;

								case GAD_CRASH_BUTTON:

									crash();

									break;

								case GAD_SETBREAK_BUTTON:

									breakpoints_open_window();

									break;

								case GAD_VARIABLES_LISTBROWSER:
								
									locals_handle_input();
									break;

								case GAD_X_BUTTON:

									console_clear();
									break;

								case GAD_HEX_BUTTON:

									hex_open_window();
									break;

								case GAD_DISASSEMBLE_BUTTON:
									disassembler_open_window();
									break;


								case GAD_AREXX_BUTTON:

									arexxconsole_open_window();
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
