/*variables.c*/


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
#include <proto/utility.h>


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
#include "gui.h"
#include "variables.h"
#include "freemem.h"


#define dprintf(format...)	IExec->DebugPrintF(format)


Object *VariablesListBrowserObj;
extern struct Window *mainwin;

struct List variable_list;
int locals_freemem_hook = -1;

struct ColumnInfo *variablescolumninfo;

char *vardummytext = "dummy";

struct Node *params_node = NULL;
struct Node *locals_node = NULL;
struct Node *globals_node = NULL;
struct Node *registers_node = NULL;
int globals_list_populated = 0;
int locals_list_populated = 0;
int params_list_populated = 0;

extern struct stab_function *current_function;
struct stab_function *variables_shown_function = NULL;

/********************************************************************/

BOOL is_readable_address (uint32 addr)
{
    uint32 attr;
    APTR stack;
    BOOL ret = FALSE;

      /* Go supervisor */
    stack = IExec->SuperState();
    
    attr = IMMU->GetMemoryAttrs((APTR)addr, 0);

    /* Return to old state */
    if (stack)
        IExec->UserState(stack);

    if (attr & MEMATTRF_RW_MASK)
        ret = TRUE;

    return ret;
}

uint32 get_pointer_value(struct stab_symbol *s)
{
    if(s->type == NULL)
    {
    	return -1;
    }
	switch (s->location)
    {
    case L_STACK:
        {
        uint32 stackp = (uint32)process->pr_Task.tc_SPReg;
        uint32 addr = stackp + s->offset;

		if(!is_readable_address(addr))
			return 0x0;
        return *(uint32 *)addr;
        }

    case L_ABSOLUTE:
        {
        uint32 addr = s->address;
        if(!is_readable_address(addr))
        	return 0x0;
		return *(uint32 *)addr;
        }
	case L_POINTER:
		{
		uint32 addr = get_pointer_value(s->pointer) + s->offset;
		return addr;
		}
//    case L_REGISTER:

    default:
        printf( "Not a known variable type" );
        break;
    }
    return -1;
}

char *print_variable_value(struct stab_symbol *s)
{
    char *ret = freemem_malloc(locals_freemem_hook, 256);
    uint32 *addr = 0x0;

    switch (s->location)
    {
    case L_STACK:
        {
        uint32 stackp = (uint32)process->pr_Task.tc_SPReg;
	    addr = (uint32 *)(stackp + s->offset);
        break;
        }

    case L_ABSOLUTE:
        {
        addr = (uint32 *)s->address;
        break;
        }
	case L_POINTER:
		{
		addr = (uint32 *)get_pointer_value(s);
		break;
		}
    default:
        printf( "Not a known variable type" );
        break;
    }
    
    if(!is_readable_address((uint32)addr))
    	return "<ACCESS DENIED>";
    
    if (s->type == NULL)
    {
        sprintf(ret, "(no type)");
    }
    else
        switch (s->type->type)
        {
        case T_POINTER:
            {
			sprintf( ret, "(*) 0x%x", *addr);
            }
            break;
            
		case T_VOID:
			{
			sprintf( ret, "(void)");
			}
			break;
			
		case T_ENUM:
			{
			struct stab_enum *enu = s->type->enum_ptr;
			if(s->type->points_to)
				enu = s->type->points_to->enum_ptr;

            int32 value32 = *(int32*)addr;
			struct stab_enum_element *e = (struct stab_enum_element *)IExec->GetHead(&(enu->list));
			if(e)
			while(1)
			{
				if(value32 == e->value)
				{
					sprintf(ret, "%s (%d)", e->name, value32);
					return ret;
				}
				if(e == (struct stab_enum_element *)IExec->GetTail(&(enu->list)))
					break;
				e = (struct stab_enum_element *)IExec->GetSucc((struct Node *)e);
			}
			sprintf(ret, "<unknown> (%d)", value32);
			}
			break;
		case T_STRUCT:
			sprintf(ret, "(struct)");
			break;
		case T_UNION:
			sprintf(ret, "(union)");
			break;
		case T_32:
            {
            int32 value32 = *(int32*)addr;
            sprintf(ret, "%d", value32);
            }
            break;

        case T_U32:
            {
            uint32 valueu32 = *addr;
            sprintf(ret, "0x%x", valueu32);
            }
            break;

        case T_16:
            {
            int16 value16 = *(int16*)addr;
            int32 convert16 = value16;
            sprintf(ret, "%d", convert16);
            }
            break;

        case T_U16:
            {
            uint16 valueu16 = *(uint16*)addr;
            uint32 convertu16 = valueu16;
            sprintf(ret, "%u", convertu16);
            }
            break;

        case T_8:
            {
            signed char value8 = *(signed char*)addr;
            int32 convert8 = value8;
            sprintf(ret, "%d", convert8);
            }
            break;

        case T_U8:
            {
            char valueu8 = *(char*)addr;
            uint32 convertu8 = valueu8;
            sprintf(ret, "%u (%c)", convertu8, valueu8);
            }
            break;

        case T_FLOAT32:
            {
            float valuef32 = *(float*)addr;
            sprintf(ret, "%e", valuef32);
            }
            break;

        case T_FLOAT64:
            {
            double valuef64 = *(double*)addr;
            sprintf(ret, "%f", valuef64);
            }
            break;

        default:
            {
            //if we can't get the type, just print hex:
            uint32 unknownu32 = *(uint32*)addr;
            sprintf(ret, "0x%x (UNKNOWN)", unknownu32);
            }
            break;
        }

    return ret;
}

struct Node *variables_add_children(struct Node *n, struct stab_symbol *s, int32 gen, int hidden)
{
	uint32 hidflag = (hidden ? LBFLG_HIDDEN : 0);
	struct Node *node;

	if(!s->type)
	{
		if (node = IListBrowser->AllocListBrowserNode(2,
		        								LBNA_Generation, gen+1,
		        								LBNA_Flags, hidflag,
                                                LBNA_Column, 0,
                                                LBNA_UserData, NULL,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, s->name,
                                                LBNA_Column, 1,
                                               		LBNCA_Text, "(unknown type)",
                                                TAG_DONE))
                                    {
                                    	if(n)
	                                        IExec->Insert(&variable_list, node, n);
	                                    else
	                                    	IExec->AddTail(&variable_list, node);
                                    }
		return node;
	}
	switch(s->type->type)
	{
      	case T_UNKNOWN:
    	case T_VOID:
    	case T_BOOL:
	    case T_U8:
	    case T_8:
	    case T_U16:
	    case T_16:
	    case T_U32:
	    case T_32:
	    case T_U64:
	    case T_64:
	    case T_FLOAT32:
	    case T_FLOAT64:
	    case T_ENUM:
	   	{
			TEXT buf[1024];
	        IUtility->SNPrintf(buf, sizeof(buf), "%s", s->name);

			char *str = print_variable_value(s);
			
            struct variables_userdata *u = freemem_malloc(locals_freemem_hook, sizeof(struct variables_userdata));
            u->s = s;
            u->haschildren = 0;
            
			if (node = IListBrowser->AllocListBrowserNode(2,
		        								LBNA_Generation, gen+1,
		        								LBNA_Flags, hidflag,
                                                LBNA_Column, 0,
                                                LBNA_UserData, u,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, buf,
                                                LBNA_Column, 1,
                                               		LBNCA_Text, str,
                                                TAG_DONE))
                                    {
                                    	if(n)
	                                        IExec->Insert(&variable_list, node, n);
	                                    else
	                                    	IExec->AddTail(&variable_list, node);
                                    }
		}
		break;
				
		case T_POINTER:
		{
			TEXT buf[1024];
	        IUtility->SNPrintf(buf, sizeof(buf), "%s", s->name);
	        
		    struct variables_userdata *u = freemem_malloc(locals_freemem_hook, sizeof(struct variables_userdata));
            u->s = s;
            u->haschildren = 1;
            u->isopen = 0;
            
			char *str = print_variable_value(s);
	        if (node = IListBrowser->AllocListBrowserNode(2,
		        								LBNA_Generation, gen+1,
		        								LBNA_Flags, LBFLG_HASCHILDREN|hidflag,
		        								LBNA_UserData, u,
                                                LBNA_Column, 0,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, buf,
                                                LBNA_Column, 1,
                                                	LBNCA_Text, str,
                                                TAG_DONE))
                                    {
                                    	if(n)
                                    		IExec->Insert(&variable_list, node, n);
                                    	else
	                                        IExec->AddTail(&variable_list, node);
                                    }

		}
		break;
		
		case T_STRUCT:
		case T_UNION:
		{
			TEXT buf[1024];
	        IUtility->SNPrintf(buf, sizeof(buf), "%s", s->name);

			char *str = " < struct > ";
			
            struct variables_userdata *u = freemem_malloc(locals_freemem_hook, sizeof(struct variables_userdata));
            u->s = s;
            u->haschildren = 1;
            u->isopen = 1;
            
			if (node = IListBrowser->AllocListBrowserNode(2,
		        								LBNA_Generation, gen+1,
		        								LBNA_Flags, LBFLG_HASCHILDREN|hidflag,
                                                LBNA_Column, 0,
                                                LBNA_UserData, u,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, buf,
                                                LBNA_Column, 1,
                                               		LBNCA_Text, str,
                                                TAG_DONE))
            {
               	if(n)
	        	    IExec->Insert(&variable_list, node, n);
	            else
	              	IExec->AddTail(&variable_list, node);
            }
	
			struct stab_structure *stru = s->type->struct_ptr;
			if(s->type->points_to)
				stru = s->type->points_to->struct_ptr;

			struct stab_struct_element *e = (struct stab_struct_element *)IExec->GetHead(&(stru->list));
			if(e)
			while(1)
			{
	    	    struct variables_userdata *u = freemem_malloc(locals_freemem_hook, sizeof(struct variables_userdata));
		    	struct stab_symbol *news = freemem_malloc(locals_freemem_hook, sizeof(struct stab_symbol));
		    	news->name = e->name;
		    	news->type = e->type;
		    	switch(s->location)
		    	{
		    		case L_ABSOLUTE:
		    			news->location = L_ABSOLUTE;
		    			news->address = s->address + (e->bitoffset >> 3);
		    			break;
		    		case L_STACK:
				        news->location = L_STACK;
        				news->offset = s->offset + (e->bitoffset >> 3);
        				break;
        			case L_POINTER:
        				news->location = L_POINTER;
        				news->offset = (e->bitoffset >> 3);
        				news->pointer = s->pointer;
        				break;
        		}

		    	node = variables_add_children(node, news, gen+1, TRUE);
				
				if(e == (struct stab_struct_element *)IExec->GetTail(&(stru->list)))
					break;
				e = (struct stab_struct_element *)IExec->GetSucc((struct Node *)e);
			}

		}
		break;

		default:
			break;
	}
	return node;
}

struct Node *variables_add_pointer_children(struct Node *n, struct stab_symbol *s, int32 gen, int hidden)
{
    struct stab_symbol *news = freemem_malloc(locals_freemem_hook, sizeof(struct stab_symbol));
    news->name = " - ";
    news->type = s->type->points_to;
	news->location = L_POINTER;
	news->pointer = s;
	news->offset = 0;
	return variables_add_children(n, news, gen, hidden);
}

void params_populate_list(struct Node *node)
{
    struct Node *n = node;

    struct List *l = &(current_function->params);
    struct stab_symbol *s = (struct stab_symbol *)IExec->GetHead (l);

	IIntuition->SetAttrs(VariablesListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
    while (s)
    {
    	n = variables_add_children(n, s, 1, FALSE); //not hidden
    	
        s = (struct stab_symbol *)IExec->GetSucc((struct Node *)s);
    }
	IIntuition->SetGadgetAttrs((struct Gadget *)VariablesListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &variable_list, TAG_END);
	
	params_list_populated = 1;
}

void locals_populate_list(struct Node *node)
{
    struct Node *n = node;

    struct List *l = &(current_function->symbols);
    struct stab_symbol *s = (struct stab_symbol *)IExec->GetHead (l);

	IIntuition->SetAttrs(VariablesListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
    while (s)
    {
    	n = variables_add_children(n, s, 1, FALSE); //not hidden
    	
        s = (struct stab_symbol *)IExec->GetSucc((struct Node *)s);
    }
	IIntuition->SetGadgetAttrs((struct Gadget *)VariablesListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &variable_list, TAG_END);
	
	locals_list_populated = 1;
}

void globals_populate_list(struct Node *node)
{
    struct Node *n = node;

    struct List *l = &global_symbols;
    struct stab_symbol *s = (struct stab_symbol *)IExec->GetHead (l);

	IIntuition->SetAttrs(VariablesListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
    while (s)
    {
    	n = variables_add_children(n, s, 1, FALSE); //not hidden
        s = (struct stab_symbol *)IExec->GetSucc((struct Node *)s);
    }
	IIntuition->SetGadgetAttrs((struct Gadget *)VariablesListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &variable_list, TAG_END);
	
	globals_list_populated = 1;
}

void add_register(char *reg, char *value)
{
	struct Node *node;
	if (node = IListBrowser->AllocListBrowserNode(2,
												LBNA_Generation, 3,
												LBNA_Flags, LBFLG_HIDDEN,
            									LBNA_Column, 0,
            										LBNCA_CopyText, TRUE,
                									LBNCA_Text, reg,
												LBNA_Column, 1,
													LBNCA_CopyText, TRUE,
													LBNCA_Text, value,
            									TAG_DONE))
        							{
							            IExec->AddTail(&variable_list, node);
									}
}


void registers_populate_list()
{
	IIntuition->SetAttrs(VariablesListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);

	char tempstr[256];
	char tempstr2[256];
	struct Node *node;
	
	if (node = IListBrowser->AllocListBrowserNode(2,
												LBNA_Generation, 2,
												LBNA_Flags, LBFLG_HIDDEN|LBFLG_HASCHILDREN,
            									LBNA_Column, 0,
            										LBNCA_CopyText, TRUE,
                									LBNCA_Text, "system",
            									TAG_DONE))
        							{
							            IExec->AddTail(&variable_list, node);
									}

	sprintf(tempstr, "0x%x", context_copy.msr);
		add_register ("msr", tempstr);
	sprintf(tempstr, "0x%x", context_copy.ip);
		add_register ("ip", tempstr);
	sprintf(tempstr, "0x%x", context_copy.cr);
		add_register ("cr", tempstr);
	sprintf(tempstr, "0x%x", context_copy.xer);
		add_register ("xer", tempstr);
	sprintf(tempstr, "0x%x", context_copy.ctr);
		add_register ("ctr", tempstr);
	sprintf(tempstr, "0x%x", context_copy.lr);
		add_register ("lr", tempstr);

	if (node = IListBrowser->AllocListBrowserNode(2,
												LBNA_Generation, 2,
												LBNA_Flags, LBFLG_HIDDEN|LBFLG_HASCHILDREN,
            									LBNA_Column, 0,
            										LBNCA_CopyText, TRUE,
                									LBNCA_Text, "gpr",
            									TAG_DONE))
        							{
							            IExec->AddTail(&variable_list, node);
									}

	int i;
	for (i = 0; i < 32; i++)
	{
		sprintf(tempstr, "0x%x", context_copy.gpr[i]);
		sprintf(tempstr2, "gpr%d", i);
			add_register (tempstr2, tempstr);
	}

	if (node = IListBrowser->AllocListBrowserNode(2,
												LBNA_Generation, 2,
												LBNA_Flags, LBFLG_HIDDEN|LBFLG_HASCHILDREN,
            									LBNA_Column, 0,
            										LBNCA_CopyText, TRUE,
                									LBNCA_Text, "fpr",
            									TAG_DONE))
        							{
							            IExec->AddTail(&variable_list, node);
									}

	for (i = 0; i < 32; i++)
	{
		sprintf(tempstr, "%e", context_copy.fpr[i]);
		sprintf(tempstr2, "fpr%d", i);
			add_register (tempstr2, tempstr);
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)VariablesListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &variable_list, TAG_END);
}

void update_register(struct Node *n, char *valuestr)
{
	IListBrowser->SetListBrowserNodeAttrs(n,
										LBNA_Column, 1,
											LBNCA_Text, valuestr,
										TAG_END);
}

void registers_update()
{
	struct Node *n = IExec->GetSucc(registers_node);

	char tempstr[256];
	
	n = IExec->GetSucc(n);
	sprintf(tempstr, "0x%x", context_copy.msr);
		update_register(n, tempstr);
		n = IExec->GetSucc(n);
	sprintf(tempstr, "0x%x", context_copy.ip);
		update_register (n, tempstr);
		n = IExec->GetSucc(n);
	sprintf(tempstr, "0x%x", context_copy.cr);
		update_register (n, tempstr);
		n = IExec->GetSucc(n);
	sprintf(tempstr, "0x%x", context_copy.xer);
		update_register (n, tempstr);
		n = IExec->GetSucc(n);
	sprintf(tempstr, "0x%x", context_copy.ctr);
		update_register (n, tempstr);
		n = IExec->GetSucc(n);
	sprintf(tempstr, "0x%x", context_copy.lr);
		update_register (n, tempstr);
		n = IExec->GetSucc(n);

	n = IExec->GetSucc(n);
	int i;
	for (i = 0; i < 32; i++)
	{
		sprintf(tempstr, "0x%x", context_copy.gpr[i]);
			update_register (n, tempstr);
			n = IExec->GetSucc(n);
	}
	n = IExec->GetSucc(n);
	for (i = 0; i < 32; i++)
	{
		sprintf(tempstr, "%e", context_copy.fpr[i]);
			update_register (n, tempstr);
			n = IExec->GetSucc(n);
	}
}	

void variables_init_parents()
{
	if (params_node = IListBrowser->AllocListBrowserNode(2,
		        								LBNA_Generation, 1,
		        								LBNA_Flags, LBFLG_HASCHILDREN,
                                                LBNA_Column, 0,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, "parameters",
                                                TAG_DONE))
    {
       	IExec->AddTail(&variable_list, params_node);
    }
	if (locals_node = IListBrowser->AllocListBrowserNode(2,
		        								LBNA_Generation, 1,
		        								LBNA_Flags, LBFLG_HASCHILDREN,
                                                LBNA_Column, 0,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, "locals",
                                                TAG_DONE))
    {
       	IExec->AddTail(&variable_list, locals_node);
    }
	if (globals_node = IListBrowser->AllocListBrowserNode(2,
		        								LBNA_Generation, 1,
		        								LBNA_Flags, LBFLG_HASCHILDREN,
                                                LBNA_Column, 0,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, "globals",
                                                TAG_DONE))
    {
       	IExec->AddTail(&variable_list, globals_node);
    }
    if(registers_node = IListBrowser->AllocListBrowserNode(2,
		        								LBNA_Generation, 1,
		        								LBNA_Flags, LBFLG_HASCHILDREN,
                                                LBNA_Column, 0,
                                                	LBNCA_CopyText, TRUE,
                                                	LBNCA_Text, "registers",
                                                TAG_DONE))
    {
       	IExec->AddTail(&variable_list, registers_node);
    }
}

void update_variables()
{
	IIntuition->SetAttrs(VariablesListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
	struct Node *n = IExec->GetHead(&variable_list);
	while(n)
	{
		if (n == registers_node)
            break;

		if(n != locals_node && n != globals_node)
		{
			struct variables_userdata *u;
			uint32 flags;
			IListBrowser->GetListBrowserNodeAttrs(n,
												LBNA_UserData, &u,
												LBNA_Flags, &flags,
												TAG_END);
			if(u && u->s /*&& !u->haschildren*/)
			{
				char *str = print_variable_value(u->s);
				IListBrowser->SetListBrowserNodeAttrs(n,
												LBNA_Column, 1,
													LBNCA_Text, str,
												TAG_END);
			}
		}
        n = IExec->GetSucc(n);
	}
	registers_update();
	IIntuition->SetGadgetAttrs((struct Gadget *)VariablesListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &variable_list, TAG_END);
}
	
void locals_update()
{
	if(current_function != variables_shown_function)
	{
		//if function has changed:
		locals_clear();
	    IExec->NewList (&variable_list);
	    globals_list_populated = 0;
	    locals_list_populated = 0;
	    params_list_populated = 0;
	    variables_init_parents();
		//locals_populate_list(locals_node);
		registers_populate_list();
	}
	else
	{
		update_variables();
	}
	variables_shown_function = current_function;
}

void locals_handle_input()
{
	struct Node *n;
	if(IIntuition->GetAttr(LISTBROWSER_SelectedNode, VariablesListBrowserObj, (uint32 *)&n))
	{
		if(n == globals_node && !globals_list_populated)
		{
			globals_populate_list(globals_node);
			return;
		}
		if(n == locals_node && !locals_list_populated)
		{
			locals_populate_list(locals_node);
			return;
		}
		if(n == params_node && !params_list_populated)
		{
			params_populate_list(params_node);
			return;
		}
		struct variables_userdata *u;
		int32 gen;
		IListBrowser->GetListBrowserNodeAttrs(n,
												LBNA_UserData, &u,
												LBNA_Generation, &gen,
												TAG_END);
		if(u && u->haschildren && !u->isopen)
		{
			IIntuition->SetAttrs(VariablesListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
			if(u->s->type->type == T_POINTER)
				variables_add_pointer_children(n, u->s, gen, FALSE);
			else
				variables_add_children(n, u->s, gen, FALSE);
			u->isopen = 1;
			IIntuition->SetGadgetAttrs((struct Gadget *)VariablesListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &variable_list, TAG_END);
		}
	}
}

void locals_init()
{
	IExec->NewList(&variable_list);
	locals_freemem_hook = freemem_alloc_hook();
	
    variablescolumninfo = IListBrowser->AllocLBColumnInfo(2,
        LBCIA_Column, 0,
            LBCIA_Title, "Variable",
			LBCIA_Width, 200,
            LBCIA_DraggableSeparator, TRUE,
        LBCIA_Column, 1,
            LBCIA_Title, "Value",
        TAG_DONE);
}

void locals_cleanup()
{
	IListBrowser->FreeLBColumnInfo(variablescolumninfo);
	IListBrowser->FreeListBrowserList(&variable_list);
	if(locals_freemem_hook >= 0)
		freemem_free_hook(locals_freemem_hook);
}

void locals_clear()
{
	variables_shown_function = NULL;
	IIntuition->SetAttrs(VariablesListBrowserObj, LISTBROWSER_Labels, ~0, TAG_DONE);
    if(!IsListEmpty(&variable_list))
    {
        IListBrowser->FreeListBrowserList(&variable_list);
		freemem_clear(locals_freemem_hook);
    }
	IIntuition->SetGadgetAttrs((struct Gadget *)VariablesListBrowserObj, mainwin, NULL, LISTBROWSER_Labels, &variable_list, TAG_END);
}
