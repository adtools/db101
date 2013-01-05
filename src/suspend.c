/* suspend.c		Example of use of the exec debug interface
					by Alfkil Wennermark 2010

					Thanks to Steven Solie, Thomas Frieden and others

					This code is partially copied from Thomas' GDB source
					*/


#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/elf.h>
#include <proto/intuition.h>
#include <proto/requester.h>

#include <classes/requester.h>

#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/tasks.h>
#include <exec/ports.h>

#include <dos/dos.h>

#include <libraries/elf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "symbols.h"
#include "gui.h"
#include "suspend.h"
#include "breakpoints.h"
#include "console.h"
#include "pipe.h"
#include "setbreak.h"
#include "elf.h"
#include "preferences.h"

#define dprintf(format...)	IExec->DebugPrintF(format)

struct DebugIFace *IDebug = 0;
struct MMUIFace *IMMU = 0;

struct KernelDebugMessage
{
	uint32 type;
	union
	{
		struct ExceptionContext *context;
		struct Library *library;
	} message;
};

static ULONG amigaos_debug_callback(struct Hook *, struct Task *, struct KernelDebugMessage *);

struct Hook debug_hook;
struct Process *process = NULL, *me = NULL;

BPTR exec_seglist;
ULONG debug_data = 1234;

Elf32_Handle exec_elfhandle;
Elf32_Addr code_elf_addr;
int code_size;
void *code_relocated_addr;

extern BPTR outpipe[2];

struct Elf32_SymbolQuery query;

struct ExceptionContext *context_ptr, context_copy;

BYTE debug_signal;
ULONG debug_sigfield;

BOOL task_exists = FALSE;
BOOL task_playing = FALSE;
BOOL wants_to_crash = FALSE;
BOOL should_continue = FALSE;
BOOL catch_sline = FALSE;
BOOL stepping_over = FALSE;
BOOL stepping_out = FALSE;

struct stab_function *stepover_func = NULL;

void init()
{
    IDebug = (struct DebugIFace *)IExec->GetInterface((struct Library *)SysBase, "debug", 1, 0);
    if (!IDebug)
	{
		printf("Can't get DEBUG access\n");
		exit(RETURN_FAIL);
	}
    IMMU = (struct MMUIFace *)IExec->GetInterface((struct Library *)SysBase, "mmu", 1, 0);
    if (!IMMU)
	{
		IExec->DropInterface((struct Interface *)IDebug);
		console_printf(OUTPUT_WARNING, "Can't get MMU access\n");
		exit(RETURN_FAIL);
	}

	debug_signal = IExec->AllocSignal (-1);
	debug_sigfield = 1 << debug_signal;

	me = (struct Process *)IExec->FindTask(NULL);
}

void end()
{
    if (IDebug)		IExec->DropInterface((struct Interface *)IDebug);
	IDebug = NULL;

    if (IMMU)		IExec->DropInterface((struct Interface *)IMMU);
    IMMU = 0;

	IExec->FreeSignal (debug_signal);
}

void attach_hook()
{	
    debug_hook.h_Entry = (ULONG (*)())amigaos_debug_callback;
    debug_hook.h_Data =  (APTR)&debug_data;

	if(task_exists)
		IDebug->AddDebugHook((struct Task *)process, &debug_hook);
}

void remove_hook()
{
	if(task_exists)
		IDebug->AddDebugHook((struct Task*)process, NULL);
}

BOOL 
attach (struct Process *aprocess)
{
    /* Can only debug processes right now */
    if (aprocess->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
		console_printf (OUTPUT_WARNING, "Not a process!");
		return FALSE;
    }

    /* Get the seglist and elf handle of the process */
    exec_seglist = IDOS->GetProcSegList (aprocess, GPSLF_SEG|GPSLF_RUN);
  
    if (!exec_seglist)
    {
		console_printf(OUTPUT_WARNING, "Can't find seglist!\n");
		return FALSE;
    }

	process  = aprocess;

	BOOL suspend = db101_prefs.prefs_suspend_when_attach;
    /* Suspend the task */
    //dprintf("Process state: %d\n",pProcess->pr_Task.tc_State);
    if(suspend)
    	if (process->pr_Task.tc_State == TS_READY || process->pr_Task.tc_State == TS_WAIT)
    	{
			console_printf(OUTPUT_SYSTEM, "Suspending task %p", process);
			IExec->SuspendTask((struct Task *)process, 0);
			IExec->Signal((struct Task *)me, debug_sigfield);
    	}

	task_exists = TRUE;
	task_playing = FALSE;
    isattached = TRUE;
    
    if(process->pr_Task.tc_State == TS_CRASHED)
    {
    	process->pr_Task.tc_State = TS_SUSPENDED;
    }

	attach_hook();

	IDebug->ReadTaskContext((struct Task *)process, &context_copy, RTCF_ALL);

	IDOS->GetSegListInfoTags(exec_seglist, 
							 GSLI_ElfHandle, &exec_elfhandle,
							 TAG_DONE);
	return TRUE;
}

void detach()
{
	if(!task_playing)
		play();
	
	remove_hook();
	task_exists = FALSE;
	task_playing = FALSE;
	isattached = FALSE;
}

int load_inferior(char *filename, char *path, char *args)
{
//	struct Task *me = IExec->FindTask(NULL);
	int ret = 0;

	char fullpath[1024];

	console_printf(OUTPUT_SYSTEM, "Attempting to load %s in %s", filename, path);
	BPTR lock = IDOS->Lock (path, SHARED_LOCK);
	if (!lock)
	{
		console_printf(OUTPUT_WARNING, "Coudln't get currentdir lock");
		return -1;
	}
	BPTR homelock = IDOS->DupLock (lock);

	if (strlen (filename) == 0)
		return 0;
	if (strlen (path) == 0)
		strcpy (fullpath, filename);
	else if (path[strlen(path)-1] == '/' || path[strlen(path)-1] == ':')
		sprintf(fullpath, "%s%s", path, filename);
	else
		sprintf(fullpath, "%s/%s", path, filename);

    exec_seglist = IDOS->LoadSeg(fullpath);
	
    if (exec_seglist == 0L)
	{
		console_printf (OUTPUT_WARNING, "\"%s\": not an executable file", filename);
		IDOS->UnLock(lock);
		return -1;
	}

	//printf("Starting the new process:\n");


	IExec->Forbid();

	    process = IDOS->CreateNewProcTags(
				NP_Seglist,         exec_seglist,
//				NP_Entry,			foo,
				NP_FreeSeglist,     TRUE,
				NP_Name,            filename,
				NP_CurrentDir,		lock,
				NP_HomeDir,			homelock,
				NP_StackSize,		2000000,
				NP_Cli,             TRUE,
				NP_Child,			TRUE,
				NP_Arguments,       args,
				NP_Input,	    	IDOS->Input(),
				NP_CloseInput,      FALSE,
				NP_Output,          pipe_get_write_end(), //IDOS->Output(),
				NP_CloseOutput,     FALSE,
				NP_Error,           IDOS->ErrorOutput(),
				NP_CloseError,      FALSE,
				NP_NotifyOnDeathSigTask, me,
				TAG_DONE
			);

	if (!process)
	{
		IExec->Permit();
		task_exists = FALSE;
		console_printf(OUTPUT_WARNING, "Couldn't start new task!");
		return -1;
	}
	else
	{
		IExec->SuspendTask((struct Task *)process, 0L);
		task_exists = TRUE;
		task_playing = FALSE;
		attach_hook();
		IDebug->ReadTaskContext((struct Task *)process, &context_copy, RTCF_ALL);
		IExec->Permit();
		//doesn't work at the moment:
		//IExec->Signal((struct Task *)me, debug_sigfield);
		//printf("New task started...\n");
		ret = 0;
	}
	
	IDOS->GetSegListInfoTags(exec_seglist, 
							 GSLI_ElfHandle, &exec_elfhandle,
							 TAG_DONE);

	return ret;
}

BOOL play()
{
	if (task_exists)
	{
		UBYTE state = process->pr_Task.tc_State;
		if (state != TS_READY)
		{
			if(state == TS_CRASHED)
				process->pr_Task.tc_State = TS_READY;
			//IExec->Forbid();
			IDebug->WriteTaskContext((struct Task *)process, &context_copy, RTCF_ALL);
			IExec->RestartTask((struct Task *)process, 0L);
			if (process->pr_Task.tc_State == TS_SUSPENDED || process->pr_Task.tc_State == TS_CRASHED)
			{
				//IExec->Permit();
				console_printf(OUTPUT_WARNING, "Failed to restart task!");
				//IExec->RemTask (newtask);
				return FALSE;
			}
			else
			{
				//IExec->Permit();
				task_playing = TRUE;
				return TRUE;
			}
		}
	}
	return FALSE;
}

void read_task_context()
{
	IDebug->ReadTaskContext((struct Task *)process, &context_copy, RTCF_ALL);
}

void pause()
{
	if (task_exists)
		if (task_playing)
		{
			task_playing = FALSE;
			IExec->SuspendTask ((struct Task *) process, 0L);
			
			read_task_context();
			IExec->Signal((struct Task *)me, debug_sigfield);
		}
}

void step()
{
	if (!hasfunctioncontext)
		return;

	struct stab_function *f = current_function;

	catch_sline = TRUE;
	stepping_over = TRUE;
	stepover_func = f;
	asmstep();
}

void into()
{
	catch_sline = TRUE;
	stepping_over = FALSE;
	asmstep();
}

void out()
{
	stepping_out = TRUE;
	stepout_install();
	play();
}

void asmstep()
{
	if(task_exists)
	{
		if(task_playing)
			pause();

		IDebug->WriteTaskContext((struct Task *)process, &context_copy, RTCF_STATE|RTCF_GENERAL);
		asmstep_install();
		play();
	}
}

void asmstep_nobranch()
{
	if(!task_exists)
		return;

	IDebug->WriteTaskContext((struct Task *)process, &context_copy, RTCF_STATE|RTCF_GENERAL);
	asmstep_nobranch_install();
	play();
}

void asmskip()
{
	if (task_exists)
	{
		pause();

		context_copy.ip += 4;
		IDebug->WriteTaskContext((struct Task *)process, &context_copy, RTCF_STATE);
	}
}


void killtask()
{
	if (task_exists)
	{
		//it is unsafe to Remove a process in AmigaDOS
		//if we attached to the process, RemTask is going to crash...
		if (!isattached)
			IExec->RemTask ((struct Task *)process);
		else
			detach();

		task_exists = FALSE;
		task_playing = FALSE;
	}
}


void crash()
{
	wants_to_crash = TRUE;
	//how to do this ???
	//insert_breakpoint (context_copy.ip);
}


int debug_counter = 0;

ULONG
amigaos_debug_callback(struct Hook *hook, struct Task *currentTask, 
					   struct KernelDebugMessage *dbgmsg)
{
    struct ExecIFace *IExec = (struct ExecIFace *)((struct ExecBase *)SysBase)->MainInterface;

	uint32 *data = (uint32 *)hook->h_Data;
	uint32 traptype = 0;

	/* these are the 4 types of debug msgs: */
	switch (dbgmsg->type)
	{
		case DBHMT_REMTASK:
			*data = 9;
			if (currentTask == (struct Task *)process)
				IExec->Signal ((struct Task *)me, SIGF_CHILD);
			//task_exists = FALSE;
			break;

		case DBHMT_EXCEPTION:
			//*data = dbgmsg->message.context->ip;
			traptype = dbgmsg->message.context->Traptype;
			*data = traptype;
			if (traptype == 0x700 || traptype == 0xd00)
			{
			}
			context_ptr = dbgmsg->message.context;
			memcpy (&context_copy, context_ptr, sizeof(struct ExceptionContext));
			IExec->Signal ((struct Task *)me, debug_sigfield);
			return 1;

			break;

		case DBHMT_OPENLIB:
			*data = 13;
			break;

		case DBHMT_CLOSELIB:
			*data = 15;
			break;

		default:
			*data = 0;
			break;
	}
	/* returning 1 will suspend the task ! */
	return 0;
}

#define BIG_ENDIAN

asm (
"	.globl meth_start	\n"
"	.globl meth_end		\n"
"meth_start:			\n"
"	trap				\n"
"meth_end:				\n"
);


extern unsigned int meth_start, meth_end;




int memory_insert_breakpoint(uint32 addr, uint32 *buffer)
{
  uint32 oldAttr;
  APTR stack;

dprintf("Setting BP at 0x%x\n", addr);

  /* Write the breakpoint.  */
  if (1)
  {
  	/* Go supervisor */
	stack = IExec->SuperState();

  	/* Make sure to unprotect the memory area */
	oldAttr = IMMU->GetMemoryAttrs((APTR)addr, 0);
	IMMU->SetMemoryAttrs((APTR)addr, 4, MEMATTRF_READ_WRITE);

#if 1
		*buffer = *(uint32 *)addr;
		*(uint32 *)addr = meth_start;	//insert asm("trap")
		//dprintf("buffer: 0x%x meth_start: 0x%x addr:0x%x\n", *buffer, meth_start, addr);
#else
		uint32 realaddr = (uint32)IMMU->GetPhysicalAddress((APTR)addr);
		if(realaddr == 0x0)
			realaddr = addr;
		//dprintf("Setting bp at 0x%x\n", realaddr);
		*buffer = setbreak(realaddr, meth_start);
#endif

	/* Set old attributes again */
	IMMU->SetMemoryAttrs((APTR)addr, 4, oldAttr);
	
	/* Return to old state */
	if (stack)
		IExec->UserState(stack);
  }
  IExec->CacheClearU();

  return 0;
}

int memory_remove_breakpoint(uint32 addr, uint32 *buffer)
{
  uint32 oldAttr;
  APTR stack;

  /* Restore the memory contents.  */

  if (1)
  {
  	/* Go supervisor */
	stack = IExec->SuperState();

	IExec->Forbid();	
  	/* Make sure to unprotect the memory area */
	oldAttr = IMMU->GetMemoryAttrs((APTR)addr, 0);
	IMMU->SetMemoryAttrs((APTR)addr, 4, MEMATTRF_READ_WRITE);

#if 1
		*(uint32 *)addr = *buffer;	//restore old instruction
#else
		uint32 realaddr = (uint32)IMMU->GetPhysicalAddress((APTR)addr);
		if(realaddr == 0x0)
			realaddr = addr;
		setbreak(realaddr, *buffer);
#endif

	/* Set old attributes again */
	IMMU->SetMemoryAttrs((APTR)addr, 4, oldAttr);

	IExec->Permit();	
	/* Return to old state */
	if (stack)
		IExec->UserState(stack);
  }
  IExec->CacheClearU();

  return 0;
}
