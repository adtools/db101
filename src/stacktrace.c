/*stacktrace.c*/


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

#include <graphics/text.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "suspend.h"
#include "stabs.h"
#include "gui.h"
#include "stacktrace.h"
#include "freemem.h"
#include "variables.h"
#include "progress.h"
#include "console.h"

struct List stacktrace_list;

extern struct Window *mainwin;

extern struct DebugIFace *IDebug;
extern struct MMUIFace *IMMU;

#define MAX(a, b)  (a>b?a:b)

int stacktrace_freemem_hook = -1;

void stacktrace_init()
{
	IExec->NewList(&stacktrace_list);
	stacktrace_freemem_hook = freemem_alloc_hook();
}

void stacktrace_cleanup()
{
	IListBrowser->FreeListBrowserList(&stacktrace_list);
	freemem_free_hook(stacktrace_freemem_hook);
}

void stacktrace_clear()
{
	IListBrowser->FreeListBrowserList(&stacktrace_list);	
	freemem_clear(stacktrace_freemem_hook);
	IIntuition->RefreshGadgets ((struct Gadget *)MainObj[GAD_STACKTRACE_LISTBROWSER], mainwin, NULL);
}

void make_symbol_from_address(uint32 addr, char *buffer, int buffer_size);

void stacktrace_update()
{
	if (!task_exists)
		return;

	stacktrace_clear();
	IExec->NewList(&stacktrace_list);
	
	IIntuition->SetAttrs(MainObj[GAD_STACKTRACE_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);

	//crashsite
	int i = 1;
	char buffer[512] = "";
	struct Node *node;
	BOOL firstframe = TRUE;

	struct Task *t = (struct Task *)process;
	uint32 stackupper = (uint32)t->tc_SPUpper;
	uint32 stacklower = (uint32)t->tc_SPLower;
	uint32 stackpointer =  (uint32)t->tc_SPReg; //context_copy.gpr[1];

open_progress("Making stacktrace...", stackupper-stackpointer, 0);

	uint32 *p = (uint32 *)stackpointer;

	struct stab_function *f = stabs_get_function_from_address (context_copy.ip);
	char *fname = NULL;
	if (f && f->name)
		fname = f->name;
	sprintf(buffer, "0x%x 0x%x (%d): %s", p, p-stackpointer, i, fname);
	if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_CopyText, TRUE,
                								LBNCA_Text, buffer,
            									TAG_DONE))
        							{
							            IExec->AddTail(&stacktrace_list, node);
									}
	while (p)
	{
		uint32 lr;
		i++;
		
		p =(uint32 *)*p;
		if(stacklower >= (uint32)p || stackupper <= (uint32)p)
			break;

        uint32 memflags = IMMU->GetMemoryAttrs((APTR)p, 0);
        if(memflags & MEMATTRF_NOT_MAPPED)
        	break;

		lr = p[1];
        if(!lr && firstframe)
        	lr = context_copy.lr;
        firstframe = FALSE;

		//first try the stabs functions:
		f = stabs_get_function_from_address (lr);
		fname = NULL;
		if (f && f->name)
			fname = f->name;
		if (!fname)
		{
			//try the symbol table:

		}
		if (!fname)
		{
			//try to find the section:
		}
		//if everything fails:
		if (!fname)
			fname = "<unknown function>";

		sprintf(buffer, "0x%x 0x%x (%d): %s", p, p-stackpointer, i, fname);

		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
            									LBNCA_CopyText, TRUE,
                								LBNCA_Text, buffer,
            									TAG_DONE))
        							{
							            IExec->AddTail(&stacktrace_list, node);
									}
update_progress_val((uint32)p-stackpointer);
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_STACKTRACE_LISTBROWSER], mainwin, NULL, LISTBROWSER_Labels, &stacktrace_list, TAG_END);
close_progress();
}
