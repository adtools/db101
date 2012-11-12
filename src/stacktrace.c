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


Object *StacktraceListBrowserObj;
struct List stacktrace_list;

extern struct Window *mainwin;

extern struct DebugIFace *IDebug;

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
	IIntuition->RefreshGadgets ((struct Gadget *)StacktraceListBrowserObj, mainwin, NULL);
}

void stacktrace_update()
{
	if (!task_exists)
		return;

	stacktrace_clear();
	IExec->NewList(&stacktrace_list);
	
	struct Task *t = (struct Task *)process;
	struct Node *node;
	uint32 stacksize = process->pr_StackSize;
	uint32 stackupper = (uint32)t->tc_SPUpper;
	uint32 stackpointer = (uint32)t->tc_SPReg;

	uint32 p = stackpointer;

	IIntuition->SetAttrs(StacktraceListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
	int i = 0;
	while (p < stackupper-28)
	{
		i++;
		char *str = (char*) freemem_malloc(stacktrace_freemem_hook, 1024);

		//first try the stabs functions:
		struct stab_function *f = stabs_get_function_from_address (*(uint32*)(p+4));
		char *fname = NULL;
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

		sprintf(str, "0x%x (%d): %s", p-stackpointer, i, fname);

		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&stacktrace_list, node);
									}
		p = *(uint32*)p;
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)StacktraceListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &stacktrace_list, TAG_END);
}
