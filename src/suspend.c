/* suspend.c		Example of use of the exec debug interface
					by Alfkil Wennermark 2010

					Thanks to Steven Solie, Thomas Frieden and others

					This code is partially copied from Thomas' GDB source
					*/


#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/elf.h>

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


int amigaos_relocate_elfhandle ()
{
    Elf32_Handle hElf = (Elf32_Handle)exec_elfhandle;
    Elf32_Shdr *pHeader;
    int i;
    uint32 nSecs;
    uint32 strtblidx; /* for debugging */
    char *strtbl; /* for debugging */

    struct TagItem tags[] =
		{
			{GST_SectionIndex,     0},
			{GST_Load,             TRUE},
			{TAG_DONE,             0}
		};

    /* Get number of sections in ELF file and the string
     * table index. The latter is used only for debugging
     * purposes. */
    if ((2 != IElf->GetElfAttrsTags(hElf,
    		EAT_NumSections, &nSecs,
    		EAT_SectionStringTable, &strtblidx,
    		TAG_DONE)))
    {
    	console_printf(OUTPUT_WARNING,"Cannot query elf handle");
    	return -1;
    }

    if (!(strtbl = (char*)IElf->GetSectionTags(hElf, GST_SectionIndex, strtblidx, TAG_DONE)))
    {
    	console_printf(OUTPUT_WARNING,"Unable to get string table\n");
    	return -1;
    }

    /* Go through all sections, and make sure they are loaded and relocated */
    for (i = 1; i < nSecs; i++)
    {
		if (!(pHeader = IElf->GetSectionHeaderTags(hElf, GST_SectionIndex, i, TAG_DONE)))
			continue;

		/* We also keep track, where the executable section is located
		 * which the base address is.
		 */
		if (pHeader->sh_flags & SWF_EXECINSTR)
		{
			code_elf_addr = pHeader->sh_addr;
			code_size = pHeader->sh_size;
			code_relocated_addr = IElf->GetSectionTags(hElf,
					GST_SectionIndex, i,
					TAG_DONE);

			console_printf(OUTPUT_SYSTEM, "Executable section (0x%x/%d bytes) starts at %p",(int)code_elf_addr,code_size,code_relocated_addr);
		}

		/* If this is a rela section, relocate the related section */
		if (pHeader && (pHeader->sh_type == SHT_RELA))
		{
			Elf32_Shdr *pRefHeader;

			/* Get the section header to which this rela section refers. This is the one we want
			 * to relocate. Make sure that it exists. */
			pRefHeader = IElf->GetSectionHeaderTags(hElf, GST_SectionIndex, pHeader->sh_info, TAG_DONE);

			/* But we don't need to do anything, if this has the SWF_ALLOC, as this case
			 * has already been handled by LoadSeg. Sections that bear debugging information
			 * (e.g., drawf2 ones) don't come with that flag.
			 */
			if (pRefHeader && !(pRefHeader->sh_flags & SWF_ALLOC))
			{
				console_printf(OUTPUT_SYSTEM, "Relocating section \"%s\" (index %d) which is referred by section \"%s\" (index %d)",
						&strtbl[pRefHeader->sh_name], pHeader->sh_info,
						&strtbl[pHeader->sh_name],i);
				tags[0].ti_Data = pHeader->sh_info;

				if (!IElf->RelocateSection(hElf, tags))
				{
					console_printf(OUTPUT_WARNING,"Section %s (index %d) could not been relocated.",&strtbl[pRefHeader->sh_name],(int)pHeader->sh_info);
				}

			}
		}
    }
}

BOOL 
amigaos_attach (struct Process *aprocess)
{
    /* Can only debug processes right now */
    if (aprocess->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
		printf ("Not a process!\n");
		return FALSE;
    }

    /* Get the seglist and elf handle of the process */
    exec_seglist = IDOS->GetProcSegList (aprocess, GPSLF_SEG|GPSLF_RUN);
  
    if (!exec_seglist)
    {
		console_printf(OUTPUT_WARNING, "Can't find seglist!\n");
		return FALSE;
    }


    IDOS->GetSegListInfoTags(exec_seglist, 
							 GSLI_ElfHandle, &exec_elfhandle,
							 //GSLI_Native, &seglist,
							 TAG_DONE);

    if (!exec_elfhandle)
    {
		exec_seglist = 0;
		console_printf(OUTPUT_WARNING, "Process is no ELF binary!\n");
		return FALSE;
    }

	process  = aprocess;

    /* Suspend the task */
    //dprintf("Process state: %d\n",pProcess->pr_Task.tc_State);
    if (process->pr_Task.tc_State == TS_READY || process->pr_Task.tc_State == TS_WAIT)
    {
		console_printf(OUTPUT_SYSTEM, "Suspending task %p", process);
		IExec->SuspendTask((struct Task *)process, 0);
    }

	task_exists = TRUE;
	task_playing = FALSE;
   
    if(process->pr_Task.tc_State == TS_CRASHED)
    {
    	process->pr_Task.tc_State = TS_SUSPENDED;
    }

	attach_hook();

	IDebug->ReadTaskContext((struct Task *)process, &context_copy, RTCF_ALL);
	IExec->Signal((struct Task *)me, debug_sigfield);

	if (open_elfhandle() == 0)
	{
		killtask();
		return FALSE;
	}
	if (amigaos_relocate_elfhandle() == -1)
	{
		killtask();
		return FALSE;
	}
	return TRUE;
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
		ret = -1;
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

	if (open_elfhandle() == 0)
	{
		killtask();
		return -1;
	}
	if (amigaos_relocate_elfhandle() == -1)
	{
		killtask();
		return -1;
	}

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
			IExec->Forbid();
			IExec->RestartTask((struct Task *)process, 0L);
			if (process->pr_Task.tc_State == TS_SUSPENDED || process->pr_Task.tc_State == TS_CRASHED)
			{
				IExec->Permit();
				console_printf(OUTPUT_WARNING, "Failed to restart task!");
				//IExec->RemTask (newtask);
				return FALSE;
			}
			else
			{
				IExec->Permit();
				task_playing = TRUE;
				return TRUE;
			}
		}
	}
	return FALSE;
}

void pause()
{
	if (task_exists)
		if (task_playing)
		{
			task_playing = FALSE;
			IExec->SuspendTask ((struct Task *) process, 0L);
		}
}

void step()
{
	//current_function->currentline++;
	if (!hasfunctioncontext)
		return;

	struct stab_function *f = current_function;

	insert_line_breakpoints();

	if (is_breakpoint_at(context_copy.ip) && !(f->line[f->currentline].type == LINE_LOOP))
	{
		should_continue = TRUE;
		asmstep();
	}
	else
	{
		install_all_breakpoints();
		play();
	}
}

void into()
{
	catch_sline = TRUE;
	asmstep();
}

void asmstep()
{
	if(task_exists)
	{
		if (task_playing)
			pause();

		context_ptr->ip = context_copy.ip;

		asmstep_install();

		play();
	}
}

void asmskip()
{
	if (task_exists)
	{
		pause();

		context_copy.ip += 4;
		//context_ptr->ip = context_copy.ip;
		IDebug->WriteTaskContext((struct Task *)process, &context_copy, RTCF_STATE);
	}
}

void read_task_context()
{
	IDebug->ReadTaskContext((struct Task *)process, &context_copy, RTCF_ALL);
}

void killtask()
{
	if (task_exists)
	{
		IExec->SuspendTask ((struct Task *) process, 0L);

		//if we attached to the process, RemTask is going to crash...
		if (!isattached)
			IExec->RemTask ((struct Task *)process);
		else
			isattached = FALSE;

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
			*data = dbgmsg->message.context->ip;
			traptype = dbgmsg->message.context->Traptype;
			if (traptype == 0x700 || traptype == 0xd00)
			{
				if (wants_to_crash)
				{
					wants_to_crash = FALSE;
					return 0;
				}
				context_ptr = dbgmsg->message.context;

				memcpy (&context_copy, context_ptr, sizeof(struct ExceptionContext));

				IExec->Signal ((struct Task *)me, debug_sigfield);
			}
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

  /* Write the breakpoint.  */
  if (1)
  {
  	/* Go supervisor */
	stack = IExec->SuperState();

  	/* Make sure to unprotect the memory area */
	oldAttr = IMMU->GetMemoryAttrs((APTR)addr, 0);
	IMMU->SetMemoryAttrs((APTR)addr, 4, MEMATTRF_READ_WRITE);

		//*buffer = *(uint32 *)addr;
		//*(uint32 *)addr = meth_start;	//insert asm("trap")
		//dprintf("buffer: 0x%x meth_start: 0x%x addr:0x%x\n", *buffer, meth_start, addr);
		*buffer = setbreak(addr, meth_start);

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
	
		//*(uint32 *)addr = *buffer;	//restore old instruction
	uint32 realaddr = IMMU->GetPhysicalAddress(addr);
	setbreak(realaddr, *buffer);

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
