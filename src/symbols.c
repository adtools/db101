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


struct List symbols_list;
BOOL has_symbols = FALSE;

static ULONG amigaos_symbol_callback(struct Hook *, struct Task *, struct SymbolMsg *);


struct Hook symbol_hook;

ULONG amigaos_symbol_callback(struct Hook *hook, struct Task *task, struct SymbolMsg *symbolmsg)
{
	if (symbolmsg->Name)
	{
		struct amigaos_symbol *symbol = IExec->AllocMem(sizeof (struct amigaos_symbol), MEMF_ANY|MEMF_CLEAR);
		if (!symbol)
			printf("AllocMem failed!\n");
		symbol->name = strdup (symbolmsg->Name);
		symbol->value = symbolmsg->AbsValue;
		//nosymbols++;
		IExec->AddTail (&symbols_list, (struct Node *)symbol);
	}
	return 1;
}

Elf32_Handle open_elfhandle()
{
	BPTR exec_seglist = IDOS->GetProcSegList (process, GPSLF_SEG|GPSLF_RUN);

	if (!exec_seglist)
	{
		printf("Couldn't get seglist!\n");
		return (0);
	}

    IDOS->GetSegListInfoTags(exec_seglist, 
							 GSLI_ElfHandle,      &exec_elfhandle,
							 TAG_DONE);

	if (exec_elfhandle == NULL)
	{
		printf ("not a PowerPC executable\n");
		return (0);
    }
 
    exec_elfhandle = IElf->OpenElfTags(OET_ElfHandle, exec_elfhandle, TAG_DONE);

	if (exec_elfhandle == NULL)
	{
		printf ("couldn't open elfhandle\n");
		return (0);
    }
	return exec_elfhandle;
}

void close_elfhandle (Elf32_Handle handle)
{
	IElf->CloseElfTags (exec_elfhandle, CET_CloseInput, TRUE, TAG_DONE);
}


void get_symbols ()
{
	if (has_symbols)
		free_symbols();
	IExec->NewList (&symbols_list);

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
	struct amigaos_symbol *symbol = (struct amigaos_symbol *)IExec->GetHead (&symbols_list);
	while (symbol)
	{
		struct amigaos_symbol *next = (struct amigaos_symbol *)IExec->GetSucc ((struct Node *)symbol);
		IExec->Remove ((struct Node *)symbol);
		free (symbol->name);
		IExec->FreeMem (symbol, sizeof(struct amigaos_symbol));
		symbol = next;
	}
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

