/*breakpoints.c*/

#include <proto/exec.h>

#include <exec/lists.h>

#include "suspend.h"
#include "breakpoints.h"
#include "gui.h"
#include "stabs.h"

extern struct DebugIFace *IDebug;

struct List breakpoint_list;
BOOL breakpoints_installed = FALSE;
BOOL asmtrace_should_restore = FALSE;

void init_breakpoints()
{
	IExec->NewList(&breakpoint_list);
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

	if (f->linetype[i] == LINE_LOOP)
	{
		uint32 newline = translate_sline_to_nline (f->lineinfile[i]);
		insert_breakpoint (f->address + f->lines[newline+1], BR_LINE);
		insert_breakpoint (f->address + f->lines[i+1], BR_LINE);
	}
	else
		insert_breakpoint (f->address + f->lines[i+1], BR_LINE);
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
		if (f->lineinfile[i] == sline)
			return i;
}

int get_nline_from_address(uint32 addr)
{
	if (!hasfunctioncontext)
		return -1;

	struct stab_function *f = current_function;
	int i;
	for (i=0; i < f->numberoflines; i++)
		if (f->address + f->lines[i] == addr)
			return i;

	return -1;
}


void guess_line_in_function ()
{
	if (!hasfunctioncontext)
		return;
	struct stab_function *f = current_function;
	uint32 ip = context_copy.ip;

	if (! (ip >= f->address && ip < f->address + f->size))
	{
		f->currentline = -1;
		return;
	}

	int i;
	for (i=0; i < f->numberoflines-1; i++)
	{
		if ( ip >= f->address + f->lines[i] && ip < f->address + f->lines[i+1])
		{
			f->currentline = i;
			return;
		}
	}

	f->currentline = f->numberoflines-1;
}


void insert_breakpoint(uint32 addr, uint32 type)
{
	if(is_breakpoint_at(addr))
		return;

//printf("insert breakpoint at 0x%x\n", addr);
	struct breakpoint *b = IExec->AllocMem (sizeof(struct breakpoint), MEMF_ANY|MEMF_CLEAR);

	b->address = addr;
	b->type = type;
	b->save_buffer = 0;

	IExec->AddTail (&breakpoint_list, (struct Node *)b);
}

void remove_breakpoint (uint32 addr)
{
	if (!is_breakpoint_at)
		return;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			if (addr == b->address)
				IExec->Remove (node);

			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
	}
}


BOOL asm_trace = FALSE;

uint32 asmstep_addr = 0;
uint32 asmstep_buffer = 0;


void asmstep_install()
{
	char dummy1[5], dummy2[40];
	uint32 traceaddr = (uint32)IDebug->DisassembleNative((APTR)context_copy.ip, dummy1, dummy2);

	if (is_breakpoint_at(traceaddr))
	{
		asmtrace_should_restore = FALSE;
		asm_trace = TRUE;
	}
	else
	{
		asmtrace_should_restore = TRUE;

		if (!memory_insert_breakpoint (traceaddr, &asmstep_buffer))
		{
			asmstep_addr = traceaddr;
			asm_trace = TRUE;
		}
	}
}

void asmstep_remove()
{
	if (asm_trace && asmtrace_should_restore)
		memory_remove_breakpoint(asmstep_addr, &asmstep_buffer);
	asm_trace = FALSE;
}
