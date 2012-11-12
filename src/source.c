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
            LBCIA_Title, "l        ",
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

void source_load_file(char *name)
{
	char buffer[1024];
	char linebuf[64];

	char *sourcename = current_function->sourcename;
	char *realname;
	int u = 0;
	if((u=__unix_to_amiga_path_name(sourcename, &realname)) == 1)
		sourcename = realname;

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
		int line = 1;
		while(1)
		{
			if(fgets(buffer, 1024, fd) != buffer)
				break;
			
			struct Node *node;
			char *str = source_convert_string(buffer);
			sprintf(linebuf, "%d", line);
			char *lstr = freemem_strdup (source_freemem_hook, linebuf);
			if (node = IListBrowser->AllocListBrowserNode(3,
												LBNA_Column, 1,
												LBNCA_Text,  lstr,
            									LBNA_Column, 2,
                								LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&source_list, node);
									}
			line++;	
		}
		fclose(fd);
		IIntuition->SetGadgetAttrs((struct Gadget *)SourceListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &source_list, TAG_END);
	}
}

void source_update()
{
	if(strcmp(currentfile, current_function->sourcename) != 0)
	{
		source_load_file(current_function->sourcename);
		strcpy(currentfile, current_function->sourcename);
	}
	int line = current_function->lineinfile[current_function->currentline];
	
	IIntuition->SetGadgetAttrs((struct Gadget *)SourceListBrowserObj, mainwin, NULL,
													LISTBROWSER_Selected, line-1,
													LISTBROWSER_MakeVisible, line-1,
													TAG_END);
}
