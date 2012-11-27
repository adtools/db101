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

#include <stdio.h>

#include "arexxport.h"
#include "suspend.h"
#include "gui.h"
#include "breakpoints.h"
#include "console.h"

/* ARexx command IDs. */
enum
{
	REXX_HELP,
	REXX_NAME,
	REXX_PLAY,
	REXX_PAUSE,
	REXX_STEP,
	REXX_KILL,
	REXX_LOAD,
	REXX_ATTACH,
	REXX_QUIT,
	REXX_BREAKLINE,
	REXX_BREAK
};


Object *arexx_obj;
BOOL arexx_port_is_open = FALSE;


/* Protos for the reply hook and ARexx command functions. */
STATIC VOID reply_callback(struct Hook *, Object *, struct RexxMsg *);
STATIC VOID rexx_Help (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Name (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Play (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Pause(struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Step (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Kill (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Load (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Attach (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Quit (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Breakline (struct ARexxCmd *, struct RexxMsg *);
STATIC VOID rexx_Break (struct ARexxCmd *, struct RexxMsg *);


/* Our reply hook function. */
STATIC struct Hook reply_hook;

/* The following commands are valid for this demo. */
/* This array must never be const because arexx.class writes into it. */
STATIC struct ARexxCmd Commands[] =
{
	{"HELP",		REXX_HELP,		rexx_Help,		NULL,					0, NULL, 0, 0, NULL},
	{"NAME",		REXX_NAME,		rexx_Name,		NULL,       			0, NULL, 0, 0, NULL},
	{"PLAY",		REXX_PLAY,		rexx_Play,		NULL,       			0, NULL, 0, 0, NULL},
	{"PAUSE",		REXX_PAUSE,		rexx_Pause,		NULL,       			0, NULL, 0, 0, NULL},
	{"STEP",		REXX_STEP,		rexx_Step,		NULL,       			0, NULL, 0, 0, NULL},
	{"KILL",		REXX_KILL,		rexx_Kill,		NULL,       			0, NULL, 0, 0, NULL},
	{"LOAD",		REXX_LOAD,		rexx_Load,		"FILENAME/F",			0, NULL, 0, 0, NULL},
	{"ATTACH",		REXX_ATTACH,	rexx_Attach,	"PROCESS/F",			0, NULL, 0, 0, NULL},
	{"QUIT",		REXX_QUIT,		rexx_Quit,		NULL,       			0, NULL, 0, 0, NULL},
	{"BREAKLINE",	REXX_BREAKLINE,	rexx_Breakline,	"LINE/N/A,FILE/F/A",	0, NULL, 0, 0, NULL},
	{"BREAK",		REXX_BREAK,		rexx_Break,		"FUNCTION/F/A",			0, NULL, 0, 0, NULL},
	{NULL,      	0,				NULL,			NULL,					0, NULL, 0, 0, NULL}
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
	if(rxm->rm_Result1 == 20 && rxm->rm_Result2 == 0)
	{
		console_printf(OUTPUT_AREXX, "%s is NOT and Arexx command!", rxm->rm_Args[0]);
		console_printf(OUTPUT_AREXX, "For a list of known commands, type HELP");
	}
	else if(rxm->rm_Result1 == 1 && rxm->rm_Result2 == 0)
		console_printf(OUTPUT_AREXX, "Command %s was successful (r1=%ld, r2=%ld)", rxm->rm_Args[0], rxm->rm_Result1, rxm->rm_Result2);
}

STATIC VOID rexx_Help (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	console_printf(OUTPUT_AREXX, " ===== AREXX HELP ===== ");
	console_printf(OUTPUT_AREXX, " NAME: returns the name of db101");
	console_printf(OUTPUT_AREXX, " LOAD <filename>: load and executable file");
	console_printf(OUTPUT_AREXX, " PLAY: execute file from the current address");
	console_printf(OUTPUT_AREXX, " PAUSE: pause execution of current executable");
	console_printf(OUTPUT_AREXX, " STEP: execute the next line and stop");
	console_printf(OUTPUT_AREXX, " KILL: kill the executable (dangerous)");
	console_printf(OUTPUT_AREXX, " ATTACH <process>: try to attach debugger to a running process");
	console_printf(OUTPUT_AREXX, " QUIT: leave db101 (will kill current process)");
	console_printf(OUTPUT_AREXX, " BREAKLINE <line> <file>: will attempt to set up a breakpoint at a line in a sourcefile");
	console_printf(OUTPUT_AREXX, " BREAK <function>: will set up a breakpoint at the beginning of a function");
	console_printf(OUTPUT_AREXX, " ===== (DONE) ===== ");
}	
	
/* NAME */
STATIC VOID rexx_Name (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	/* return the program name. */
	ac->ac_Result = "Debug 101";
	console_printf(OUTPUT_AREXX, "Program name: db101");
	console_printf(OUTPUT_AREXX, "Arexx Port: AREXXDB101");
}

STATIC VOID rexx_Play (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	main_play();
	ac->ac_RC = TRUE;
}


STATIC VOID rexx_Pause (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	main_pause();
	ac->ac_RC = TRUE;
}

STATIC VOID rexx_Step (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	main_step();
	ac->ac_RC = TRUE;
}


STATIC VOID rexx_Kill (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	main_kill();
	ac->ac_RC = TRUE;
}

STATIC VOID rexx_Load (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	/* Print some text */
	if (ac->ac_ArgList[0])
	{
		main_load ((char*)ac->ac_ArgList[0], "", "");
	}
	ac->ac_RC = TRUE;
}

STATIC VOID rexx_Attach (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	printf("process = %s\n", (char *)ac->ac_ArgList[0]);
	//TODO: find process by name?
	struct Process *pr = (struct Process *)IExec->FindTask ((char *)ac->ac_ArgList[0]);
	if (!pr)
		ac->ac_RC = FALSE;
	else
	{
		ac->ac_RC = TRUE;
		main_attach (pr);
	}
}

STATIC VOID rexx_Quit (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	struct Task *me = IExec->FindTask (NULL);
	IExec->Signal (me, SIGBREAKF_CTRL_C);

	ac->ac_RC = TRUE;
}

STATIC VOID rexx_Breakline (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	printf("arg[0] = %d\n", *((uint32*)ac->ac_ArgList[0]));
	printf("arg[1] = %s\n", (char *)ac->ac_ArgList[1]);

	ac->ac_RC = breakpoint_line_in_file ( *((uint32*)ac->ac_ArgList[0]), (char *)ac->ac_ArgList[1]);
}

STATIC VOID rexx_Break (struct ARexxCmd *ac, struct RexxMsg *rxm UNUSED)
{
	console_printf(OUTPUT_AREXX, "arg[0] = %s\n", (char *)ac->ac_ArgList[0]);

	ac->ac_RC = breakpoint_function ((char *)ac->ac_ArgList[0]);
}
