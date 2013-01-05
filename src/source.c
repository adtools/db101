#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/listbrowser.h>
#include <proto/requester.h>
#include <proto/asl.h>

#include <classes/requester.h>

#include <stdio.h>
#include <string.h>

#include "source.h"
#include "freemem.h"
#include "console.h"
#include "stabs.h"
#include "gui.h"
#include "breakpoints.h"
#include "stabs.h"
#include "modules.h"

#define dprintf(format...)	IExec->DebugPrintF(format)


struct List source_list;
//Object *MainObj[GAD_SOURCE_LISTBROWSER];
struct ColumnInfo *sourcecolumninfo;

extern struct Window *mainwin;

int source_freemem_hook = -1;
extern int modules_freemem_hook;

struct stab_function *shownfunction = NULL;
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
	IIntuition->SetAttrs(MainObj[GAD_SOURCE_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
	if(!IsListEmpty(&source_list))
	{
		IListBrowser->FreeListBrowserList(&source_list);
		freemem_clear(source_freemem_hook);
	}
	shownfunction = NULL;
	IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_SOURCE_LISTBROWSER], mainwin, NULL, LISTBROWSER_Labels, &source_list, TAG_END);
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

int source_ask(char *sourcename)
{
	Object *reqobj;
	char bodystring[1024];
	
	sprintf(bodystring, "Db101 failed to load sourcefile %s.\nThis is probably because the directory your"
						"executable resides in is different from the root of your source tree.\n"
						"You can choose another location to look for now", sourcename);

	reqobj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
            									REQ_Type,       REQTYPE_INFO,
												REQ_TitleText,  "File not found",
									            REQ_BodyText,   bodystring,
									            //REQ_Image,      REQ_IMAGE,
												//REQ_TimeOutSecs, 10,
									            REQ_GadgetText, "Choose root dir|Choose sourcefile|Cancel",
									            TAG_END
        );

	uint32 code = TRUE;
	if ( reqobj )
	{
		IIntuition->IDoMethod( reqobj, RM_OPENREQ, NULL, mainwin, NULL, TAG_END );
		IIntuition->GetAttr(REQ_ReturnCode, reqobj, &code);
		IIntuition->DisposeObject( reqobj );
	}
	return code;
}

char *source_request_file(char *filename, char *path)
{
	char buffer[1024];
	sprintf(buffer, "Please specify location for file %s", filename);
	struct FileRequester *req = IAsl->AllocAslRequestTags(ASL_FileRequest,
													  ASLFR_Window, mainwin,
													  ASLFR_TitleText, buffer,
													  ASLFR_InitialDrawer, path,
													  TAG_DONE);
	char *ret;
	if (!req)
	{
		console_printf(OUTPUT_WARNING, "Couldn't allocate requester!");	
		return NULL;
	}
	if (!(IAsl->AslRequest (req, NULL)))
	{
		return NULL;
	}

	ret = (char *)freemem_malloc(source_freemem_hook, 1024);
	strcpy(ret, req->fr_Drawer);
	IDOS->AddPart(ret, req->fr_File, 1024);

	IAsl->FreeAslRequest(req);
	return ret;
}

void source_load_file(struct stab_sourcefile *sourcefile, char *solname)
{
	source_clear();

	if(!sourcefile->hasbeeninterpreted)
		stabs_load_sourcefile(sourcefile);
	
	char buffer[1024];
	char linebuf[64];

	char *name = sourcefile->filename;
	BOOL soloverride = FALSE;
	if(solname)
	{
		name = solname;
		soloverride = TRUE;
	}
	char *sourcename;
	char *realname;
	int u = 0;
	if((u=__unix_to_amiga_path_name(name, &realname)) == 1)
		sourcename = realname;
	else
		sourcename = name;

	FILE *fd;
	char fullpath[1024];
	char *realpath = "";
	if (strlen (sourcename) == 0)
		return;

	struct stab_module *m = sourcefile->module;
	struct modules_entry *e = modules_lookup_entry(m->name);
	if(e->sourcepath)
	{
		strcpy(fullpath, e->sourcepath);
		IDOS->AddPart(fullpath, sourcename, 1024);
		realpath = e->sourcepath;
	}
	else
	{
		strcpy(fullpath, sourcename);
	}
	fd = fopen(fullpath, "r");
	if(!fd && strlen(childpath))
	{
		strcpy(fullpath, childpath);
		IDOS->AddPart(fullpath, sourcename, 1024);
		realpath = childpath;
		fd = fopen(fullpath, "r");
	}
	if(!fd)
	{
		switch(source_ask(sourcename))
		{
			case 1:
			{
				//request a path
				char *path = modules_request_path(m->name, m->sourcepath);
				if(path && strlen(path))
				{
					strcpy(fullpath, path);
					IDOS->AddPart(fullpath, sourcename, 1024);
					realpath = path;
					fd = fopen(fullpath, "r");
				}
			}
				break;
			
			case 2:
			{
				char *file = source_request_file(sourcename, m->sourcepath);
				if(file && strlen(file))
				{
					if(strcmp(IDOS->FilePart(file), IDOS->FilePart(sourcename)))
					{
						console_printf(OUTPUT_WARNING, "The file you have choosen has a different name!");
						return;
					}
					if(strlen(file) < strlen(sourcename))
					{
						console_printf(OUTPUT_WARNING, "Path too short!");
						return;
					}
					file[strlen(file)-strlen(sourcename)] = '\0';
					strcpy(fullpath, file);
					realpath = file;
					IDOS->AddPart(fullpath, sourcename, 1024);
					fd = fopen(fullpath, "r");
				}
			}
				break;
			
			case 0:
				console_printf(OUTPUT_WARNING, "User aborted!");
				return;
		}
	}
	if(fd)
	{
		m->sourcepath = freemem_strdup(modules_freemem_hook, realpath);
		if(e)
			e->sourcepath = m->sourcepath;
		IIntuition->SetAttrs(MainObj[GAD_SOURCE_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
		int sline = 1;
		while(1)
		{
			if(fgets(buffer, 1024, fd) != buffer)
				break;

			uint32 nline;
			struct stab_function *f;
			BOOL box = FALSE, check = FALSE;
			lb_userdata *lbnode = NULL;
			
	        if((f = stabs_sline_to_nline(sourcefile, sline, &nline, solname)) != 0 )
   		    {
       		    box = TRUE;
           		check = is_breakpoint_at(f->address + f->line[nline].adr);
           		
           		lbnode = freemem_malloc(source_freemem_hook, sizeof(lb_userdata));
    	    	if( lbnode )
	        	{
        			lbnode->sline = sline;
					lbnode->nline = nline;
					lbnode->func = f;
				}
				if(current_function == f)
					shownfunction = f;
       		}
       		else
            	box = FALSE;

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
		IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_SOURCE_LISTBROWSER], mainwin, NULL, LISTBROWSER_Labels, &source_list, TAG_END);
	}
	else
		console_printf(OUTPUT_WARNING, "Failed to open source file %s", name);
}

void source_show_currentline()
{
	if(current_function && current_function == shownfunction)
	{
		int line = current_function->line[current_function->currentline].infile;
	
		IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_SOURCE_LISTBROWSER], mainwin, NULL,
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

	IIntuition->GetAttr(LISTBROWSER_SelectedNode, MainObj[GAD_SOURCE_LISTBROWSER], (ULONG *) &node);
	if(!node)
		return;
	IIntuition->GetAttrs(MainObj[GAD_SOURCE_LISTBROWSER], LISTBROWSER_RelEvent, &event, TAG_DONE);
	
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

			if(f)
			{
				if(checked)
				{
					insert_breakpoint(f->address + f->line[nline].adr, BR_NORMAL_FUNCTION, (APTR)f, sline);
					console_printf(OUTPUT_SYSTEM, "Inserted break at addr f+%d", f->line[nline].adr);
				}
				else
					remove_breakpoint(f->address + f->line[nline].adr);
			}
		break;
	}
	source_show_currentline();
}

void source_update()
{
	if(!current_function)
		source_clear();
	else
	{
		if(shownfunction != current_function)
			source_load_file(current_function->sourcefile, current_function->line[current_function->currentline].solname);
		source_show_currentline();
	}
}
