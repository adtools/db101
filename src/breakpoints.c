/*breakpoints.c*/

#include <proto/exec.h>
#include <exec/lists.h>
#include <proto/intuition.h>
#include <proto/requester.h>
#include <classes/requester.h>
#include <dos/dos.h>

#include <string.h>
#include <stdio.h>

#include "suspend.h"
#include "breakpoints.h"
#include "gui.h"
#include "stabs.h"
#include "freemem.h"
#include "preferences.h"
#include "tracking.h"
#include "console.h"

#define dprintf(format, args...) ((struct ExecIFace *)((*(struct ExecBase **)4)->MainInterface))->DebugPrintF("[%s] " format, __PRETTY_FUNCTION__ , ## args)

extern struct Window *mainwin;

extern BOOL is_readable_address (uint32);

extern struct DebugIFace *IDebug;

struct List breakpoint_list;
BOOL breakpoints_installed = FALSE;

BOOL has_tracebit = TRUE;

void init_breakpoints()
{
	IExec->NewList(&breakpoint_list);
	
	uint32 family;
	IExec->GetCPUInfoTags(GCIT_Family, &family, TAG_DONE);
	if(family == CPUFAMILY_4XX)
		has_tracebit = FALSE;
}

BOOL is_breakpoint_at(uint32 addr)
{
	BOOL ret = FALSE;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			if (addr == b->address)
			{
				ret = TRUE;
				break;
			}

			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
	}
	return ret;
}

void install_all_breakpoints()
{
	if (breakpoints_installed)
		return;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			memory_insert_breakpoint (b->address, &(b->save_buffer));

			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
		breakpoints_installed = TRUE;
	}
}

void suspend_all_breakpoints()
{
	if (!breakpoints_installed)
		return;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			memory_remove_breakpoint (b->address, &(b->save_buffer));

			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
		breakpoints_installed = FALSE;
	}
}

void insert_line_breakpoints()
{
	struct stab_function *f = current_function;
	uint32 i = f->currentline;
	if(i == f->numberoflines-1)
		return;

	if (f->line[i].type == LINE_LOOP)
	{
		uint32 newline = translate_sline_to_nline (f->line[i].infile);
		insert_breakpoint (f->address + f->line[newline+1].adr, BR_LINE, NULL, 0);
		insert_breakpoint (f->address + f->line[i+1].adr, BR_LINE, NULL, 0);
	}
	else
	{
		int line = f->line[f->currentline].infile;
		for(; line == f->line[i].infile; i++)
			;
		insert_breakpoint (f->address + f->line[i].adr, BR_LINE, NULL, 0);
	}
}

void remove_line_breakpoints()
{
	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			if (b->type == BR_LINE)
			{
				IExec->Remove(node);
				node = IExec->GetHead (&breakpoint_list);
			}
			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
	}
}

uint32 translate_sline_to_nline(uint32 sline)
{
	if (!hasfunctioncontext)
		return;
	struct stab_function *f = current_function;

	int i;
	for (i = 0; i < f->numberoflines; i++)
		if (f->line[i].infile == sline)
			return i;
}

int get_nline_from_address(uint32 addr)
{
	if (!hasfunctioncontext)
		return -1;

	struct stab_function *f = current_function;
	int i;
	for (i=0; i < f->numberoflines; i++)
		if (f->address + f->line[i].adr == addr)
			return i;
	return -1;
}


int guess_line_in_function ()
{
	if (!hasfunctioncontext)
		return;
	struct stab_function *f = current_function;
	uint32 ip = context_copy.ip;

	if (! (ip >= f->address && ip < f->address + f->size))
	{
		f->currentline = -1;
		return -1;
	}

	int i;
	for (i=0; i < f->numberoflines-1; i++)
	{
		if ( ip >= f->address + f->line[i].adr && ip < f->address + f->line[i+1].adr)
		{
			f->currentline = i;
			return i;
		}
	}

	f->currentline = f->numberoflines-1;
}

BOOL breakpoint_line_in_file (uint32 sline, char *file)
{
	uint32 nline;
	struct stab_sourcefile *s = stabs_get_sourcefile(file);
	struct stab_function *f = stabs_sline_to_nline(s, sline, &nline, NULL);
	if(f)
	{
		insert_breakpoint (f->address + f->line[nline].adr, BR_NORMAL_FUNCTION, (APTR)f, sline);
		return TRUE;
	}
	return FALSE;
}

BOOL breakpoint_function (char *fname)
{
	struct stab_function *f = stabs_get_function_from_name(fname);
	if(f)
	{
		uint32 sline = 0;
		if(f->line)
			sline = f->line[0].infile;
		insert_breakpoint (f->address, BR_NORMAL_FUNCTION, f, sline);
		return TRUE;
	}
	return FALSE;
}


void insert_breakpoint(uint32 addr, uint32 type, APTR object, uint32 line)
{
	if(is_breakpoint_at(addr))
		return;

	struct breakpoint *b = IExec->AllocVecTags (sizeof(struct breakpoint), TAG_DONE);

	b->address = addr;
	b->type = type;
	b->save_buffer = 0;
	switch(type)
	{
		case BR_NORMAL_SYMBOL:
			b->symbol = (struct amigaos_symbol *)object;
			break;
		case BR_NORMAL_FUNCTION:
			b->function = (struct stab_function *)object;
			b->line = line;
			break;
	}
	IExec->AddTail (&breakpoint_list, (struct Node *)b);
}

void remove_breakpoint(uint32 addr)
{
	if (!is_breakpoint_at(addr))
		return;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	while (node)
	{
		b = (struct breakpoint *)node;

		if (addr == b->address)
		{
			IExec->Remove (node);
			IExec->FreeVec (b);	
			break;
		}

		node = IExec->GetSucc (node);
	}
}

void remove_breakpoint_node(struct breakpoint *p)
{
	if(p)
		IExec->Remove((struct Node *)p);
}

void free_breakpoints ()
{
	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct Node *next;
	struct breakpoint *b;

	while (node)
	{
		next = IExec->GetSucc (node);
		b = (struct breakpoint *)node;
		IExec->Remove (node);
		IExec->FreeVec (b);		

		node = next;
	}
}

uint32 stepout_address, stepout_buffer;
BOOL stepout_installed = FALSE;

void stepout_install()
{
	stepout_address = context_copy.lr;
	if((hasfunctioncontext && stabs_get_function_from_address(stepout_address) == current_function)
		|| (stepout_address == context_copy.ip))
	{	
		uint32 *newstack = *(uint32**)context_copy.gpr[1];
		stepout_address = *(++newstack);
	}
	memory_insert_breakpoint (stepout_address, &stepout_buffer);
	stepout_installed = TRUE;
}

void stepout_remove()
{
	if(stepout_installed)
		memory_remove_breakpoint(stepout_address, &stepout_buffer);
	stepout_installed = FALSE;
}


extern struct MMUIFace *IMMU;


BOOL asm_trace = FALSE;
BOOL no_msr_trace = FALSE;

uint32 asmstep_addr[2] = { 0, 0};
uint32 asmstep_buffer[2] = { 0, 0 };
BOOL asmtrace_should_restore[2] = { FALSE, FALSE };
int no_asm_breaks = 0;

#define    MSR_TRACE_ENABLE           0x00000400

void asmstep_install()
{
	if(has_tracebit)
	{
		struct ExceptionContext ctx;;
		IDebug->ReadTaskContext((struct Task *)process, &ctx, RTCF_STATE);
		//this is not supported on the sam cpu:
		ctx.msr |= MSR_TRACE_ENABLE;
		ctx.ip = context_copy.ip;
		IDebug->WriteTaskContext((struct Task *)process, &ctx, RTCF_STATE);
	}
	else
	{
		int32 offset;

		char dummy1[5], dummy2[40];
		asmstep_addr[0] = (uint32)IDebug->DisassembleNative((APTR)context_copy.ip, dummy1, dummy2);

		switch(PPC_DisassembleBranchInstr(*(uint32 *)context_copy.ip, &offset))
		{
			case PPC_OTHER:
				no_asm_breaks = 1;
				break;
			case PPC_BRANCHTOLINK:
				no_asm_breaks = 2;
				asmstep_addr[1] = context_copy.lr;
				break;
			case PPC_BRANCHTOCTR:
				no_asm_breaks = 2;
				asmstep_addr[1] = context_copy.ctr;
				break;
			case PPC_BRANCH:
				no_asm_breaks = 1;
				asmstep_addr[0] = context_copy.ip + offset;
				break;
			case PPC_BRANCHCOND:
				no_asm_breaks = 2;
				asmstep_addr[1] = context_copy.ip + offset;
				break;
			default:
				break;
		}
		int i;
		if(no_asm_breaks == 2 && asmstep_addr[0] == asmstep_addr[1])
			no_asm_breaks = 1;
		for(i = 0; i < no_asm_breaks; i++)
		{
			if (is_breakpoint_at(asmstep_addr[i]))
			{
				asmtrace_should_restore[i] = FALSE;
			}
			else
			{
				asmtrace_should_restore[i] = TRUE;
				memory_insert_breakpoint (asmstep_addr[i], &asmstep_buffer[i]);
			}
		}
	}
	asm_trace = TRUE;
}

void asmstep_nobranch_install()
{
	no_msr_trace = TRUE;
	asm_trace = TRUE;
	no_asm_breaks = 1;
	char dummy1[5], dummy2[40];
	asmstep_addr[0] = (uint32)IDebug->DisassembleNative((APTR)context_copy.ip, dummy1, dummy2);
	if(is_breakpoint_at(asmstep_addr[0]))
		asmtrace_should_restore[0] = FALSE;
	else
	{
		asmtrace_should_restore[0] = TRUE;
		memory_insert_breakpoint(asmstep_addr[0], &asmstep_buffer[0]);
	}
}

void asmstep_remove()
{
	if(has_tracebit && !no_msr_trace)
	{
		struct ExceptionContext ctx;;
		IDebug->ReadTaskContext((struct Task *)process, &ctx, RTCF_STATE);
		//this is not supported on the sam cpu:
		ctx.msr &= ~MSR_TRACE_ENABLE;
		ctx.ip = context_copy.ip;
		IDebug->WriteTaskContext((struct Task *)process, &ctx, RTCF_STATE);
	}
	else
	{
		int i;
		for(i = 0; i < no_asm_breaks; i++)
		{
			if (asmtrace_should_restore[i])
				memory_remove_breakpoint(asmstep_addr[i], &asmstep_buffer[i]);
		}
	}
	no_msr_trace = FALSE;
	asm_trace = FALSE;
}
