#include <exec/types.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/button.h>
#include <proto/label.h>
#include <proto/layout.h>
#include <proto/string.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/string.h>
#include <images/label.h>

#include <stdio.h>

#include <reaction/reaction_macros.h>

#include "importwindow.h"
#include "breakpoints.h"
#include "console.h"
#include "sourcelist.h"

enum
{
	IMPW_ADDRESS_STRING = 0,
	IMPW_IMPORT_BUTTON,
	
	IMPW_NUM
};

Object *ImpWinObj;
Object *ImpObj[IMPW_NUM];
struct Window *impwin = NULL;
BOOL import_window_is_open = FALSE;

char imp_address_buffer[10] = "";

void import_open_window()
{
	if(impwin)
		return;
		
	if(( ImpWinObj = WindowObject,
    	WA_ScreenTitle, "Debug 101",
        WA_Title, "Import external module",
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_Activate, TRUE,
        WINDOW_Position, WPOS_CENTERSCREEN,

        WINDOW_GadgetHelp, TRUE,
        WINDOW_ParentGroup, HLayoutObject,
            LAYOUT_SpaceOuter, TRUE,
            LAYOUT_DeferLayout, TRUE,
            
            LAYOUT_AddChild, ImpObj[IMPW_ADDRESS_STRING] = StringObject,
				GA_ID,				IMPW_ADDRESS_STRING,
				GA_RelVerify,		TRUE,
				GA_TabCycle,		TRUE,
				STRINGA_MinVisible,	9,
				STRINGA_MaxChars,	9,
				STRINGA_Buffer,		imp_address_buffer,
				STRINGA_HookType,	SHK_HEXADECIMAL,
			StringEnd,
			CHILD_NominalSize,		TRUE,
			CHILD_Label, LabelObject, LABEL_Text, "Address:", LabelEnd,

			LAYOUT_AddChild, ImpObj[IMPW_IMPORT_BUTTON] = ButtonObject,
				GA_ID,			IMPW_IMPORT_BUTTON,
				GA_RelVerify,	TRUE,
				GA_Text,		"Import!",
			ButtonEnd,
			CHILD_NominalSize,	TRUE,
		EndMember,
	EndWindow))
	{
		if(impwin = (struct Window *) RA_OpenWindow(ImpWinObj))
		{
			import_window_is_open = TRUE;
		}
	}
}

uint32 import_obtain_window_signal()
{
	uint32 signal = 0x0;
	if(import_window_is_open)
		IIntuition->GetAttr(WINDOW_SigMask, ImpWinObj, &signal);
	return signal;
}

void import_event_handler()
{
	ULONG result, done = FALSE;
	WORD Code;
	
	while((result = RA_HandleInput(ImpWinObj, &Code)) != WMHI_LASTMSG)
	{
		switch(result & WMHI_CLASSMASK)
		{
			case WMHI_CLOSEWINDOW:
				done = TRUE;
				break;
			case WMHI_GADGETUP:
				switch(result & WMHI_GADGETMASK)
				{
					case IMPW_ADDRESS_STRING:
					case IMPW_IMPORT_BUTTON:
					{
						char astring[11];
						uint32 address;
						
						sprintf(astring, "0x%s",imp_address_buffer); 
						address= strtol(astring, astring+11, 16);
						
						switch(try_import_segment(address))
						{
							case -3:
								console_printf(OUTPUT_WARNING, "Address is not associated with a seglist");
								break;
							case -2:
								console_printf(OUTPUT_WARNING, "Segment is already loaded!");
								break;
							case TRUE:
								console_printf(OUTPUT_SYSTEM, "Successfully imported new segment!");
								sourcelist_update();
								break;
							case FALSE:
								console_printf(OUTPUT_SYSTEM, "Failed to import new segment!");
								break;
						}
						done = TRUE;
					}
						break;
				}
				break;
		}
		if(done)
		{
			import_close_window();
			break;
		}
	}
}

void import_close_window()
{
	if(import_window_is_open)
	{
		IIntuition->DisposeObject(ImpWinObj);
		import_window_is_open = FALSE;
	}
}
