#include <exec/types.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/window.h>
#include <proto/button.h>
#include <proto/string.h>
#include <proto/layout.h>
#include <proto/label.h>
#include <proto/listbrowser.h>
#include <proto/space.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>
#include <gadgets/space.h>
#include <gadgets/string.h>

#include <reaction/reaction_macros.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "stabs.h"
#include "tracking.h"
#include "console.h"
#include "freemem.h"
#include "modules.h"

enum
{
	MOD_WINDOW = 0,
	MOD_MODULES_LISTBROWSER,
	MOD_PATH_STRING,
	MOD_PATH_BUTTON,
	
	MOD_FLUSH_BUTTON,
	MOD_DELETE_BUTTON,
	MOD_NEW_BUTTON,
	MOD_NEW_STRING,
	
	MOD_SAVE_BUTTON,
	MOD_USE_BUTTON,
	
	MOD_NUM
};

Object *ModObj[MOD_NUM];
struct Window *modwin = NULL;
extern struct Window *mainwin;
struct List mod_listbrowser_list;
struct ColumnInfo *mod_colinfo = NULL;

char mod_string_buffer[1024] = "";

struct List modules_entries;

int modules_freemem_hook = -1;
extern int stabs_freemem_hook;

void modules_init()
{
	modules_freemem_hook = freemem_alloc_hook();
	IExec->NewList(&modules_entries);

    mod_colinfo = IListBrowser->AllocLBColumnInfo(2,
        LBCIA_Column, 0,
            LBCIA_Title, "Access",
 //           LBCIA_DraggableSeparator, TRUE,
        LBCIA_Column, 1,
            LBCIA_Title, "Path",
 //           LBCIA_DraggableSeparator, TRUE,
        TAG_DONE);
}

void modules_cleanup()
{
	IListBrowser->FreeLBColumnInfo(mod_colinfo);
	IExec->NewList(&modules_entries);
	freemem_free_hook(modules_freemem_hook);
	modules_freemem_hook = -1;
}

void modules_clear()
{
	freemem_clear(modules_freemem_hook);
	IExec->NewList(&modules_entries);
}

void modules_add_entry(char *name, char *sourcepath, BOOL accessallowed)
{
	struct modules_entry *e = modules_lookup_entry(name);
	if(!e)
	{
		e = freemem_malloc(modules_freemem_hook, sizeof(struct modules_entry));
		if(e)
		{
			//e->module = stabs_get_module(name);
			e->name = freemem_strdup(modules_freemem_hook, name);
			e->sourcepath = freemem_strdup(modules_freemem_hook, sourcepath);
			e->accessallowed = accessallowed;
			console_printf(OUTPUT_SYSTEM, "Adding module %s to modules list...", e->name);
			IExec->AddTail(&modules_entries, (struct Node *)e);
		}
	}
	else
		console_printf(OUTPUT_WARNING, "error: module entry already exists!");
}

void modules_remove_entry(char *name)
{
	struct modules_entry *e = (struct modules_entry *)IExec->GetHead(&modules_entries);
	while(e)
	{
		if(!strcmp(e->name, name))
		{
			IExec->Remove((struct Node *)e);
			return;
		}
		e = (struct modules_entry *)IExec->GetSucc((struct Node *)e);
	}
}

struct modules_entry *modules_lookup_entry(char *name)
{
	struct modules_entry *e = (struct modules_entry *)IExec->GetHead(&modules_entries);
	while(e)
	{
		if(!strcmp(e->name, name))
			return e;
		e = (struct modules_entry *)IExec->GetSucc((struct Node *)e);
	}
	return FALSE;
}

void modules_update()
{
	struct modules_entry *e = (struct modules_entry *)IExec->GetHead(&modules_entries);
	struct Node *n = IExec->GetHead(&mod_listbrowser_list);
	while(e)
	{
		struct stab_module *m = stabs_get_module(e->name);
		if(m)
		{
			m->sourcepath = freemem_strdup(stabs_freemem_hook, e->sourcepath);
			//m->accessallowed = e->accessallowed;
		}
		int check;
		if(n)
		{
			IListBrowser->GetListBrowserNodeAttrs(n, LBNA_Checked, &check, TAG_DONE);
			e->accessallowed = (BOOL)check;
		}
		n = IExec->GetSucc(n);
		e = (struct modules_entry *)IExec->GetSucc((struct Node *)e);
	}
}

static char *modheader = "DB101-MODULESLIST\n";

BOOL modules_load_list()
{
	modules_clear();

	BPTR fh = IDOS->FOpen("db101.modlist", MODE_OLDFILE, 2048);
	if(!fh)
		return FALSE;
	char buffer[2048] = "";
	IDOS->FGets(fh, buffer, 2048);
	if(strcmp(buffer, modheader))
	{
		printf("Wrong header!\n");
		goto fail;
	}
	while(IDOS->FGets(fh, buffer, 2048) > 0)
	{
		//parse string
		struct modules_entry *e = freemem_malloc(modules_freemem_hook, sizeof(struct modules_entry));
		char *ptr = buffer;
		if(!(ptr = skip_in_string(ptr, "\\")))
			goto fail;
		e->name = strdup_until(ptr, '\\');
		if(!(ptr = skip_in_string(ptr, "\\")))
			goto fail;
		e->sourcepath = strdup_until(ptr, '\\');
		if(!(ptr = skip_in_string(ptr, "\\")))
			goto fail;
		char *temp = strdup_until(ptr, '\\');
		e->accessallowed = (strcmp(temp, "TRUE") ? FALSE : TRUE);
		//e->module = stabs_get_module(e->name);
		e->hasbeenloaded = FALSE;
		IExec->AddTail(&modules_entries, (struct Node*)e);
	}
	IDOS->FClose(fh);
	return TRUE;

fail:
	IDOS->FClose(fh);
	printf("db101.modlist has wrong format!\n");
	modules_clear();
	return FALSE;
}

void modules_save_list()
{
	BPTR fh = IDOS->Open("db101.modlist", MODE_NEWFILE);
	if(!fh)
	{
		console_printf(OUTPUT_WARNING, "Failed to open db101.modlist for saving");
		return;
	}
	char buffer[2048];
	sprintf(buffer, "%s\n", modheader);
	IDOS->Write(fh, buffer, strlen(buffer));
	struct modules_entry *e = (struct modules_entry *)IExec->GetHead(&modules_entries);
	while(e)
	{
		sprintf(buffer, "\\%s\\%s\\%s\\\n", e->name, e->sourcepath, (e->accessallowed ? "TRUE" : "FALSE"));
		IDOS->Write(fh, buffer, strlen(buffer));
		e = (struct modules_entry *)IExec->GetSucc((struct Node *)e);
	}
	IDOS->Close(fh);
}

void modules_update_string()
{
	struct Node *n;
	IIntuition->GetAttr(LISTBROWSER_SelectedNode, ModObj[MOD_MODULES_LISTBROWSER], (uint32*)&n);
	if(n)
	{
		struct modules_entry *m;
   		IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &m);
 		IIntuition->SetAttrs(ModObj[MOD_PATH_STRING], STRINGA_TextVal, m->sourcepath, TAG_END);
   		IIntuition->RefreshGadgets((struct Gadget *)ModObj[MOD_PATH_STRING], modwin, NULL);
   	}
}

void modules_update_window()
{
	IIntuition->SetAttrs(ModObj[MOD_MODULES_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
	if(!IsListEmpty(&mod_listbrowser_list))
		IListBrowser->FreeListBrowserList(&mod_listbrowser_list);
	IExec->NewList(&mod_listbrowser_list);
	
	struct modules_entry *mod = (struct modules_entry *)IExec->GetHead(&modules_entries);
	while (mod)
	{
		struct Node *n;
		if(n = IListBrowser->AllocListBrowserNode(2,
												LBNA_UserData, mod,
												LBNA_CheckBox, TRUE,
												LBNA_Checked, (mod->accessallowed ? TRUE : FALSE),
												LBNA_Column, 1,
													LBNCA_CopyText, TRUE,
													LBNCA_Text, mod->name,
												TAG_DONE))
		{
			IExec->AddTail(&mod_listbrowser_list, n);
		}
		mod = (struct modules_entry *)IExec->GetSucc((struct Node *)mod);
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)ModObj[MOD_MODULES_LISTBROWSER], modwin, NULL,
													LISTBROWSER_Labels, &mod_listbrowser_list,
													TAG_END);
	modules_update_string();
}

#define SPACE LAYOUT_AddChild, SpaceObject, End

void modules_open_window()
{
	if(modwin)
		return;
	
	IExec->NewList(&mod_listbrowser_list);

	if(ModObj[MOD_WINDOW] = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Modules",
            WA_Width, 600,
            WA_Height, 400,
            WA_DepthGadget, TRUE,
            WA_SizeGadget, TRUE,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_Activate, TRUE,
            WINDOW_Position, WPOS_CENTERSCREEN,
            
            WINDOW_GadgetHelp, TRUE,
            WINDOW_ParentGroup, VLayoutObject,
            	LAYOUT_SpaceOuter, TRUE,
            	LAYOUT_DeferLayout, TRUE,
            	
            	LAYOUT_AddChild, HLayoutObject,
	            	LAYOUT_AddChild, ModObj[MOD_MODULES_LISTBROWSER] = ListBrowserObject,
    	        		GA_ID, MOD_MODULES_LISTBROWSER,
        	    		GA_RelVerify, TRUE,
            			GA_TabCycle, TRUE,
            			LISTBROWSER_AutoFit, TRUE,
                   		LISTBROWSER_ColumnInfo, mod_colinfo,
            			LISTBROWSER_ColumnTitles, TRUE,
            			LISTBROWSER_Labels, &mod_listbrowser_list,
            			LISTBROWSER_ShowSelected, TRUE,
            		ListBrowserEnd,
            	
	            	LAYOUT_AddChild, VLayoutObject,
	            		LAYOUT_AddChild, ModObj[MOD_PATH_STRING] = StringObject,
	            			GA_ID, MOD_PATH_STRING,
	            			GA_RelVerify, TRUE,
	            			STRINGA_Buffer, mod_string_buffer,
	            			STRINGA_TextVal, "",
	            			STRINGA_MinVisible, 10,
	            			STRINGA_MaxChars, 1024,
	            		StringEnd,
	            		CHILD_WeightedHeight, 0,
            		
	            		LAYOUT_AddChild, ModObj[MOD_PATH_BUTTON] = ButtonObject,
	            			GA_ID, MOD_PATH_BUTTON,
	            			GA_RelVerify, TRUE,
	            			GA_Text, "Select path",
	            		ButtonEnd,
	            		CHILD_WeightedHeight, 0,
	            		
	            		SPACE,
	            	EndMember,
	            EndMember,
	            
	            LAYOUT_AddChild, HLayoutObject,
	            	LAYOUT_AddChild, ModObj[MOD_FLUSH_BUTTON] = ButtonObject,
	            		GA_ID, MOD_FLUSH_BUTTON,
	            		GA_RelVerify, TRUE,
	            		GA_Text,	"Flush",
	            	ButtonEnd,
	            	
	            	LAYOUT_AddChild, ModObj[MOD_DELETE_BUTTON] = ButtonObject,
	            		GA_ID, MOD_DELETE_BUTTON,
	            		GA_RelVerify, TRUE,
	            		GA_Text,	"Delete",
	            	ButtonEnd,
	            	
	            	LAYOUT_AddChild, ModObj[MOD_NEW_STRING] = StringObject,
	            		GA_ID, MOD_NEW_STRING,
	            		STRINGA_TextVal, "",
	            		STRINGA_MinVisible, 10,
	            		STRINGA_MaxChars, 1024,
	            	StringEnd,
	            	CHILD_WeightedWidth, 40,
	            	
	            	LAYOUT_AddChild, ModObj[MOD_NEW_BUTTON] = ButtonObject,
	            		GA_ID, MOD_NEW_BUTTON,
	            		GA_RelVerify, TRUE,
	            		GA_Text, "New",
	            	ButtonEnd,
	            EndMember,
	            CHILD_WeightedHeight, 0,
	            
	            LAYOUT_AddChild, HLayoutObject,
	            	LAYOUT_AddChild, ModObj[MOD_SAVE_BUTTON] = ButtonObject,
	            		GA_ID, MOD_SAVE_BUTTON,
	            		GA_RelVerify, TRUE,
	            		GA_Text, "Save",
	            	ButtonEnd,
	            	LAYOUT_AddChild, ModObj[MOD_USE_BUTTON] = ButtonObject,
	            		GA_ID, MOD_USE_BUTTON,
	            		GA_RelVerify, TRUE,
	            		GA_Text, "Use",
	            	ButtonEnd,
	            EndMember,
	            CHILD_WeightedHeight, 0,
            EndGroup,
		EndWindow)
	{
		if(modwin = (struct Window *) RA_OpenWindow(ModObj[MOD_WINDOW]))
		{
			IIntuition->SetWindowAttr(mainwin, WA_BusyPointer, (APTR)TRUE, 0L);
			modules_update_window();
			modules_event_loop();
			modules_close_window();
			IIntuition->SetWindowAttr(mainwin, WA_BusyPointer, (APTR)FALSE, 0L);
		}
	}
}

char *modules_request_path(char *module, char *path)
{
	char buffer[1024];
	sprintf(buffer, "Select path for module %s", module);
	struct FileRequester *req = IAsl->AllocAslRequestTags(ASL_FileRequest,
														ASLFR_DrawersOnly, TRUE,
													  ASLFR_Window, modwin,
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

	ret = (char *)freemem_strdup(modules_freemem_hook, req->fr_Drawer);

	IAsl->FreeAslRequest(req);
	return ret;
}

void modules_event_loop()
{
	ULONG wait, signal;
    ULONG result;
    BOOL done = FALSE;
    WORD Code;
    
	IIntuition->GetAttr( WINDOW_SigMask, ModObj[MOD_WINDOW], &signal );

	while(!done)
	{
		wait = IExec->Wait(signal);

		if(wait & signal)
		{
		    while ((result = RA_HandleInput(ModObj[MOD_WINDOW], &Code)) != WMHI_LASTMSG)
		    {
		        switch (result & WMHI_CLASSMASK)
		        {
		            case WMHI_CLOSEWINDOW:
		            	modules_update();
		                done = TRUE;
		                break;
		                
		             case WMHI_GADGETUP:
		                switch(result & WMHI_GADGETMASK)
		                {
		                	case MOD_MODULES_LISTBROWSER:
								modules_update_string();
		                		break;
                		
		                	case MOD_PATH_STRING:
		                	{
		                		struct Node *n;
		                		IIntuition->GetAttr(LISTBROWSER_SelectedNode, ModObj[MOD_MODULES_LISTBROWSER], (uint32*)&n);
		                		if(n)
		                		{
			                		struct modules_entry *m;
			                		IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &m);
			                		m->sourcepath = freemem_strdup(modules_freemem_hook, mod_string_buffer);
			                	}
		                	}
		                		break;
                	
		                	case MOD_PATH_BUTTON:
	                		{
		                		struct Node *n;
		                		IIntuition->GetAttr(LISTBROWSER_SelectedNode, ModObj[MOD_MODULES_LISTBROWSER], (uint32*)&n);
		                		if(n)
		                		{
		                			struct modules_entry *m;
	    	            			IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &m);
	    	            			char *path = modules_request_path(m->name, m->sourcepath);
	    	            			if(path && strlen(path))
	    	            			{
			        	        		m->sourcepath = path;
			    	            		IIntuition->SetAttrs(ModObj[MOD_PATH_STRING], STRINGA_TextVal, m->sourcepath, TAG_END);
			    	            		IIntuition->RefreshGadgets((struct Gadget *)ModObj[MOD_PATH_STRING], modwin, NULL);
			        	        	}
		        	        	}
	        	        	}
		                		break;
		                	
		                	case MOD_FLUSH_BUTTON:
		                		modules_clear();
		                		modules_update_window();
		                		break;
		                	
		                	case MOD_DELETE_BUTTON:
		                	{
		                		struct Node *n;
		                		IIntuition->GetAttr(LISTBROWSER_SelectedNode, ModObj[MOD_MODULES_LISTBROWSER], (uint32*)&n);
		                		if(n)
		                		{
		                			struct modules_entry *m;
		                			IListBrowser->GetListBrowserNodeAttrs(n, LBNA_UserData, &m);
		                			if(m)
		                			{
		                				IExec->Remove((struct Node *)m);
		                				modules_update_window();
		                			}
		                		}
		                		break;
		                	}
		                	
		                	case MOD_NEW_BUTTON:
		                	{
		                		char *text = NULL;
		                		IIntuition->GetAttr(STRINGA_TextVal, ModObj[MOD_NEW_STRING], (uint32*)&text);
		                		if(text && strlen(text))
		                		{
									struct modules_entry *e = freemem_malloc(modules_freemem_hook, sizeof(struct modules_entry));
									e->name = freemem_strdup(modules_freemem_hook, text);
									e->accessallowed = FALSE;
									e->hasbeenloaded = FALSE;
									e->sourcepath = freemem_malloc(modules_freemem_hook, 1024);
									strcpy(e->sourcepath, "");
									IExec->AddTail(&modules_entries, (struct Node *)e);
									modules_update_window();
								}
							}
								break;
							
		                	case MOD_SAVE_BUTTON:
		                		modules_save_list();
		                		modules_update();
		                		done = TRUE;
		                		break;
		                	
		                	case MOD_USE_BUTTON:
		                		modules_update();
		                		done = TRUE;
		                		break;
		                }
				}
			}
		}
	}
}

void modules_close_window()
{
	if(modwin)
	{
		IIntuition->DisposeObject(ModObj[MOD_WINDOW]);
		if(!IsListEmpty(&mod_listbrowser_list))
			IListBrowser->FreeListBrowserList(&mod_listbrowser_list);
		modwin = NULL;
	}
}
