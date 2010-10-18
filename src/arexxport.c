;/* execute me to compile
gcc -o ARexxExample ARexxExample.c -lauto
quit
*/

#include <classes/window.h>
#include <classes/arexx.h>
#include <gadgets/layout.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/arexx.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>


#include "arexxport.h"
#include "suspend.h"
#include "gui.h"

/* ARexx command IDs. */
enum
{
	REXX_NAME,
	REXX_PLAY,
	REXX_PAUSE,
	REXX_KILL,
	REXX_LOAD,
	REXX_ATTACH
};


Object *arexx_obj;
BOOL arexx_port_is_open = FALSE;


/* Protos for the reply hook and ARexx command functions. */
STATIC VOID reply_callback(struct Hook *, Object *, struct RexxMsg *);
STATIC VOID rexx_Name (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Play (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Pause(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Kill (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Load (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Attach (struct ARexxCmd *, struct RexxMsg *);

/* Our reply hook function. */
STATIC struct Hook reply_hook;

/* The following commands are valid for this demo. */
/* This array must never be const because arexx.class writes into it. */
STATIC struct ARexxCmd Commands[] =
{
	{"NAME",	REXX_NAME,		rexx_Name,		NULL,       	0, NULL, 0, 0, NULL},
	{"PLAY",	REXX_PLAY,		rexx_Play,		NULL,       	0, NULL, 0, 0, NULL},
	{"PAUSE",	REXX_PAUSE,		rexx_Pause,		NULL,       	0, NULL, 0, 0, NULL},
	{"KILL",	REXX_KILL,		rexx_Kill,		NULL,       	0, NULL, 0, 0, NULL},
	{"LOAD",	REXX_LOAD,		rexx_Load,		"FILENAME/F",	0, NULL, 0, 0, NULL},
	{"ATTACH",	REXX_ATTACH,	rexx_Attach,	"PROCESS/F",	0, NULL, 0, 0, NULL},
	{NULL,      0,				NULL,			NULL,			0, NULL, 0, 0, NULL}
};


uint32 arexx_obtain_signal()
{
	uint32 signal = 0x0;
	IIntuition->GetAttr(AREXX_SigMask, arexx_obj, &signal);

	return signal;
}


void arexx_open_port()
{

	/* Create host object. */
	arexx_obj = IIntuition->NewObject(NULL, "arexx.class",
		AREXX_HostName, "AREXXDB101",
		AREXX_Commands, Commands,
		AREXX_NoSlot, TRUE,
		AREXX_ReplyHook, &reply_hook,
	TAG_DONE);

	if (arexx_obj)
	{
		arexx_port_is_open = TRUE;


		/* Setup the reply callback hook. */
		reply_hook.h_Entry    = (HOOKFUNC)reply_callback;
		reply_hook.h_SubEntry = NULL;
		reply_hook.h_Data     = NULL;
	}
}



void arexx_event_handler()
{
	IIntuition->IDoMethod(arexx_obj, AM_HANDLEEVENT);
}


void arexx_close_port()
{
	if (arexx_port_is_open)
		IIntuition->DisposeObject(arexx_obj);
}

/* This function gets called whenever we get an ARexx reply. In this example,
* we will see a reply come back from the REXX server when it has finished
* attempting to start the Demo.rexx macro.
*/
STATIC VOID reply_callback (struct Hook *hook UNUSED, Object *o UNUSED, struct RexxMsg *rxm)
{
	IDOS->Printf("Args[0]: %s\nResult1: %ld Result2: %ld\n",
	rxm->rm_Args[0], rxm->rm_Result1, rxm->rm_Result2);
}

/* NAME */
STATIC VOID rexx_Name (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	/* return the program name. */
	ac->ac_Result = "ARexxTest";
}

STATIC VOID rexx_Play (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	main_play();
	ac->ac_Result = "OK";
}


STATIC VOID rexx_Pause (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	main_pause();
	ac->ac_Result = "OK";
}

STATIC VOID rexx_Kill (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	main_kill();
	ac->ac_Result = "OK";
}

STATIC VOID rexx_Load (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	/* Print some text */
	if (ac->ac_ArgList[0])
	{
		main_load ((char*)ac->ac_ArgList[0], "", "");
	}
}

STATIC VOID rexx_Attach (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	/* return the compilation date. */
	ac->ac_Result = "11-10-95";

}
