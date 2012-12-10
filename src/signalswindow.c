/* signalswindow.c */

#include <exec/types.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/button.h>
#include <proto/label.h>
#include <proto/layout.h>

#include <classes/window.h>
#include <gadgets/button.h>

#include <graphics/text.h>

#include <reaction/reaction_macros.h>

#include "signalswindow.h"
#include "suspend.h"

enum
{
	SIGW_ABORT_BUTTON = 0,
	SIGW_CHILD_BUTTON,
	SIGW_2_BUTTON,
	SIGW_3_BUTTON,
	SIGW_SINGLE_BUTTON,
	SIGW_INTUITION_BUTTON,
	SIGW_6_BUTTON,
	SIGW_NET_BUTTON,
	SIGW_DOS_BUTTON,
	SIGW_9_BUTTON,
	SIGW_10_BUTTON,
	SIGW_11_BUTTON,
	SIGW_CTRL_C_BUTTON,
	SIGW_CTRL_D_BUTTON,
	SIGW_CTRL_E_BUTTON,
	SIGW_CTRL_F_BUTTON,
	
	SIGW_16_BUTTON,
	SIGW_17_BUTTON,
	SIGW_18_BUTTON,
	SIGW_19_BUTTON,
	SIGW_20_BUTTON,
	SIGW_21_BUTTON,
	SIGW_22_BUTTON,
	SIGW_23_BUTTON,
	SIGW_24_BUTTON,
	SIGW_25_BUTTON,
	SIGW_26_BUTTON,
	SIGW_27_BUTTON,
	SIGW_28_BUTTON,
	SIGW_29_BUTTON,
	SIGW_30_BUTTON,
	SIGW_31_BUTTON,
	
	SIGW_SIGNAL_BUTTON,
	SIGW_CANCEL_BUTTON,
	
	SIGW_NUM
};

const char *sigw_strings[] =
{
	"Abort",
	"Child",
	"2",
	"3",
	"Single",
	"Intuition",
	"6",
	"Net",
	"DOS",
	"9",
	"10",
	"11",
	"CTRL-C",
	"CTRL-D",
	"CTRL-E",
	"CTRL-F",
	
	"16",
	"17",
	"18",
	"19",
	"20",
	"21",
	"22",
	"23",
	"24",
	"25",
	"26",
	"27",
	"28",
	"29",
	"30",
	"31",
};

Object *SigWinObj;
Object *SigObj[SIGW_NUM];
struct Window *sigwin = NULL;

Object *create_sys_buttons()
{
	int i;
	for(i = 0; i < 16; i++)
		SigObj[i] = ButtonObject,
			GA_ID, i,
			GA_RelVerify, TRUE,
			BUTTON_PushButton, TRUE,
			GA_Text, sigw_strings[i],
		ButtonEnd;
	Object *o = VLayoutObject,
		LAYOUT_BevelStyle,	BVS_GROUP,
		LAYOUT_Label,	"System signals",
		LAYOUT_AddChild,	HLayoutObject,
			LAYOUT_AddChild,	SigObj[0],
			LAYOUT_AddChild,	SigObj[1],
			LAYOUT_AddChild,	SigObj[2],
			LAYOUT_AddChild,	SigObj[3],
			LAYOUT_AddChild,	SigObj[4],
			LAYOUT_AddChild,	SigObj[5],
			LAYOUT_AddChild,	SigObj[6],
			LAYOUT_AddChild,	SigObj[7],
		EndMember,
		LAYOUT_AddChild,	HLayoutObject,
			LAYOUT_AddChild,	SigObj[8],
			LAYOUT_AddChild,	SigObj[9],
			LAYOUT_AddChild,	SigObj[10],
			LAYOUT_AddChild,	SigObj[11],
			LAYOUT_AddChild,	SigObj[12],
			LAYOUT_AddChild,	SigObj[13],
			LAYOUT_AddChild,	SigObj[14],
			LAYOUT_AddChild,	SigObj[15],
		EndMember,
	EndMember;
	return o;
}

Object *create_user_buttons()
{
	int i;
	for(i = 16; i < 32; i++)
		SigObj[i] = ButtonObject,
			GA_ID, i,
			GA_RelVerify, TRUE,
			BUTTON_PushButton, TRUE,
			GA_Text, sigw_strings[i],
		ButtonEnd;
	Object *o = VLayoutObject,
		LAYOUT_BevelStyle,	BVS_GROUP,
		LAYOUT_Label,	"User signals",
		LAYOUT_AddChild,	HLayoutObject,
			LAYOUT_AddChild,	SigObj[16],
			LAYOUT_AddChild,	SigObj[17],
			LAYOUT_AddChild,	SigObj[18],
			LAYOUT_AddChild,	SigObj[19],
			LAYOUT_AddChild,	SigObj[20],
			LAYOUT_AddChild,	SigObj[21],
			LAYOUT_AddChild,	SigObj[22],
			LAYOUT_AddChild,	SigObj[23],
		EndMember,
		LAYOUT_AddChild,	HLayoutObject,
			LAYOUT_AddChild,	SigObj[24],
			LAYOUT_AddChild,	SigObj[25],
			LAYOUT_AddChild,	SigObj[26],
			LAYOUT_AddChild,	SigObj[27],
			LAYOUT_AddChild,	SigObj[28],
			LAYOUT_AddChild,	SigObj[29],
			LAYOUT_AddChild,	SigObj[30],
			LAYOUT_AddChild,	SigObj[31],
		EndMember,
	EndMember;
	return o;
}		

void sigwin_update()
{
	if(!task_exists)
		return;
	uint32 sigmask = process->pr_Task.tc_SigWait;
	int i;
	for(i = 0; i < 32; i++)
	{
		if((1 << i) & sigmask)
			IIntuition->SetAttrs(SigObj[i], BUTTON_SoftStyle, FSF_BOLD, TAG_DONE);
		else
			IIntuition->SetAttrs(SigObj[i], BUTTON_SoftStyle, FS_NORMAL, TAG_DONE);
		IIntuition->RefreshGadgets((struct Gadget *)SigObj[i], sigwin, NULL);
	}
}

void sigwin_send_signal()
{
	if(!task_exists)
		return;
	uint32 sigmask = 0x0;
	int i;
	for(i = 0; i < 32; i++)
	{
		uint32 selected;
		IIntuition->GetAttr(GA_Selected, SigObj[i], &selected);
		sigmask |= (selected ? (1 << i) : 0);
	}
	IExec->Signal((struct Task *)process, sigmask);
}

void sigwin_open_window()
{
    if (sigwin)
        return;

    if(( SigWinObj = WindowObject,
    	WA_ScreenTitle, "Debug 101",
        WA_Title, "Signals",
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
            LAYOUT_SpaceOuter, TRUE,
            LAYOUT_DeferLayout, TRUE,
                
            LAYOUT_AddChild, create_sys_buttons(),
            LAYOUT_AddChild, create_user_buttons(),
                
            LAYOUT_AddChild, HLayoutObject,
             	LAYOUT_AddChild, SigObj[SIGW_SIGNAL_BUTTON] = ButtonObject,
               		GA_ID, SIGW_SIGNAL_BUTTON,
                   	GA_RelVerify, TRUE,
                   	GA_Text, "Signal",
               	ButtonEnd,
               	CHILD_WeightedHeight, 0,
                	
               	LAYOUT_AddChild, SigObj[SIGW_CANCEL_BUTTON] = ButtonObject,
               		GA_ID, SIGW_CANCEL_BUTTON,
                   	GA_RelVerify, TRUE,
                   	GA_Text, "Cancel",
               	ButtonEnd,
               	CHILD_WeightedHeight, 0,
            EndMember,
		EndGroup,
	EndWindow))
	{
		sigwin = (struct Window *)RA_OpenWindow(SigWinObj);
		sigwin_update();
	}
}

uint32 sigwin_obtain_signal()
{
    uint32 signal = 0x0;
    if (sigwin)
        IIntuition->GetAttr( WINDOW_SigMask, SigWinObj, &signal );
        
    return signal;
}

void sigwin_event_handler()
{
    ULONG wait, signal, result, relevent;
    BOOL done = FALSE;
    WORD Code;

    while ((result = RA_HandleInput(SigWinObj, &Code)) != WMHI_LASTMSG)
    {
        switch (result & WMHI_CLASSMASK)
        {
            case WMHI_CLOSEWINDOW:
                done = TRUE;
                break;

             case WMHI_GADGETUP:
                switch(result & WMHI_GADGETMASK)
                {
					case SIGW_CANCEL_BUTTON:
						done = TRUE;
						break;
					
					case SIGW_SIGNAL_BUTTON:
						sigwin_send_signal();
						done = TRUE;
						break;
				}
		}
        if( done )
        {
            sigwin_close_window();
            break;
        }
	}
}
					
void sigwin_close_window()
{
    if (sigwin)
    {
        IIntuition->DisposeObject( SigWinObj );
        sigwin = NULL;
    }
}
