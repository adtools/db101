/*disassembler.c*/


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
#include "disassembler.h"
#include "freemem.h"


//Object *DisassemblerWinObj, *DisassemblerStepButtonObj, *DisassemblerSkipButtonObj, *MainObj[GAD_DISASSEMBLER_LISTBROWSER];
extern struct Window *mainwin;

int32 disassembler_selected = 0;

struct List disassembly_list;

int noinstructions = 0;
char opcodestr[15];
char operandstr[40];

extern struct stab_function *current_function;
extern struct DebugIFace *IDebug;

#define MAX(a, b)  (a>b?a:b)

void disassembler_show_selected()
{
	if (disassembler_selected >= 0)
	{
		//int32 top = MAX(disassembler_selected - 3, 0);
		IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_DISASSEMBLER_LISTBROWSER], mainwin, NULL,
												LISTBROWSER_Selected, disassembler_selected,
												LISTBROWSER_MakeVisible, disassembler_selected,
												TAG_DONE);
	}
}

void disassembler_makelist()
{
	disassembler_clear();
	
	struct Node *node;
	IExec->NewList (&disassembly_list);
	int i;
	APTR address,baseaddr;
	int max;
	if(hasfunctioncontext)
	{
		address = (APTR) current_function->address;
		max = 1024;
		baseaddr = address;
	}
	else
	{
		address = (APTR)context_copy.ip - 5*4;
		//printf("context_copy.ip = 0x%08x\n", context_copy.ip);
		max = 10;
		baseaddr = address;
	}

	IIntuition->SetAttrs(MainObj[GAD_DISASSEMBLER_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
	for (i = 0; i < max; i++)
	{
		char str[50];

		APTR newaddress = IDebug->DisassembleNative (address, opcodestr, operandstr);

		sprintf(str, "0x%08x (0x%08x): %s %s", address, address-baseaddr, opcodestr, operandstr);

		if (node = IListBrowser->AllocListBrowserNode(2,
            									LBNA_Column, 0,
            										LBNCA_CopyText, TRUE,
	                								LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&disassembly_list, node);
									}
		if (address == (APTR)context_copy.ip)
			disassembler_selected = i;

		address = newaddress;

		if (hasfunctioncontext)
			if ((uint32) (address - current_function->address) >= current_function->size)
				break;
	}
	IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_DISASSEMBLER_LISTBROWSER], mainwin, NULL,
												LISTBROWSER_Labels, &disassembly_list,
												TAG_END);
	disassembler_show_selected();
}

void disassembler_init()
{
	IExec->NewList(&disassembly_list);
}

void disassembler_cleanup()
{
	IListBrowser->FreeListBrowserList(&disassembly_list);
}

void disassembler_clear()
{
	IIntuition->SetAttrs(MainObj[GAD_DISASSEMBLER_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
	IListBrowser->FreeListBrowserList(&disassembly_list);
	IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_DISASSEMBLER_LISTBROWSER], mainwin, NULL,
												LISTBROWSER_Labels, &disassembly_list,
												TAG_END);
}
