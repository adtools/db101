#include <exec/types.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/button.h>
#include <proto/string.h>
#include <proto/layout.h>
#include <proto/label.h>
#include <proto/clicktab.h>
#include <proto/radiobutton.h>
#include <proto/checkbox.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <images/label.h>
#include <gadgets/space.h>
#include <gadgets/string.h>
#include <gadgets/clicktab.h>
#include <gadgets/radiobutton.h>
#include <gadgets/checkbox.h>

#include <reaction/reaction_macros.h>

#include <stdio.h>
#include <string.h>

#include "console.h"
#include "preferences.h"

enum
{
	PREF_WINDOW,
	PREF_LOAD_AT_ENTRY_RADIO,
	PREF_IMPORT_EXTERNALS_RADIO,
	PREF_KEEP_ELF_OPEN_BOX,
	
	PREF_ALLOW_KERNEL_ACCESS_BOX,
	PREF_SUSPEND_WHEN_ATTACH_BOX,
	PREF_SHOW_MAIN_AT_ENTRY_BOX,
	PREF_AUTO_LOAD_GLOBALS_BOX,
	
	PREF_SAVE_BUTTON,
	PREF_USE_BUTTON,
	PREF_CANCEL_BUTTON,
	
	PREF_NUM
};

Object *PrefObj[PREF_NUM];
struct Window *prefswin = NULL;
extern struct Window *mainwin;

struct preferences db101_prefs;

char *prefstag = "dbpr";

void preferences_set_defaults()
{
	db101_prefs.prefs_load_at_entry = ENTRY_LOAD_SOURCELIST;
	db101_prefs.prefs_import_externals = EXT_ASK;
	db101_prefs.prefs_allow_kernel_access = FALSE;
	db101_prefs.prefs_keep_elf_open = FALSE;
	db101_prefs.prefs_suspend_when_attach = TRUE;
	db101_prefs.prefs_show_main_at_entry = TRUE;
	db101_prefs.prefs_auto_load_globals = FALSE;
}

void load_preferences()
{
	BPTR fh = IDOS->Open("db101.prefs", MODE_OLDFILE);
	if(!fh)
	{
		//no prefs file
		preferences_set_defaults();
		return;
	}
	uint32 buffer;
	int read = IDOS->Read(fh, (APTR)&buffer, 4);
	if(read != 4 || buffer != *(uint32 *)prefstag)
	{
		console_printf(OUTPUT_WARNING, "Prefs file has bad format! Deleting...");
		IDOS->Close(fh);
		IDOS->DeleteFile("db101.prefs");
		preferences_set_defaults();
		return;
	}
	read = IDOS->Read(fh, (APTR)&db101_prefs, sizeof(struct preferences));
	if(read != sizeof(struct preferences))
	{
		console_printf(OUTPUT_WARNING, "Prefs file has bad format! Deleting...");
		IDOS->Close(fh);
		IDOS->DeleteFile("db101.prefs");
		preferences_set_defaults();
		return;
	}
	//should maybe check, that the entries are valid...
	IDOS->Close(fh);
}

void save_preferences()
{
	BPTR fh = IDOS->Open("db101.prefs", MODE_NEWFILE);
	if(!fh)
	{
		console_printf(OUTPUT_WARNING, "Failed to open prefs file for output, aborting...");
		return;
	}
	IDOS->Write(fh, prefstag, sizeof(prefstag));
	IDOS->Write(fh, (APTR)&db101_prefs, sizeof(db101_prefs));
	IDOS->Close(fh);
}

STRPTR prefs_pagelabels[] = {"Stabs", "System", NULL};
CONST_STRPTR entry_labels[] = {"Load all","Load Sourcelist","Load nothing",NULL};
CONST_STRPTR import_labels[] = {"Always","Ask","Never",NULL};

void preferences_open_window()
{
	if(prefswin)
		return;
	
    if(( PrefObj[PREF_WINDOW] = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Preferences",
            //WA_Width, 600,
            //WA_Height, 400,
            WA_DepthGadget, TRUE,
            WA_SizeGadget, TRUE,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_Activate, TRUE,
            WINDOW_Position, WPOS_CENTERSCREEN,
            
            WINDOW_GadgetHelp, TRUE,
            WINDOW_ParentGroup, VLayoutObject,
            	LAYOUT_AddChild, ClickTabObject,
					GA_Text,	prefs_pagelabels,
					CLICKTAB_Current,	0,
					CLICKTAB_PageGroup,	PageObject,
						PAGE_Add,		VLayoutObject,
			                LAYOUT_SpaceOuter, TRUE,
        			        LAYOUT_DeferLayout, TRUE,
        			        
        			        LAYOUT_AddChild, HLayoutObject,
        			        	LAYOUT_BevelStyle, BVS_GROUP,
        			        	LAYOUT_Label, "Load stabs at entry:",
								LAYOUT_AddChild, PrefObj[PREF_LOAD_AT_ENTRY_RADIO] = RadioButtonObject,
									GA_ID, PREF_LOAD_AT_ENTRY_RADIO,
									GA_Text,	entry_labels,
									RADIOBUTTON_Selected, db101_prefs.prefs_load_at_entry,
								RadioButtonEnd,
							EndMember,
							
							LAYOUT_AddChild, HLayoutObject,
								LAYOUT_BevelStyle, BVS_GROUP,
								LAYOUT_Label, "Import external modules:",
								LAYOUT_AddChild, PrefObj[PREF_IMPORT_EXTERNALS_RADIO] = RadioButtonObject,
									GA_ID, PREF_IMPORT_EXTERNALS_RADIO,
									GA_Text,	import_labels,
									RADIOBUTTON_Selected, db101_prefs.prefs_import_externals,
								RadioButtonEnd,
							EndMember,
							
							LAYOUT_AddChild, PrefObj[PREF_KEEP_ELF_OPEN_BOX] = CheckBoxObject,
        			        	GA_ID, PREF_KEEP_ELF_OPEN_BOX,
        			        	GA_Text, "Keep elf handles open",
        			        	CHECKBOX_Checked, db101_prefs.prefs_keep_elf_open,
        			        CheckBoxEnd,
						EndMember,
						
						PAGE_Add,		VLayoutObject,
			                LAYOUT_SpaceOuter, TRUE,
        			        LAYOUT_DeferLayout, TRUE,
        			        
        			        LAYOUT_AddChild, PrefObj[PREF_ALLOW_KERNEL_ACCESS_BOX] = CheckBoxObject,
        			        	GA_ID, PREF_ALLOW_KERNEL_ACCESS_BOX,
        			        	GA_Text, "Allow access to Kernel space",
        			        	CHECKBOX_Checked, db101_prefs.prefs_allow_kernel_access,
        			        CheckBoxEnd,
        			        
        			       	LAYOUT_AddChild, PrefObj[PREF_SUSPEND_WHEN_ATTACH_BOX] = CheckBoxObject,
        			        	GA_ID, PREF_SUSPEND_WHEN_ATTACH_BOX,
        			        	GA_Text, "Suspend when attaching to process",
        			        	CHECKBOX_Checked, db101_prefs.prefs_suspend_when_attach,
        			        CheckBoxEnd,
        			        
        			        LAYOUT_AddChild, PrefObj[PREF_SHOW_MAIN_AT_ENTRY_BOX] = CheckBoxObject,
        			        	GA_ID, PREF_SHOW_MAIN_AT_ENTRY_BOX,
        			        	GA_Text, "Show main function after loading",
        			        	CHECKBOX_Checked, db101_prefs.prefs_show_main_at_entry,
        			        CheckBoxEnd,
        			        
        			        LAYOUT_AddChild, PrefObj[PREF_AUTO_LOAD_GLOBALS_BOX] = CheckBoxObject,
        			        	GA_ID, PREF_AUTO_LOAD_GLOBALS_BOX,
        			        	GA_Text, "Automatically load globals",
        			        	CHECKBOX_Checked, db101_prefs.prefs_auto_load_globals,
        			        CheckBoxEnd,
        			    EndMember,
					PageEnd,
				ClickTabEnd,
					
				LAYOUT_AddChild, HLayoutObject,
                	LAYOUT_AddChild, PrefObj[PREF_SAVE_BUTTON] = ButtonObject,
                	    GA_ID, PREF_SAVE_BUTTON,
						GA_RelVerify, TRUE,
						GA_Text, "Save",
					ButtonEnd,
                	LAYOUT_AddChild, PrefObj[PREF_USE_BUTTON] = ButtonObject,
                	    GA_ID, PREF_USE_BUTTON,
						GA_RelVerify, TRUE,
						GA_Text, "Use",
					ButtonEnd,
                	LAYOUT_AddChild, PrefObj[PREF_CANCEL_BUTTON] = ButtonObject,
                	    GA_ID, PREF_CANCEL_BUTTON,
						GA_RelVerify, TRUE,
						GA_Text, "Cancel",
					ButtonEnd,
				EndMember,
                CHILD_WeightedHeight, 0,
			EndGroup,
		EndWindow))
	{
		if(prefswin = (struct Window *)RA_OpenWindow(PrefObj[PREF_WINDOW]))
		{
			IIntuition->SetWindowAttr(mainwin, WA_BusyPointer, (APTR)TRUE, 0L);
			preferences_event_loop();
			preferences_close_window();
			IIntuition->SetWindowAttr(mainwin, WA_BusyPointer, (APTR)FALSE, 0L);
		}
	}
}

void update_preferences()
{
	IIntuition->GetAttr(RADIOBUTTON_Selected, PrefObj[PREF_LOAD_AT_ENTRY_RADIO], (APTR)&db101_prefs.prefs_load_at_entry);
	IIntuition->GetAttr(RADIOBUTTON_Selected, PrefObj[PREF_IMPORT_EXTERNALS_RADIO], (APTR)&db101_prefs.prefs_import_externals);
	IIntuition->GetAttr(CHECKBOX_Checked, PrefObj[PREF_ALLOW_KERNEL_ACCESS_BOX], (APTR)&db101_prefs.prefs_allow_kernel_access);
	IIntuition->GetAttr(CHECKBOX_Checked, PrefObj[PREF_KEEP_ELF_OPEN_BOX], (APTR)&db101_prefs.prefs_keep_elf_open);
	IIntuition->GetAttr(CHECKBOX_Checked, PrefObj[PREF_SUSPEND_WHEN_ATTACH_BOX], (APTR)&db101_prefs.prefs_suspend_when_attach);
	IIntuition->GetAttr(CHECKBOX_Checked, PrefObj[PREF_SHOW_MAIN_AT_ENTRY_BOX], (APTR)&db101_prefs.prefs_show_main_at_entry);
	IIntuition->GetAttr(CHECKBOX_Checked, PrefObj[PREF_AUTO_LOAD_GLOBALS_BOX], (APTR)&db101_prefs.prefs_auto_load_globals);
}

void preferences_event_loop()
{
	ULONG wait, signal;
	BOOL done = FALSE;

	/* Obtain the window wait signal mask. */
	IIntuition->GetAttr( WINDOW_SigMask, PrefObj[PREF_WINDOW], &signal );
            
	/* Input Event Loop */
	while( !done )
	{
		wait = IExec->Wait(signal|SIGBREAKF_CTRL_C);
		if(wait & SIGBREAKF_CTRL_C) done = TRUE;

		if(wait & signal)
		{
			ULONG result;
			WORD Code;

			while ((result = RA_HandleInput(PrefObj[PREF_WINDOW], &Code)) != WMHI_LASTMSG && done != TRUE)
			{
				switch(result & WMHI_CLASSMASK)
				{
					case WMHI_CLOSEWINDOW:
						done = TRUE;
						break;

					case WMHI_GADGETUP:
						switch(result & WMHI_GADGETMASK)
						{
							case PREF_SAVE_BUTTON:
								update_preferences();
								save_preferences();
								done = TRUE;
								break;
								
							case PREF_USE_BUTTON:
								update_preferences();
								done = TRUE;
								break;
							
							case PREF_CANCEL_BUTTON:
								done = TRUE;
								break;
						}
				}
			}
		}
	}
}
							
							
void preferences_close_window()
{
	if(prefswin)
	{
		IIntuition->DisposeObject(PrefObj[PREF_WINDOW]);
		prefswin = 0;
	}
}

