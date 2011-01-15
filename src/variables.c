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

enum
{
    SYMBOL_BUTTON = 1,
    SYMBOL_LISTBROWSER
};

Object *LocalsWinObj, *LocalsButtonObj, *LocalsListBrowserObj;
struct Window *localswin = NULL;

struct List variablelist;
struct List loc_freelist;

struct ColumnInfo *localscolumninfo = NULL;

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


char *print_variable_value(struct stab_symbol *s)
{
    char *ret = IExec->AllocMem (256, MEMF_ANY|MEMF_CLEAR);

    uint32 buffer = 0x0;
    uint32 addr = 0x0;


    switch (s->location)
    {
    case L_STACK:
        {
        uint32 stackp = (uint32)process->pr_Task.tc_SPReg;
        uint32 tmpaddr = stackp + s->offset;

        buffer = *(uint32*)tmpaddr;
        addr = (uint32)&buffer;
        break;
        }

    case L_ABSOLUTE:
        {
        addr = s->address;
        break;
        }

    default:
        printf( "Not a known variable type" );
        break;
    }

    if (s->type == NULL)
    {
        //if we can't get the type, just print hex:
        uint32 unknownu32 = *(uint32*)addr;
        sprintf(ret, "0x%x (UNKNOWN)", unknownu32);
    }
    else
        switch (s->type->type)
        {
        case T_POINTER:
            {
            sprintf( ret, "0x%x", *(uint32*)addr);
            }
            break;
        case T_32:
            {
            int32 value32 = *(int32*)addr;
            sprintf(ret, "%d", value32);
            }
            break;

        case T_U32:
            {
            uint32 valueu32 = *(uint32*)addr;
            sprintf(ret, "%u", valueu32);
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
    
void locals_update_window()
{
    if (!localswin)
        return;

    struct Node *node;

    //if (has_variable_list)
    if(( !IsListEmpty( &variablelist )))
        locals_freelist();

    IExec->NewList (&variablelist);

    struct List *l = &(current_function->symbols);
    struct stab_symbol *s = (struct stab_symbol *)IExec->GetHead (l);

    if (s)
    while (1)
    {
        TEXT buf[1024];
        
        char *str = print_variable_value(s);
        //add_freelist (&loc_freelist, 256, str);
        //char *namestr = IExec->AllocMem (1024, MEMF_ANY|MEMF_CLEAR);
        //add_freelist (&loc_freelist, 1024, namestr);

        if (s->type && s->type->type == T_POINTER)
            IUtility->SNPrintf(buf, sizeof(buf), "*%s", s->name);
        else
            IUtility->SNPrintf(buf, sizeof(buf), "%s", s->name);

        if (node = IListBrowser->AllocListBrowserNode(2,
                                                LBNA_Column, 0,
                                                LBNCA_CopyText, TRUE,
                                                LBNCA_Text, buf,
                                                LBNA_Column, 1,
                                                LBNCA_CopyText, TRUE,
                                                LBNCA_Text, str,
                                                TAG_DONE))
                                    {
                                        IExec->AddTail(&variablelist, node);
                                    }
        if ((struct Node *)s == IExec->GetTail(l))
            break;
        s = (struct stab_symbol *)IExec->GetSucc((struct Node *)s);
    }

    if (localswin)
        IIntuition->RefreshGadgets ((struct Gadget *)LocalsListBrowserObj, localswin, NULL);
}

void locals_freelist()
{
    /* TODO: free strings */
    if(!IsListEmpty(&variablelist))
    {
        IListBrowser->FreeListBrowserList(&variablelist);

        freelist (&loc_freelist);
    }
}


void locals_open_window()
{
    if (localswin)
        return;

    if (!hasfunctioncontext)
        return;

    IExec->NewList (&variablelist);
    IExec->NewList (&loc_freelist);

    localscolumninfo = IListBrowser->AllocLBColumnInfo(2,
        LBCIA_Column, 0,
            LBCIA_Title, "Variable",
            LBCIA_Sortable, TRUE,
            LBCIA_AutoSort, TRUE,
            LBCIA_SortArrow, TRUE,
            LBCIA_DraggableSeparator, TRUE,
        LBCIA_Column, 1,
            LBCIA_Title, "Value",
        TAG_DONE);


    /* Create the window object. */
    if(( LocalsWinObj = WindowObject,
            WA_ScreenTitle, "Debug 101",
            WA_Title, "Locals",
            WA_Width, 400,
            WA_Height, 400,
            WA_DepthGadget, TRUE,
            WA_SizeGadget, TRUE,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_Activate, TRUE,
            WINDOW_Position, WPOS_CENTERSCREEN,
            /* there is no HintInfo array set up here, instead we define the 
            ** strings directly when each gadget is created (OM_NEW)
            */
            WINDOW_GadgetHelp, TRUE,
            WINDOW_ParentGroup, VLayoutObject,
                LAYOUT_SpaceOuter, TRUE,
                LAYOUT_DeferLayout, TRUE,

                LAYOUT_AddChild, LocalsListBrowserObj = ListBrowserObject,
                    GA_ID,                      SYMBOL_LISTBROWSER,
                    GA_RelVerify,               TRUE,
                    GA_TabCycle,                TRUE,
                    LISTBROWSER_AutoFit,        TRUE,
                    LISTBROWSER_Labels,         &variablelist,
                    LISTBROWSER_ColumnInfo,     localscolumninfo,
                    LISTBROWSER_ColumnTitles,   TRUE,
                    //LISTBROWSER_ShowSelected,   TRUE, /* not really needed yet */
                    LISTBROWSER_Striping,       LBS_ROWS,
                    LISTBROWSER_SortColumn,     0,
                    LISTBROWSER_TitleClickable, TRUE,
                ListBrowserEnd,

                LAYOUT_AddChild, LocalsButtonObj = ButtonObject,
                    GA_ID, SYMBOL_BUTTON,
                    GA_RelVerify, TRUE,
                    GA_Text, "Done",
                ButtonEnd,
                CHILD_WeightedHeight, 0,
            EndMember,
        EndWindow))
    {
        /*  Open the window. */
        if( localswin = (struct Window *) RA_OpenWindow(LocalsWinObj) )
        {
            locals_update_window();
        }
    }
}

uint32 locals_obtain_window_signal()
{
    uint32 signal = 0x0;
    if (localswin)
        IIntuition->GetAttr( WINDOW_SigMask, LocalsWinObj, &signal );

    return signal;
}


void locals_event_handler()
{
    ULONG wait, signal, result;
    BOOL done = FALSE;
    WORD Code;
    CONST_STRPTR hintinfo;
    int32 selected;
    
    /* Obtain the window wait signal mask. */
    //IIntuition->GetAttr( WINDOW_SigMask, WinObj, &signal );
    
    while ((result = RA_HandleInput(LocalsWinObj, &Code)) != WMHI_LASTMSG)
    {
        switch (result & WMHI_CLASSMASK)
        {
            case WMHI_CLOSEWINDOW:
                done = TRUE;
                break;
             case WMHI_GADGETUP:
                switch(result & WMHI_GADGETMASK)
                {
                    case SYMBOL_LISTBROWSER:
                        /* clicking variables is ignored for now */
                        break;
                        
                    case SYMBOL_BUTTON:
                        done = TRUE;
                        break;
                }
                break;
        }
        
        if( done )
        {
            locals_close_window();
        }
    }
}

void locals_close_window()
{
    if (localswin)
    {
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( LocalsWinObj );
        
        locals_freelist();
        IListBrowser->FreeLBColumnInfo(localscolumninfo);
        
        localswin = NULL;
    }
    
    return;

}

