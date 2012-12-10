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
#include <proto/clicktab.h>
#include <proto/arexx.h>
#include <proto/gadtools.h>

#include <classes/window.h>
#include <classes/requester.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>
#include <gadgets/space.h>
#include <gadgets/clicktab.h>
#include <classes/arexx.h>
#include <gadgets/string.h>

#include <reaction/reaction_macros.h>
#include <intuition/classusr.h>

#include <libraries/asl.h>
#include <interfaces/asl.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdarg.h>

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
#include "unix.h"
#include "console.h"
#include "pipe.h"
#include "signalswindow.h"

#define dprintf(format...)	IExec->DebugPrintF(format)


extern struct List variable_list;
extern struct List stacktrace_list;
extern struct List console_list;
extern struct List source_list;
extern struct List disassembly_list;
extern struct List sourcelist_list;

extern struct ColumnInfo *variablescolumninfo;
extern struct ColumnInfo *sourcecolumninfo;

struct Window *mainwin;

Object *MainWinObj;
BOOL main_window_is_open = FALSE;

Object *arexx_obj;
Object *MainObj[GAD_NUM];

struct MsgPort *AppPort = NULL;

char lastdir[1024] =  "";  //"qt:examples/widgets/calculator"; //work:code/medium/StangTennis2D";  
char filename[1024] = "";
char childpath[1024] = "";
char childfullpath[1024] = "";

struct stab_function *current_function = NULL;
struct stab_function *old_function = NULL;
BOOL hasfunctioncontext = FALSE;
BOOL isattached = FALSE;
BOOL button_isattach = TRUE;
extern BOOL catch_sline;
extern BOOL stepping_out;
extern BOOL has_tracebit;

int main_freemem_hook = -1;

char *request_file (struct Window *win, char **path)
{
	struct FileRequester *req = IAsl->AllocAslRequestTags(ASL_FileRequest,
													  ASLFR_Window, win,
													  ASLFR_TitleText, "Select executable",
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

char * child_full_path()
{
	int len = strlen(childpath);
	if(len == 0)
		strcpy(childfullpath, filename);
	else
	{
		char c = childpath[len];
		switch(c)
		{
			case ':':
			case '/':
				sprintf(childfullpath, "%s%s", childpath, filename);
			default:
				sprintf(childfullpath, "%s/%s", childpath, filename);
		}
	}
	return childfullpath;
}

BOOL request_arguments(char **ret)
{
	Object *reqobj;

	*ret = freemem_malloc(main_freemem_hook, 1024);	

	reqobj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
            									REQ_Type,       REQTYPE_STRING,
												REQ_TitleText,  "Specify arguments",
												REQS_Buffer,	*ret,
												REQS_MaxChars,	1024,
												REQS_AllowEmpty, TRUE,
									            REQ_BodyText,   "Please specify command arguments",
									            //REQ_Image,      REQ_IMAGE,
												//REQ_TimeOutSecs, 10,
									            REQ_GadgetText, "Ok|Cancel",
									            TAG_END
        );

	uint32 code = TRUE;
	if ( reqobj )
	{
		IIntuition->IDoMethod( reqobj, RM_OPENREQ, NULL, mainwin, NULL, TAG_END );
		IIntuition->GetAttr(REQ_ReturnCode, reqobj, &code);
		IIntuition->DisposeObject( reqobj );
	}

	return code;
}


void button_setdisabled (Object *g, BOOL disabled)
{
	IIntuition->SetGadgetAttrs((struct Gadget *)g, mainwin, NULL,
												GA_Disabled, disabled,
												TAG_DONE);
}

void enable(BOOL able, ULONG tags, ...)
{
	va_list args;
	ULONG tag = tags;
#ifdef __amigaos4__
	va_startlinear(args, tags);
#else
	va_start(args, tags);
#endif
	while( tag != TAG_END )
	{
		button_setdisabled(MainObj[tag], !able);
		tag = va_arg(args, ULONG);
	}
	va_end(args);
}

BOOL button_is_start = FALSE;

void button_set_start()
{
	if(!button_is_start)
		IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_START_BUTTON], mainwin, NULL,
												GA_Text, "Start",
												TAG_DONE);
	button_is_start = TRUE;
}

void button_set_continue()
{
	if(button_is_start)
		IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_START_BUTTON], mainwin, NULL,
												GA_Text, "Continue",
												TAG_DONE);
	button_is_start = FALSE;
}

void button_set_attach()
{
	if(!button_isattach)
		IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_ATTACH_BUTTON], mainwin, NULL,
												GA_Text, "Attach",
												TAG_DONE);
	button_isattach = TRUE;
}

void button_set_detach()
{
	if(button_isattach)
		IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_ATTACH_BUTTON], mainwin, NULL,
												GA_Text, "Detach",
												TAG_DONE);
	button_isattach = FALSE;
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
	signal |= breakpoints_obtain_window_signal();
	signal |= arexx_obtain_signal();
	signal |= pipe_obtain_signal();
	signal |= sigwin_obtain_signal();
	signal |= debug_sigfield;

   if( AppPort )
      signal |= 1L << AppPort->mp_SigBit;

	return signal;
}

STRPTR PageLabels_1[] = {"Source", "Assembly", NULL};

void main_open_window()
{
	main_freemem_hook = freemem_alloc_hook();

	variables_init();
	console_init();
	stacktrace_init();
	source_init();
	disassembler_init();
	sourcelist_init();
	
	tracking_init();
	pipe_init();
	
	static struct NewMenu mynewmenu[] =
	{
	    { NM_TITLE, "File", 0, 0, 0, 0},
    	{ NM_ITEM, "About...", 0, 0, 0, 0 },
    	{ NM_ITEM, "Quit", 0, 0, 0 },
    	{ NM_END, NULL, 0, 0, 0, 0}
    };

   AppPort = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);

    /* Create the window object. */
    if(( MainWinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Debug 101 v1.0.0",
            WA_Width, 1024,
			WA_Height, 768,
            WA_DepthGadget, TRUE,
			WA_SizeGadget, TRUE,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_Activate, TRUE,
            
            WINDOW_NewMenu, mynewmenu,
            WINDOW_MenuUserData, WGUD_IGNORE,
            
            WINDOW_IconifyGadget, TRUE,
            WINDOW_IconTitle, "Debug 101",
            WINDOW_AppPort, AppPort,
            
            WINDOW_Position, WPOS_CENTERSCREEN,
            /* there is no HintInfo array set up here, instead we define the 
            ** strings directly when each gadget is created (OM_NEW)
            */
            WINDOW_GadgetHelp, TRUE,
	        WINDOW_ParentGroup, MainObj[GAD_TOPLAYOUT] = VLayoutObject,
				LAYOUT_AddChild, HLayoutObject,
	                LAYOUT_AddChild, MainObj[GAD_SELECT_BUTTON] = ButtonObject,
	                    GA_ID, GAD_SELECT_BUTTON,
	                    GA_Text, "Select file",
	                    GA_RelVerify, TRUE,
	                    GA_HintInfo, "Push to select file",
	                ButtonEnd,
	                CHILD_WeightedWidth, 0,
	                
	                LAYOUT_AddChild, MainObj[GAD_RELOAD_BUTTON] = ButtonObject,
	                	GA_ID, GAD_RELOAD_BUTTON,
	                	GA_Text, "Reload",
	                	GA_RelVerify, TRUE,
	                    GA_Disabled, TRUE,
	                	GA_HintInfo, "Push to reload previously executed file",
	                ButtonEnd,
	                CHILD_WeightedWidth, 0,
	                
	                LAYOUT_AddChild, MainObj[GAD_FILENAME_STRING] = StringObject,
	                    GA_ID, GAD_FILENAME_STRING,
	                    GA_RelVerify, TRUE,
	                    GA_HintInfo, "Filename",
						GA_Disabled, TRUE,
						GA_ReadOnly, TRUE,
						STRINGA_MinVisible, 10,
						STRINGA_TextVal, "Select File",
	                StringEnd,

	                LAYOUT_AddChild, MainObj[GAD_ATTACH_BUTTON] = ButtonObject,
	                    GA_ID, GAD_ATTACH_BUTTON,
	                    GA_Text, "Attach",
	                    GA_RelVerify, TRUE,
	                    GA_HintInfo, "Push to attach to process",
	                ButtonEnd,
	                CHILD_WeightedWidth, 0,
	
					LAYOUT_AddChild, SpaceObject,
						SPACE_MinWidth, 24,
					SpaceEnd,
					
					LAYOUT_AddChild, MainObj[GAD_START_BUTTON] = ButtonObject,
						GA_ID, GAD_START_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Continue",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                
					LAYOUT_AddChild, MainObj[GAD_PAUSE_BUTTON] = ButtonObject,
						GA_ID, GAD_PAUSE_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Pause",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                
					LAYOUT_AddChild, MainObj[GAD_KILL_BUTTON] = ButtonObject,
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
					LAYOUT_AddChild, MainObj[GAD_STEPOVER_BUTTON] = ButtonObject,
						GA_ID, GAD_STEPOVER_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Step Over",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                
					LAYOUT_AddChild, MainObj[GAD_STEPINTO_BUTTON] = ButtonObject,
						GA_ID, GAD_STEPINTO_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Step Into",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                
					LAYOUT_AddChild, MainObj[GAD_STEPOUT_BUTTON] = ButtonObject,
						GA_ID, GAD_STEPOUT_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Step Out",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                	                
					LAYOUT_AddChild, SpaceObject,
						SPACE_MinWidth, 24,
					SpaceEnd,
					
					LAYOUT_AddChild, MainObj[GAD_SETBREAK_BUTTON] = ButtonObject,
						GA_ID, GAD_SETBREAK_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Breaks",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

					LAYOUT_AddChild, MainObj[GAD_HEX_BUTTON] = ButtonObject,
						GA_ID, GAD_HEX_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Hex",
						GA_Disabled, TRUE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,

				EndMember,
				CHILD_WeightedHeight, 0,

				LAYOUT_AddChild, HLayoutObject,
		            LAYOUT_AddChild, MainObj[GAD_STACKTRACE_LISTBROWSER] = ListBrowserObject,
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
				
	                LAYOUT_AddChild, MainObj[GAD_VARIABLES_LISTBROWSER] = ListBrowserObject,
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
	            
	            LAYOUT_AddChild, HLayoutObject,
		            LAYOUT_AddChild, MainObj[GAD_CLICKTAB] = ClickTabObject,
		            	GA_Text, PageLabels_1,

		            	CLICKTAB_Current,	0,
		            	CLICKTAB_PageGroup,	PageObject,
				    		PAGE_Add, VLayoutObject,
				    			//LAYOUT_SpaceOuter,	TRUE,
				    			//LAYOUT_DeferLayout,	TRUE,
				    			LAYOUT_AddChild, MainObj[GAD_SOURCE_LISTBROWSER] = ListBrowserObject,
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
							EndMember,
						
							PAGE_Add, VLayoutObject,
								LAYOUT_AddChild, MainObj[GAD_DISASSEMBLER_LISTBROWSER] = ListBrowserObject,
			    	        	    GA_ID,                     GAD_DISASSEMBLER_LISTBROWSER,
	    		    	    	    GA_RelVerify,              TRUE,
	        		    		    GA_TabCycle,               TRUE,
									//GA_TextAttr,			   &courier_font,
	        	    	    		LISTBROWSER_AutoFit,       TRUE,
	        	    		  	    LISTBROWSER_Labels,        &disassembly_list,
	        	  		    	  	LISTBROWSER_ShowSelected,  TRUE,
	        	        		    LISTBROWSER_Striping,      LBS_ROWS,
								ListBrowserEnd,
							
								LAYOUT_AddChild, HLayoutObject,
									LAYOUT_AddChild, MainObj[GAD_DISASSEMBLER_STEP_BUTTON] = ButtonObject,
	    	            			    GA_ID, GAD_DISASSEMBLER_STEP_BUTTON,
										GA_RelVerify, TRUE,
	            	    			    GA_Text, "Step",
	            			    	ButtonEnd,
				                	CHILD_WeightedHeight, 0,

		                			LAYOUT_AddChild, MainObj[GAD_DISASSEMBLER_SKIP_BUTTON] = ButtonObject,
	    	            			    GA_ID, GAD_DISASSEMBLER_SKIP_BUTTON,
										GA_RelVerify, TRUE,
	            	        			GA_Text, "Skip",
    	        				   	ButtonEnd,
				                	CHILD_WeightedHeight, 0,
								EndMember,
			                	CHILD_WeightedHeight, 0,
							EndMember,
						PageEnd,
					EndMember,
					
					LAYOUT_WeightBar,	TRUE,
					LAYOUT_AddChild, MainObj[GAD_SOURCELIST_LISTBROWSER] = ListBrowserObject,
						GA_ID,                     GAD_SOURCELIST_LISTBROWSER,
	    		    	GA_RelVerify,              TRUE,
	        		    GA_TabCycle,               TRUE,
						LISTBROWSER_AutoFit,       TRUE,
	        	    	LISTBROWSER_Labels,        &sourcelist_list,
	        	  		LISTBROWSER_ShowSelected,  TRUE,
	        	        LISTBROWSER_Striping,      LBS_ROWS,
					ListBrowserEnd,
					CHILD_WeightedWidth,		20,
				EndMember,
				
				LAYOUT_AddChild, HLayoutObject,
		            LAYOUT_AddChild, MainObj[GAD_CONSOLE_LISTBROWSER] = ListBrowserObject,
    		            GA_ID,                     GAD_CONSOLE_LISTBROWSER,
        		        GA_RelVerify,              TRUE,
            		    GA_TabCycle,               TRUE,
                		LISTBROWSER_AutoFit,       TRUE,
                	    LISTBROWSER_Labels,        &console_list,
					ListBrowserEnd,
				EndMember,
				
				LAYOUT_AddChild, HLayoutObject,
	                LAYOUT_AddChild, MainObj[GAD_AREXX_STRING] = StringObject,
    	                GA_ID, GAD_AREXX_STRING,
        	            GA_RelVerify, TRUE,
						GA_Disabled, FALSE,
						GA_ReadOnly, FALSE,
						STRINGA_TextVal, "",
        	        StringEnd,

					LAYOUT_AddChild, MainObj[GAD_AREXX_BUTTON] = ButtonObject,
						GA_ID, GAD_AREXX_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Send",
						GA_Disabled, FALSE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                CHILD_WeightedWidth, 0,
	                
	                LAYOUT_AddChild, MainObj[GAD_SIGNAL_BUTTON] = ButtonObject,
						GA_ID, GAD_SIGNAL_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "Signal",
						GA_Disabled, FALSE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
	                CHILD_WeightedWidth, 0,
	                
					LAYOUT_AddChild, MainObj[GAD_X_BUTTON] = ButtonObject,
						GA_ID, GAD_X_BUTTON,
	                    GA_RelVerify, TRUE,
						GA_Text, "X",
						GA_Disabled, FALSE,
					ButtonEnd,
	                CHILD_WeightedHeight, 0,
					CHILD_WeightedWidth, 0,
				EndMember,
				CHILD_WeightedHeight, 0,

	        EndMember,

        EndWindow))
    {
        /*  Open the window. */
        if( mainwin = (struct Window *) RA_OpenWindow(MainWinObj) )
        {
			main_window_is_open = TRUE;
			button_set_start();
		}
	}

	return;
}

BOOL done = FALSE;
//#define    MSR_TRACE_ENABLE           0x00000400

void show_disassembler()
{
	IIntuition->SetAttrs(MainObj[GAD_CLICKTAB],
								CLICKTAB_Current, 1,
								TAG_END);
	IIntuition->IDoMethod(MainWinObj, WM_RETHINK);
}

void show_source()
{
	IIntuition->SetAttrs(MainObj[GAD_CLICKTAB],
								CLICKTAB_Current, 0,
								TAG_END);
	IIntuition->IDoMethod(MainWinObj, WM_RETHINK);
}

void event_loop()
{	
            ULONG wait, signal;
			BOOL shouldplay = FALSE;
			char *symbol = NULL;
			branch branchallowed = NOBRANCH;

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

					button_set_continue();

					BOOL tracing = FALSE;

					//signal from debugger means TRAP
					task_playing = FALSE;
					suspend_all_breakpoints();
					
					if(stepping_out)
					{
						stepping_out = FALSE;
						stepout_remove();
					}
					if (asm_trace)
					{
						asmstep_remove();
						if(!should_continue)
						{
							switch(is_branch_allowed())
							{
								case DISALLOWEDBRANCH:
									if(catch_sline)
									{
										stepping_out = TRUE;
										stepout_install();
										shouldplay = TRUE;
									}
									else
									{
										//console_printf(OUTPUT_WARNING, "Branch into new area not allowed!");
										enable(TRUE, GAD_STEPOUT_BUTTON, TAG_END);
										enable(FALSE, GAD_STEPINTO_BUTTON, GAD_STEPOVER_BUTTON, GAD_DISASSEMBLER_STEP_BUTTON, TAG_END);
									}
									catch_sline = FALSE;
									break;
								case DISALLOWEDBRANCHCOND:
									if(catch_sline)
									{
										stepping_out = TRUE;
										stepout_install();
										shouldplay = TRUE;
									}
									else
									{
										//console_printf(OUTPUT_WARNING, "Branch into new area not allowed!");
										enable(TRUE, GAD_STEPOVER_BUTTON, GAD_STEPOUT_BUTTON, TAG_END);
										enable(FALSE, GAD_STEPINTO_BUTTON, GAD_DISASSEMBLER_STEP_BUTTON, TAG_END);
									}
									catch_sline = FALSE;
									break;
								default:
									break;
							}
						}			
						tracing = TRUE;
						if(!should_continue && !catch_sline)
						{
							disassembler_makelist();
							variables_update();
							source_update();
						}
					}
					if (should_continue)
					{
						install_all_breakpoints();
						shouldplay = TRUE;
						should_continue = FALSE;
					}
					else
					{
						current_function = stabs_get_function_from_address (context_copy.ip);
						dprintf("current_function == 0x%x\n", current_function);
						if (current_function)
							hasfunctioncontext = TRUE;
						else
							hasfunctioncontext = FALSE;
							
						if (hasfunctioncontext)
						{
							int nline = get_nline_from_address (context_copy.ip);

							if (nline != -1)
							{
								catch_sline = FALSE;
								current_function->currentline = nline;
								//output_lineinfile (current_function->lineinfile[current_function->currentline]);

							}
							else if (!catch_sline)
							{
								nline = guess_line_in_function();
								if(nline)
									current_function->currentline = nline;
							}
							else if (!tracing)
							{
								console_printf(OUTPUT_WARNING, "Function overload error!");
								//printf("function size: 0x%x function address: 0x%x\n", current_function->size, current_function->address);
							}
						}
						else if(symbol = get_symbol_from_value(context_copy.ip))
						{
							console_printf(OUTPUT_NORMAL, "At symbol %s", symbol);
							if(catch_sline)
							{
								catch_sline = FALSE;
								enable(TRUE, GAD_START_BUTTON, GAD_STEPINTO_BUTTON, GAD_STEPOUT_BUTTON, GAD_DISASSEMBLER_STEP_BUTTON, TAG_END);
								enable(FALSE, GAD_PAUSE_BUTTON, GAD_STEPOVER_BUTTON, TAG_END);
							}
							source_update();
							variables_update();
							stacktrace_update();
							disassembler_makelist();
							
							show_disassembler();
						}
						else if(!tracing)
						{
							console_printf(OUTPUT_WARNING, "Program has stopped at an unknown point in memory (TRAP)");
							disassembler_makelist();
							show_disassembler();
						}

						if(catch_sline)
						{
							//static int count = 0;
							//dprintf("asmstep(%d)\n", count++);
							asmstep();
						}
						else if(hasfunctioncontext)
						{
							if (current_function != old_function)
								output_functionheader();
							old_function = current_function;

							enable(TRUE, GAD_START_BUTTON, GAD_STEPOVER_BUTTON, GAD_STEPINTO_BUTTON, GAD_STEPOUT_BUTTON, GAD_KILL_BUTTON, TAG_END);
							enable(FALSE, GAD_PAUSE_BUTTON, GAD_SELECT_BUTTON, TAG_END);

							remove_line_breakpoints();

							variables_update();
							stacktrace_update();
							disassembler_makelist();
							source_update();
							
							if(!tracing)
								show_source();
						}
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

					enable(TRUE, GAD_RELOAD_BUTTON, GAD_SELECT_BUTTON, TAG_END);
					enable(FALSE, GAD_START_BUTTON, GAD_PAUSE_BUTTON, GAD_STEPOVER_BUTTON, GAD_STEPINTO_BUTTON,
								  GAD_KILL_BUTTON, GAD_SETBREAK_BUTTON, GAD_FILENAME_STRING, GAD_HEX_BUTTON, TAG_END);

					button_set_start();
					IIntuition->RefreshGadgets ((struct Gadget *)MainObj[GAD_FILENAME_STRING], mainwin, NULL);

					free_symbols();
					stabs_free_stabs();
					free_breakpoints();
					tracking_clear();

					hex_close_window();
					variables_clear();
					source_clear();
					sourcelist_clear();
					stacktrace_clear();
					disassembler_clear();

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
				if (wait & breakpoints_obtain_window_signal())
				{
					breakpoints_event_handler();
				}
				if (wait & sigwin_obtain_signal())
				{
					sigwin_event_handler();
				}
				if(wait & arexx_obtain_signal())
				{
					arexx_event_handler();
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

	hex_close_window();
	breakpoints_close_window();
	main_close_window();
	arexx_close_port();
	
	IExec->FreeSysObject(ASOT_PORT, AppPort);
	
	pipe_cleanup();
	tracking_cleanup();
	
	variables_cleanup();
	console_cleanup();
	stacktrace_cleanup();
	source_cleanup();
	disassembler_cleanup();
	sourcelist_cleanup();
	
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
		
		enable(TRUE, GAD_PAUSE_BUTTON, GAD_KILL_BUTTON, TAG_END);
		enable(FALSE, GAD_START_BUTTON, GAD_STEPOVER_BUTTON, GAD_STEPINTO_BUTTON, GAD_STEPOUT_BUTTON, GAD_SELECT_BUTTON, TAG_END);

		should_continue = FALSE;
		if(!play())
			enable(TRUE, GAD_START_BUTTON, TAG_END);
	}
}

void main_pause()
{
	pause();

	enable(TRUE, GAD_START_BUTTON, TAG_END);
	enable(FALSE, GAD_PAUSE_BUTTON, TAG_END);
}

void main_step()
{
	enable(FALSE, GAD_STEPOVER_BUTTON, GAD_STEPINTO_BUTTON, GAD_STEPOUT_BUTTON, TAG_END);
	
	if (hasfunctioncontext)
	{
		step();
	}
}

void main_into()
{
	enable(TRUE, GAD_PAUSE_BUTTON, TAG_END);
	enable(FALSE, GAD_STEPOVER_BUTTON, GAD_STEPINTO_BUTTON, GAD_STEPOUT_BUTTON, TAG_END);
	
	if (1) //hasfunctioncontext)
	{
		into();
	}
}

void main_out()
{
	enable(FALSE, GAD_STEPOVER_BUTTON, GAD_STEPINTO_BUTTON, GAD_STEPOUT_BUTTON, TAG_END);
	out();
}

void main_kill()
{
	enable(TRUE, GAD_SELECT_BUTTON, TAG_END);
	enable(FALSE, GAD_START_BUTTON, GAD_PAUSE_BUTTON, GAD_STEPOVER_BUTTON, GAD_STEPINTO_BUTTON, GAD_STEPOUT_BUTTON,
				  GAD_KILL_BUTTON, GAD_FILENAME_STRING, TAG_END);

	killtask();

	console_printf(OUTPUT_SYSTEM, "Program has been killed");
}

void main_load(char *name, char *path, char *args)
{
	if (!load_inferior (name, path, args))
	{
		tracking_clear();		
		
		console_printf (OUTPUT_SYSTEM, "New process loaded");
		init_breakpoints();
		console_printf(OUTPUT_SYSTEM, "interpreting stabs...");
		get_symbols();
		stabs_interpret_stabs();
		close_elfhandle(exec_elfhandle);
		console_printf(OUTPUT_SYSTEM, "Done!");

		enable(TRUE, GAD_START_BUTTON, GAD_KILL_BUTTON, GAD_SETBREAK_BUTTON, GAD_FILENAME_STRING, GAD_HEX_BUTTON, TAG_END);
		enable(FALSE, GAD_SELECT_BUTTON, GAD_RELOAD_BUTTON, TAG_END);
		button_set_start();

		IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_FILENAME_STRING], mainwin, NULL,
														STRINGA_TextVal, name,
														TAG_DONE);
		sourcelist_update();
		sourcelist_show_main();
		show_source();
	}
}

BOOL main_attach(struct Process *pr)
{
	if (!pr)
		return FALSE;

	if (attach(pr))
	{
		console_printf(OUTPUT_SYSTEM, "Attached to process");
		isattached = TRUE;

		tracking_clear();
		init_breakpoints();

		stabs_interpret_stabs();
		close_elfhandle(exec_elfhandle);

		enable(TRUE, GAD_START_BUTTON, GAD_KILL_BUTTON, GAD_SETBREAK_BUTTON, GAD_FILENAME_STRING, GAD_HEX_BUTTON, GAD_STEPOVER_BUTTON,
					 GAD_STEPINTO_BUTTON, TAG_END);
		enable(FALSE, GAD_SELECT_BUTTON, TAG_END);

										//IIntuition->SetGadgetAttrs((struct Gadget *)FilenameStringObj, mainwin, NULL,
										//				STRINGA_TextVal, strinfo,
										//				TAG_DONE);
		return TRUE;
	}
	return FALSE;
}

void show_about()
{
	Object *reqobj;

	reqobj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
            									REQ_Type,       REQTYPE_INFO,
												REQ_TitleText,  "About",
									            REQ_BodyText,   "Debug101 v.1.0.0\n © Copyright 2012 by Alfkil Thorbjørn Wennermark\n THANKS TO:\nkas1e and NinjaDNA for testing\nAll the folks on the os4invite list for cooporation and support",
									            REQ_GadgetText, "Ok",
									            TAG_END
        );

	if ( reqobj )
	{
		IIntuition->IDoMethod( reqobj, RM_OPENREQ, NULL, mainwin, NULL, TAG_END );
		IIntuition->DisposeObject( reqobj );
	}
}

struct apExecute arexxexecute;
char arexxcommandstring[1024];
struct gpInput activemsg;

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
				
			case WMHI_ICONIFY:
				if( RA_Iconify(MainWinObj) )
				mainwin = NULL;
				break;

			case WMHI_UNICONIFY:
				mainwin = RA_Uniconify(MainWinObj);
				if( !mainwin )
					done = TRUE;
				break;

			case WMHI_MENUPICK:
				if (Code != MENUNULL)
				switch (MENUNUM(Code))
				{
					case 0:
						switch(ITEMNUM(Code))
						{
							case 0:
								show_about();
								break;
							case 1:
								done = TRUE;
								break;
						}
				}
				break;
			
            case WMHI_GADGETUP:
            switch(result & WMHI_GADGETMASK)
            {
				case GAD_SELECT_BUTTON:

					strinfo = request_file(mainwin, &path);
					if (!strinfo)
						break;

					strcpy (filename, strinfo);
					if (strlen(path) > 0)
					{
						strcpy (childpath, path);
					}

					if(!request_arguments(&args))
					{
						console_printf(OUTPUT_WARNING, "User aborted!");
						break;
					}

					main_load (strinfo, path, args);
									
                    break;
                                    
				case GAD_RELOAD_BUTTON:
								
					main_load(strinfo, path, args);
					break;
									
				case GAD_ATTACH_BUTTON:

					if(!isattached)
					{
						pr = attach_select_process();
						if(main_attach (pr))
							button_set_detach();
					}
					else
					{
						console_printf(OUTPUT_SYSTEM, "Detaching from external process...");
						detach();
						button_set_attach();
						enable(TRUE, GAD_SELECT_BUTTON, TAG_END);
						enable(FALSE, GAD_START_BUTTON, GAD_PAUSE_BUTTON, GAD_KILL_BUTTON, GAD_STEPOVER_BUTTON,
										GAD_STEPINTO_BUTTON, GAD_STEPOUT_BUTTON, GAD_SETBREAK_BUTTON, GAD_HEX_BUTTON, TAG_END);
						console_printf(OUTPUT_SYSTEM, "Done!");
					}
						
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

				case GAD_STEPINTO_BUTTON:
				
					main_into();
					break;

				case GAD_STEPOUT_BUTTON:
				
					main_out();
					break;

				case GAD_KILL_BUTTON:

					main_kill();
					break;
#if 0
				case GAD_CRASH_BUTTON:

					crash();
					break;
#endif
				case GAD_SETBREAK_BUTTON:

					breakpoints_open_window();
					break;

				case GAD_VARIABLES_LISTBROWSER:
								
					variables_handle_input();
					break;

				case GAD_SIGNAL_BUTTON:
				
					sigwin_open_window();
					break;

				case GAD_X_BUTTON:

					console_clear();
					break;

				case GAD_HEX_BUTTON:

					hex_open_window();
					break;

				case GAD_DISASSEMBLER_LISTBROWSER:
					disassembler_show_selected();
					break;
									
				case GAD_DISASSEMBLER_STEP_BUTTON:
					asmstep();
					should_continue = FALSE;
					asm_trace = TRUE;
					break;
									
				case GAD_DISASSEMBLER_SKIP_BUTTON:
					asmskip();
					disassembler_makelist();
					break;

				case GAD_SOURCE_LISTBROWSER:
					source_handle_input();
					source_show_currentline();
					break;
				
				case GAD_SOURCELIST_LISTBROWSER:
					sourcelist_handle_input();
					break;
					
				case GAD_AREXX_BUTTON:
				case GAD_AREXX_STRING:
				{
					char *str;
									
                    IIntuition->GetAttrs( MainObj[GAD_AREXX_STRING], STRINGA_TextVal, &str, TAG_DONE );
					strcpy (arexxcommandstring, str);

					arexxexecute.MethodID = AM_EXECUTE;
					arexxexecute.ape_CommandString = arexxcommandstring;
					arexxexecute.ape_PortName = "AREXXDB101";
					arexxexecute.ape_IO = (BPTR)NULL;

                    IIntuition->SetAttrs( MainObj[GAD_AREXX_STRING], STRINGA_TextVal, "", TAG_DONE );
					IIntuition->RefreshGadgets ((struct Gadget *)MainObj[GAD_AREXX_STRING], mainwin, NULL);

					console_printf(OUTPUT_AREXX, arexxcommandstring);
					IIntuition->IDoMethodA(arexx_obj, (Msg)&arexxexecute);
	
					//IIntuition->ActivateGadget((struct Gadget *)MainObj[GAD_AREXX_STRING], mainwin, NULL);
					ILayout->ActivateLayoutGadget((struct Gadget *)MainObj[GAD_TOPLAYOUT], mainwin, NULL, (uint32)MainObj[GAD_AREXX_STRING]);
					break;
				}
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
