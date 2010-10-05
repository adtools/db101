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

#define    MSR_TRACE_ENABLE           0x00000400

static ULONG amigaos_debug_callback(struct Hook *, struct Task *, struct KernelDebugMessage *);

struct Hook debug_hook;
struct Process *process = NULL, *me = NULL;

BPTR exec_seglist;
ULONG debug_data = 1234;

Elf32_Handle exec_elfhandle;
Elf32_Addr code_elf_addr;
int code_size;
void *code_relocated_addr;


struct Elf32_SymbolQuery query;

struct ExceptionContext *context_ptr, context_copy;

BYTE debug_signal;
ULONG debug_sigfield;

BOOL task_exists = FALSE;
BOOL task_playing = FALSE;
BOOL wants_to_crash = FALSE;
BOOL should_continue = FALSE;
BOOL entering_function = FALSE;

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
		printf("Can't get MMU access\n");
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
    	fprintf(stderr,"Cannot query elf handle\n");
    	return -1;
    }

    if (!(strtbl = (char*)IElf->GetSectionTags(hElf, GST_SectionIndex, strtblidx, TAG_DONE)))
    {
    	fprintf(stderr,"Unable to get string table\n");
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

			printf("Executable section (0x%x/%d bytes) starts at %p\n",(int)code_elf_addr,code_size,code_relocated_addr);
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
				printf("Relocating section \"%s\" (index %d) which is referred by section \"%s\" (index %d)\n",
						&strtbl[pRefHeader->sh_name], pHeader->sh_info,
						&strtbl[pHeader->sh_name],i);
				tags[0].ti_Data = pHeader->sh_info;

				if (!IElf->RelocateSection(hElf, tags))
				{
					fprintf(stderr,"Section %s (index %d) could not been relocated.\n",&strtbl[pRefHeader->sh_name],(int)pHeader->sh_info);
				}

			}
		}
    }
}

int load_inferior(char *filename, char *path, char *args)
{
//	struct Task *me = IExec->FindTask(NULL);
	int ret = 0;

	char fullpath[1024];

	BPTR lock = IDOS->Lock (path, SHARED_LOCK);
	if (!lock)
	{
		printf("Coudln't get currentdir lock\n");
		return -1;
	}
	BPTR homelock = IDOS->DupLock (lock);

	if (strlen (filename) == 0)
		return 0;
	if (strlen (path) == 0)
		strcpy (fullpath, filename);
	else if (path[strlen(path)-1] == '/')
		sprintf(fullpath, "%s%s", path, filename);
	else
		sprintf(fullpath, "%s/%s", path, filename);

    exec_seglist = IDOS->LoadSeg(fullpath);
	
    if (exec_seglist == 0L)
	{
		printf ("\"%s\": not an executable file\n", filename);
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
				NP_Output,          IDOS->Output(),
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
		printf("Couldn't start new task\n");
		ret = -1;
	}
	else
	{
		IExec->SuspendTask((struct Task *)process, 0L);
		task_exists = TRUE;
		task_playing = FALSE;
		attach_hook();
		IExec->Permit();
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

void play()
{
	if (task_exists)
		if (!task_playing)
		{
			task_playing = TRUE;
			entering_function = TRUE;
			IExec->RestartTask((struct Task *)process, 0L);
			if (process->pr_Task.tc_State == TS_SUSPENDED)
			{
				printf("Failed to restart task!\n");
				//IExec->RemTask (newtask);
			}
		}
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

	if (is_breakpoint_at(context_copy.ip) && !(f->linetype[f->currentline] == LINE_LOOP))
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



void asmstep()
{
	if(task_exists)
	{
		if (task_playing)
			pause();

		//should work, but doesn't:
		//context_ptr->msr |= MSR_TRACE_ENABLE;
		//this is just a bad hack:
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
		context_ptr->ip = context_copy.ip;

		disassembler_makelist();
	}
}


void killtask()
{
	if (task_exists)
	{
		IExec->SuspendTask ((struct Task *) process, 0L);
		IExec->RemTask ((struct Task *)process);
		task_exists = FALSE;
		task_playing = FALSE;
	}
}

//struct ExceptionContext *context_ptr;


void crash()
{
	wants_to_crash = TRUE;
	//how to do this ???
	//insert_breakpoint (context_ptr->ip);
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
			//task_exists = FALSE;
			break;

		case DBHMT_EXCEPTION:
			*data = 11;
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

		*buffer = *(uint32 *)addr;

		*(uint32 *)addr = meth_start;	//insert asm("trap")

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
	
  	/* Make sure to unprotect the memory area */
	oldAttr = IMMU->GetMemoryAttrs((APTR)addr, 0);
	IMMU->SetMemoryAttrs((APTR)addr, 4, MEMATTRF_READ_WRITE);
	
		*(uint32 *)addr = *buffer;	//restore old instruction

	/* Set old attributes again */
	IMMU->SetMemoryAttrs((APTR)addr, 4, oldAttr);
	
	/* Return to old state */
	if (stack)
		IExec->UserState(stack);
  }
  IExec->CacheClearU();

  return 0;
}



