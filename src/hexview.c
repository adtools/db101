/* hexview.c */

#include <exec/types.h>
#include <exec/lists.h>

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
#include <proto/chooser.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <gadgets/chooser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

#include <libraries/asl.h>
#include <interfaces/asl.h>

#include <libraries/elf.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "suspend.h"
#include "symbols.h"
#include "hexview.h"
#include "freemem.h"

enum
{
	HEX_BUTTON = 1,
	HEX_CHOOSER,
	HEX_LISTBROWSER
};

Object *HexWinObj, *HexChooserObj, *HexButtonObj, *HexListBrowserObj;
struct Window *hexwin;

BOOL hex_window_is_open = FALSE;
//int selected = 0;

struct List hexlist;
char *sectionlist[1024] = { NULL };
uint32 hex_numsections;

struct List hex_freelist;

struct TextAttr courier_font =
{
	"courier.font",
	15,
	FS_NORMAL,
	0x01
};



void interpret_chars (char *src, char *dest, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
		if (src[i] < 32)
			dest[i] = '.';
		else if (src[i] <= 127)
			dest[i] = src[i];
		else
			dest[i] = '.';
	}
}

void hex_init_section_list ()
{
	int index;
	Elf32_Shdr *header;
	int i;
	char *name, *strtable;
	struct Node *node;

	open_elfhandle();
	if (!exec_elfhandle)
		return;

	IElf->GetElfAttrsTags (exec_elfhandle,  EAT_NumSections, &hex_numsections,
											EAT_SectionStringTable, &index,
											TAG_DONE);
	strtable = IElf->GetSectionTags(exec_elfhandle, GST_SectionIndex, index, TAG_DONE);

	for (i = 0; i < hex_numsections; i++)
	{
		header = IElf->GetSectionHeaderTags (exec_elfhandle, GST_SectionIndex, i, TAG_DONE);

		if (header)
		{
			name = &strtable[header->sh_name];
			sectionlist[i] = strdup(name);
		}
	}
	sectionlist[i] = NULL;

	close_elfhandle(exec_elfhandle);
}

void hex_free_section_list ()
{
	int i;
	for (i = 0; i < hex_numsections; i++)
		if (sectionlist[i])
		{
			free (sectionlist[i]);
			sectionlist[i] = NULL;
		}
}


void hex_load_section (int index)
{
	open_elfhandle();
	if (!exec_elfhandle)
		return;

	uint32 *section = IElf->GetSectionTags(exec_elfhandle, GST_SectionIndex, index, TAG_DONE);
	Elf32_Shdr *header = IElf->GetSectionHeaderTags (exec_elfhandle, GST_SectionIndex, index, TAG_DONE);
	if (!section)
		return;

	uint32 size = header->sh_size;
	char *string, interpreted_string[1024];
	struct Node *node;
	int i;
	char *cptr;
	uint32 *dptr = section;

//printf("section = 0x%8x\n", section);
//printf("size = %d\n", size);


	IExec->NewList (&hexlist);

	for (i = 0; i < size; i += 16, dptr += 4)
	{
		string = IExec->AllocMem (1024, MEMF_ANY|MEMF_CLEAR);
		add_freelist (&hex_freelist, 1024, string);

		if (i - size < 16)
		{
			sprintf(string, "<dummy>");
		}
		else
		{
			interpret_chars ((char *)dptr, interpreted_string, 16);
			cptr = interpreted_string;
			sprintf(string, "0x%08x (0x%x): %08x %08x %08x %08x    %c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c",
							dptr,
							dptr - section,
							dptr[0],	
							dptr[1],
							dptr[2],
							dptr[3],
							cptr[0], cptr[1], cptr[2], cptr[3],
							cptr[4], cptr[5], cptr[6], cptr[7],
							cptr[8], cptr[9], cptr[10], cptr[11],
							cptr[12], cptr[13], cptr[14], cptr[15]);
		}		

		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
                								LBNCA_Text, string,
            									TAG_DONE))
        							{
							            IExec->AddTail(&hexlist, node);
									}
	}
	close_elfhandle(exec_elfhandle);
}

void hex_free_section()
{
	freelist(&hex_freelist);
}


void hex_open_window()
{
	if (hex_window_is_open)
		return;

	if (!task_exists)
		return;

	IExec->NewList (&hex_freelist);

	hex_init_section_list ();
	hex_load_section (0);

    /* Create the window object. */
    if(( HexWinObj = WindowObject,
            WA_ScreenTitle, "DEbug 101",
            WA_Title, "Hex Viewer",
            WA_Width, 750,
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

                LAYOUT_AddChild, HexChooserObj = ChooserObject,
                    GA_ID,               HEX_CHOOSER,
                    GA_RelVerify,        TRUE,
                    CHOOSER_LabelArray,  sectionlist,  // array of strings
                    CHOOSER_Selected,    0,
					CHOOSER_MaxLabels,	 40,
                    GA_Underscore,		 0,
                End,  // Chooser
                CHILD_NominalSize, TRUE,

	            LAYOUT_AddChild, HexListBrowserObj = ListBrowserObject,
    	            GA_ID,                     HEX_LISTBROWSER,
        	        GA_RelVerify,              TRUE,
            	    GA_TabCycle,               TRUE,
					GA_TextAttr,			   &courier_font,
                	LISTBROWSER_AutoFit,       TRUE,
                    LISTBROWSER_Labels,        &hexlist,
//                    LISTBROWSER_ColumnInfo,    NULL,
              	    LISTBROWSER_ColumnTitles,  TRUE,
                	LISTBROWSER_ShowSelected,  TRUE,
                    LISTBROWSER_Striping,      LBS_ROWS,
				ListBrowserEnd,

                LAYOUT_AddChild, HexButtonObj = ButtonObject,
                    GA_ID, HEX_BUTTON,
					GA_RelVerify, TRUE,
                    GA_Text, "Done",
                ButtonEnd,
                CHILD_WeightedHeight, 0,
			EndMember,
        EndWindow))
    {
        /*  Open the window. */
        if( hexwin = (struct Window *) RA_OpenWindow(HexWinObj) )
        	hex_window_is_open = TRUE;
	}
	return;
}

uint32 hex_obtain_window_signal()
{
	uint32 signal = 0x0;
	if (hex_window_is_open)
		IIntuition->GetAttr( WINDOW_SigMask, HexWinObj, &signal );

	return signal;
}

void hex_event_handler()
{
            ULONG wait, signal, result, done = FALSE;
            WORD Code;
            CONST_STRPTR hintinfo;
			uint32 selected;
            
            /* Obtain the window wait signal mask. */
            //IIntuition->GetAttr( WINDOW_SigMask, HexWinObj, &signal );
            
            /* Input Event Loop */
                while ((result = RA_HandleInput(HexWinObj, &Code)) != WMHI_LASTMSG)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;
                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {
								case HEX_CHOOSER:

									IIntuition->GetAttrs( HexChooserObj, CHOOSER_Selected, &selected, TAG_DONE );
									hex_free_section();
									hex_load_section (selected);
									IIntuition->RefreshGadgets ((struct Gadget *)HexListBrowserObj, hexwin, NULL);

									break;

                                case HEX_BUTTON:
                                    /* if the user has entered a new text for the buttons
                                    ** help text, get it, and set it
                                    */
									done = TRUE;
                                    break;
                            }
                            break;
                    }
					if (done)
					{
						hex_close_window();
						break;
					}
                }
}

void hex_close_window()
{
	if (hex_window_is_open)
	{
		hex_free_section();
        hex_free_section_list();


        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( HexWinObj );

		hex_window_is_open = FALSE;
    }
    
    return;
}
