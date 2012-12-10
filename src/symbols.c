/*symbols.c*/


#include <exec/types.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/button.h>
#include <proto/string.h>
#include <proto/layout.h>
#include <proto/label.h>
#include <proto/listbrowser.h>
#include <proto/asl.h>
#include <proto/elf.h>


#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

#include <libraries/asl.h>
#include <interfaces/asl.h>

#include <libraries/elf.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "suspend.h"
#include "stabs.h"
#include "symbols.h"
#include "freemem.h"
#include "console.h"

struct List symbols_list;
BOOL has_symbols = FALSE;

int symbols_freemem_hook = -1;

static ULONG amigaos_symbol_callback(struct Hook *, struct Task *, struct SymbolMsg *);


struct Hook symbol_hook;

ULONG amigaos_symbol_callback(struct Hook *hook, struct Task *task, struct SymbolMsg *symbolmsg)
{
	if (symbolmsg->Name)
	{
		struct amigaos_symbol *symbol = freemem_malloc(symbols_freemem_hook, sizeof(struct amigaos_symbol));
		if (!symbol)
			printf("AllocMem failed!\n");
		symbol->name = strdup (symbolmsg->Name);
		symbol->value = symbolmsg->AbsValue;
		//nosymbols++;
		IExec->AddTail (&symbols_list, (struct Node *)symbol);
	}
	return 1;
}

Elf32_Handle open_elfhandle(BPTR seglist)
{
	Elf32_Handle elfhandle;
    IDOS->GetSegListInfoTags(seglist, 
							 GSLI_ElfHandle,      &elfhandle,
							 TAG_DONE);

	if(elfhandle == NULL)
	{
		console_printf (OUTPUT_WARNING, "not a PowerPC executable");
		return (0);
    }

    elfhandle = IElf->OpenElfTags(OET_ElfHandle, elfhandle, TAG_DONE);

	if (elfhandle == NULL)
	{
		console_printf (OUTPUT_WARNING, "couldn't open elfhandle");
		return (0);
   	}
	return elfhandle;
}

void close_elfhandle (Elf32_Handle handle)
{
	IElf->CloseElfTags (handle, CET_CloseInput, TRUE, TAG_DONE);
}


void get_symbols ()
{
	if (has_symbols)
		free_symbols();
	IExec->NewList (&symbols_list);

	symbols_freemem_hook = freemem_alloc_hook();

    symbol_hook.h_Entry = (ULONG (*)())amigaos_symbol_callback;
    symbol_hook.h_Data =  NULL;

	//printf("calling ScanSymbolTable...\n");
	IElf->ScanSymbolTable (exec_elfhandle, &symbol_hook, NULL);
	//printf("done!\n");

	has_symbols = TRUE;
}

void free_symbols ()
{
	if (!has_symbols)
		return;

	if(symbols_freemem_hook != -1)
		freemem_free_hook(symbols_freemem_hook);
	IExec->NewList(&symbols_list);
	has_symbols = FALSE;
}

void list_symbols ()
{
//	int i;
//	for ( i = 0; i < nosymbols; i++)
//		printf (" \"%s\" = 0x%x\n", symlist[i], symval[i]);
}

uint32 get_symval_from_name (char *name)
{
	if (!has_symbols)
		return 0x0;
	struct amigaos_symbol *s = (struct amigaos_symbol *)IExec->GetHead (&symbols_list);
	while (s)
	{
		if (!strcmp(name, s->name))
			return s->value;
		s = (struct amigaos_symbol *)IExec->GetSucc ((struct Node *)s);
	}
	return 0x0;
}

char *get_symbol_from_value(uint32 val)
{
	if(!has_symbols)
		return NULL;
	struct amigaos_symbol *s = (struct amigaos_symbol *)IExec->GetHead(&symbols_list);
	while(s)
	{
		if(s->value == val)
			return s->name;
		s = (struct amigaos_symbol *)IExec->GetSucc((struct Node *)s);
	}
	return NULL;
}
