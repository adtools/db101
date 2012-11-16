#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/listbrowser.h>

#include <stdio.h>
#include <string.h>

#include "source.h"
#include "freemem.h"
#include "console.h"
#include "stabs.h"
#include "gui.h"
#include "breakpoints.h"

struct List source_list;
Object *SourceListBrowserObj;
struct ColumnInfo *sourcecolumninfo;

extern struct Window *mainwin;

int source_freemem_hook = -1;

char currentfile[1024] = "";
extern char childpath[1024];

void source_init()
{
	IExec->NewList(&source_list);
	source_freemem_hook = freemem_alloc_hook();
	
    sourcecolumninfo = IListBrowser->AllocLBColumnInfo(3,
        LBCIA_Column, 0,
            LBCIA_Title, "Br",
            LBCIA_DraggableSeparator, TRUE,
        LBCIA_Column, 1,
            LBCIA_Title, "line",
            LBCIA_DraggableSeparator, TRUE,
        LBCIA_Column, 2,
        	LBCIA_Title, "",
            LBCIA_DraggableSeparator, TRUE,
        TAG_DONE);

}

void source_cleanup()
{
	IListBrowser->FreeLBColumnInfo(sourcecolumninfo);
	IListBrowser->FreeListBrowserList(&source_list);
	freemem_free_hook(source_freemem_hook);
}

void source_clear()
{
	IIntuition->SetAttrs(SourceListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
	if(!IsListEmpty(&source_list))
	{
		IListBrowser->FreeListBrowserList(&source_list);
		freemem_clear(source_freemem_hook);
	}
	strcpy(currentfile, "");
	IIntuition->SetGadgetAttrs((struct Gadget *)SourceListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &source_list, TAG_END);
}

char *source_convert_string(char *str)
{
	char *ptr = str;
	int len = 0;
	for(; *ptr != '\0'; ptr++)
	{
		switch(*ptr)
		{
			case '\t':
				len += 4;
				break;
			case '\n':
				break;
			default:
				len++;
				break;
		}
	}
	char *ret = freemem_malloc(source_freemem_hook, len+1);
	char *ptr2 = ret;
	for(ptr = str; *ptr != '\0'; ptr++)
		switch(*ptr)
		{
			case '\t':
				*(ptr2++) = ' '; *(ptr2++) = ' '; *(ptr2++) = ' '; *(ptr2++) = ' ';
				break;
			case '\n':
				break;
			default:
				*(ptr2++) = *ptr;
		}
	*ptr2 = '\0';
	return ret;
}

typedef struct /* lb_userdata */
{
   uint32 sline;
   int32 nline;
   struct stab_function *func;
} lb_userdata;

void source_load_file(char *name)
{
	source_clear();
	
	char buffer[1024];
	char linebuf[64];

	char *sourcename;
	char *realname;
	int u = 0;
	if((u=__unix_to_amiga_path_name(name, &realname)) == 1)
		sourcename = realname;
	else
		sourcename = name;

	char fullpath[1024];
	if (strlen (sourcename) == 0)
		return;
	if (strlen (childpath) == 0)
		strcpy (fullpath, sourcename);
	else if (childpath[strlen(childpath)-1] == '/')
		sprintf(fullpath, "%s%s", childpath, sourcename);
	else
		sprintf(fullpath, "%s/%s", childpath, sourcename);

	FILE *fd = fopen(fullpath, "r");
	if(fd)
	{
		IIntuition->SetAttrs(SourceListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
		int sline = 1;
		while(1)
		{
			if(fgets(buffer, 1024, fd) != buffer)
				break;

			uint32 nline;
			struct stab_function *f;
			BOOL box = FALSE, check = FALSE;
			
	        if((f = stabs_sline_to_nline(name, sline, &nline)) != 0 )
    	    {
        	    box = TRUE;
            	check = is_breakpoint_at(f->address + f->line[nline].adr);
         	}
         	else
            	box = FALSE;

         	lb_userdata *lbnode = freemem_malloc(source_freemem_hook, sizeof(lb_userdata));
    	    if( lbnode )
	        {
        		lbnode->sline = sline;
				lbnode->nline = nline;
				lbnode->func = f;
			}

			struct Node *node;
			char *str = source_convert_string(buffer);
			if (node = IListBrowser->AllocListBrowserNode(3,
												LBNA_CheckBox, box,
												box ? LBNA_Checked : TAG_SKIP, check,
												LBNA_UserData, lbnode,
												LBNA_Column, 1,
													LBNCA_HorizJustify, LCJ_RIGHT,
													LBNCA_CopyInteger, TRUE,
													LBNCA_Integer, &sline,
            									LBNA_Column, 2,
                									LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&source_list, node);
									}
			sline++;	
		}
		fclose(fd);
		IIntuition->SetGadgetAttrs((struct Gadget *)SourceListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &source_list, TAG_END);
		strcpy(currentfile, name);
	}
	else
		console_printf(OUTPUT_WARNING, "Failed to open source file %s", name);
}

void source_show_currentline()
{
	if(current_function && strcmp(currentfile, current_function->sourcename) == 0)
	{
		int line = current_function->line[current_function->currentline].infile;
	
		IIntuition->SetGadgetAttrs((struct Gadget *)SourceListBrowserObj, mainwin, NULL,
													LISTBROWSER_Selected, line-1,
													LISTBROWSER_MakeVisible, line-1,
													TAG_END);
	}
}

void source_handle_input()
{
	uint32 event, checked = 0, sline = 0, nline, sel;
	struct Node *node = NULL;
	lb_userdata *lbnode;
	struct stab_function *f;

	IIntuition->GetAttr(LISTBROWSER_SelectedNode, SourceListBrowserObj, (ULONG *) &node);
	if(!node)
		return;
	IIntuition->GetAttrs(SourceListBrowserObj, LISTBROWSER_RelEvent, &event, TAG_DONE);
	
	switch( event )
	{
		case LBRE_CHECKED:
		case LBRE_UNCHECKED:

			checked = ((event & LBRE_CHECKED) ? TRUE : FALSE);

			IListBrowser->GetListBrowserNodeAttrs( node,
                                                LBNA_Checked, &checked,
                                                LBNA_UserData, &lbnode,
                                                TAG_DONE);
			sline = lbnode->sline;
			nline = lbnode->nline;
			f = lbnode->func;
			//f = stabs_sline_to_nline(currentfile, sline, &nline);

			if(f)
			{
				if(checked)
					insert_breakpoint(f->address + f->line[nline].adr, BR_NORMAL_FUNCTION, (APTR)f, sline);
				else
					remove_breakpoint(f->address + f->line[nline].adr);
			}
		break;
	}
	source_show_currentline();
}

void source_update()
{
	if(strcmp(currentfile, current_function->sourcename) != 0)
		source_load_file(current_function->sourcename);
	source_show_currentline();
}
