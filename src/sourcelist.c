#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/listbrowser.h>

#include <stdio.h>
#include <string.h>

#include "sourcelist.h"
#include "source.h"
#include "freemem.h"
#include "console.h"
#include "stabs.h"
#include "gui.h"
#include "symbols.h"

struct List sourcelist_list;
//extern Object *MainObj[GAD_SOURCELIST_LISTBROWSER];

extern struct Window *mainwin;

int sourcelist_freemem_hook = -1;

void sourcelist_init()
{
	IExec->NewList(&sourcelist_list);
	sourcelist_freemem_hook = freemem_alloc_hook();
}

void sourcelist_cleanup()
{
	IListBrowser->FreeListBrowserList(&sourcelist_list);
	freemem_free_hook(sourcelist_freemem_hook);
}

void sourcelist_clear()
{
	IIntuition->SetAttrs(MainObj[GAD_SOURCELIST_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
	if(!IsListEmpty(&sourcelist_list))
	{
		IListBrowser->FreeListBrowserList(&sourcelist_list);
		freemem_clear(sourcelist_freemem_hook);
		IExec->NewList(&sourcelist_list);
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_SOURCELIST_LISTBROWSER], mainwin, NULL, LISTBROWSER_Labels, &sourcelist_list, TAG_END);
}

void sourcelist_update()
{
	sourcelist_clear();
	IIntuition->SetAttrs(MainObj[GAD_SOURCELIST_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
	struct stab_module *m = (struct stab_module *)IExec->GetHead(&modules_list);
	while(m)
	{
		struct stab_sourcefile *s = (struct stab_sourcefile *)IExec->GetHead(&m->sourcefiles);
		while(s)
		{
			struct Node *node;
			if (node = IListBrowser->AllocListBrowserNode(1,
												LBNA_UserData,	s,
	           									LBNA_Column, 0,
	           										LBNCA_CopyText, TRUE,
    	            								LBNCA_Text, s->filename,
            									TAG_DONE))
        							{
							            IExec->AddTail(&sourcelist_list, node);
									}
			s = (struct stab_sourcefile *)IExec->GetSucc((struct Node *)s);
		}
		m = (struct stab_module *)IExec->GetSucc((struct Node *)m);
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_SOURCELIST_LISTBROWSER], mainwin, NULL, LISTBROWSER_Labels, &sourcelist_list, TAG_END);
}

void sourcelist_handle_input()
{
	uint32 event;
	IIntuition->GetAttrs(MainObj[GAD_SOURCELIST_LISTBROWSER], LISTBROWSER_RelEvent, &event, TAG_DONE);
	
	switch( event )
	{
		case LBRE_NORMAL:
		{
			struct Node *node;
			struct stab_sourcefile *s;
			IIntuition->GetAttr(LISTBROWSER_SelectedNode, MainObj[GAD_SOURCELIST_LISTBROWSER], (ULONG *) &node);
			if(!node)
				return;
			IListBrowser->GetListBrowserNodeAttrs(node, LBNA_UserData, &s, TAG_DONE);
			if(s)
			{
				source_load_file(s, NULL);
				source_show_currentline();
			}
		}
		break;
	}
}

void sourcelist_show_main()
{
	uint32 addr = get_symval_from_name("main");
	console_printf(OUTPUT_NORMAL, "main=0x%x", addr);
	struct stab_function *f = stabs_get_function_from_address(addr);
	if(f)
	{
		struct Node *n = IExec->GetHead(&sourcelist_list);
		while(n)
		{
			struct stab_sourcefile *s;
			IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &s, TAG_DONE);
			if(s == f->sourcefile)
			{
				IIntuition->SetAttrs(MainObj[GAD_SOURCELIST_LISTBROWSER], LISTBROWSER_SelectedNode, n, TAG_DONE);
				IIntuition->RefreshGadgets((struct Gadget *)MainObj[GAD_SOURCELIST_LISTBROWSER], mainwin, NULL);
				source_load_file(f->sourcefile, NULL);
				IIntuition->SetAttrs(MainObj[GAD_SOURCE_LISTBROWSER], LISTBROWSER_MakeVisible, f->line[0].infile-2, TAG_DONE);
				IIntuition->RefreshGadgets((struct Gadget *)MainObj[GAD_SOURCE_LISTBROWSER], mainwin, NULL);
			}
			n = IExec->GetSucc(n);
		}
	}
}
