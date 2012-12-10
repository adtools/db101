/*breakpoints.c*/

#include <proto/exec.h>
#include <exec/lists.h>
#include <proto/intuition.h>
#include <proto/requester.h>
#include <classes/requester.h>
#include <dos/dos.h>

#include <string.h>
#include <stdio.h>

#include "suspend.h"
#include "breakpoints.h"
#include "gui.h"
#include "stabs.h"
#include "freemem.h"

#define dprintf(format, args...) ((struct ExecIFace *)((*(struct ExecBase **)4)->MainInterface))->DebugPrintF("[%s] " format, __PRETTY_FUNCTION__ , ## args)

extern struct Window *mainwin;

extern BOOL is_readable_address (uint32);

extern struct DebugIFace *IDebug;

struct List breakpoint_list;
BOOL breakpoints_installed = FALSE;

BOOL has_tracebit = TRUE;

void init_breakpoints()
{
	IExec->NewList(&breakpoint_list);
	
	uint32 family;
	IExec->GetCPUInfoTags(GCIT_Family, &family, TAG_DONE);
	if(family == CPUFAMILY_4XX)
		has_tracebit = FALSE;
}

BOOL is_breakpoint_at(uint32 addr)
{
	BOOL ret = FALSE;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			if (addr == b->address)
			{
				ret = TRUE;
				break;
			}

			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
	}
	return ret;
}

void install_all_breakpoints()
{
	if (breakpoints_installed)
		return;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			memory_insert_breakpoint (b->address, &(b->save_buffer));

			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
		breakpoints_installed = TRUE;
	}
}

void suspend_all_breakpoints()
{
	if (!breakpoints_installed)
		return;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			memory_remove_breakpoint (b->address, &(b->save_buffer));

			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
		breakpoints_installed = FALSE;
	}
}

void insert_line_breakpoints()
{
	struct stab_function *f = current_function;
	uint32 i = f->currentline;
	if(i == f->numberoflines-1)
		return;

	if (f->line[i].type == LINE_LOOP)
	{
		uint32 newline = translate_sline_to_nline (f->line[i].infile);
		insert_breakpoint (f->address + f->line[newline+1].adr, BR_LINE, NULL, 0);
		insert_breakpoint (f->address + f->line[i+1].adr, BR_LINE, NULL, 0);
	}
	else
		insert_breakpoint (f->address + f->line[i+1].adr, BR_LINE, NULL, 0);
}

void remove_line_breakpoints()
{
	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	if (node)
	{
		while(1)
		{
			b = (struct breakpoint *)node;

			if (b->type == BR_LINE)
			{
				IExec->Remove(node);
				node = IExec->GetHead (&breakpoint_list);
			}
			if (node == IExec->GetTail(&breakpoint_list))
				break;
			node = IExec->GetSucc (node);
		}
	}
}

uint32 translate_sline_to_nline(uint32 sline)
{
	if (!hasfunctioncontext)
		return;
	struct stab_function *f = current_function;

	int i;
	for (i = 0; i < f->numberoflines; i++)
		if (f->line[i].infile == sline)
			return i;
}

int get_nline_from_address(uint32 addr)
{
	if (!hasfunctioncontext)
		return -1;

	struct stab_function *f = current_function;
	int i;
	for (i=0; i < f->numberoflines; i++)
		if (f->address + f->line[i].adr == addr)
			return i;
	return -1;
}


int guess_line_in_function ()
{
	if (!hasfunctioncontext)
		return;
	struct stab_function *f = current_function;
	uint32 ip = context_copy.ip;

	if (! (ip >= f->address && ip < f->address + f->size))
	{
		f->currentline = -1;
		return -1;
	}

	int i;
	for (i=0; i < f->numberoflines-1; i++)
	{
		if ( ip >= f->address + f->line[i].adr && ip < f->address + f->line[i+1].adr)
		{
			f->currentline = i;
			return i;
		}
	}

	f->currentline = f->numberoflines-1;
}

BOOL breakpoint_line_in_file (uint32 sline, char *file)
{
	uint32 nline;
	struct stab_function *f = stabs_sline_to_nline(file, sline, &nline);
	if(f)
	{
		insert_breakpoint (f->address + f->line[nline].adr, BR_NORMAL_FUNCTION, (APTR)f, sline);
		return TRUE;
	}
	return FALSE;
}

BOOL breakpoint_function (char *fname)
{
	struct stab_function *f = stabs_get_function_from_name(fname);
	if(f)
	{
		uint32 sline = 0;
		if(f->line)
			sline = f->line[0].infile;
		insert_breakpoint (f->address, BR_NORMAL_FUNCTION, f, sline);
		return TRUE;
	}
	return FALSE;
}


void insert_breakpoint(uint32 addr, uint32 type, APTR object, uint32 line)
{
	if(is_breakpoint_at(addr))
		return;

	struct breakpoint *b = IExec->AllocVecTags (sizeof(struct breakpoint), TAG_DONE);

	b->address = addr;
	b->type = type;
	b->save_buffer = 0;
	switch(type)
	{
		case BR_NORMAL_SYMBOL:
			b->symbol = (struct amigaos_symbol *)object;
			break;
		case BR_NORMAL_FUNCTION:
			b->function = (struct stab_function *)object;
			b->line = line;
			break;
	}
	IExec->AddTail (&breakpoint_list, (struct Node *)b);
}

void remove_breakpoint(uint32 addr)
{
	if (!is_breakpoint_at(addr))
		return;

	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct breakpoint *b;

	while (node)
	{
		b = (struct breakpoint *)node;

		if (addr == b->address)
		{
			IExec->Remove (node);
			IExec->FreeVec (b);	
			break;
		}

		node = IExec->GetSucc (node);
	}
}

void remove_breakpoint_node(struct breakpoint *p)
{
	if(p)
		IExec->Remove((struct Node *)p);
}

void free_breakpoints ()
{
	struct Node *node = IExec->GetHead(&breakpoint_list);
	struct Node *next;
	struct breakpoint *b;

	while (node)
	{
		next = IExec->GetSucc (node);
		b = (struct breakpoint *)node;
		IExec->Remove (node);
		IExec->FreeVec (b);		

		node = next;
	}
}

uint32 stepout_address, stepout_buffer;
BOOL stepout_installed = FALSE;

void stepout_install()
{
	stepout_address = context_copy.lr;
	if((hasfunctioncontext && stabs_get_function_from_address(stepout_address) == current_function)
		|| (stepout_address == context_copy.ip))
	{	
		uint32 *newstack = *(uint32**)context_copy.gpr[1];
		stepout_address = *(++newstack);
	}
	memory_insert_breakpoint (stepout_address, &stepout_buffer);
	stepout_installed = TRUE;
}

void stepout_remove()
{
	if(stepout_installed)
		memory_remove_breakpoint(stepout_address, &stepout_buffer);
	stepout_installed = FALSE;
}

/* general defines */
#define PPCIDXMASK      0xfc000000
#define PPCIDX2MASK     0x000007fe
#define PPCDMASK        0x03e00000
#define PPCAMASK        0x001f0000
#define PPCBMASK        0x0000f800
#define PPCCMASK        0x000007c0
#define PPCMMASK        0x0000003e
#define PPCCRDMASK      0x03800000
#define PPCCRAMASK      0x001c0000
#define PPCLMASK        0x00600000
#define PPCOE           0x00000400
#define PPCVRC          0x00000400
#define PPCDST          0x02000000
#define PPCSTRM         0x00600000

#define PPCIDXSH        26
#define PPCDSH          21
#define PPCASH          16
#define PPCBSH          11
#define PPCCSH          6
#define PPCMSH          1
#define PPCCRDSH        23
#define PPCCRASH        18
#define PPCLSH          21
#define PPCIDX2SH       1

#define PPCGETIDX(x)    (((x)&PPCIDXMASK)>>PPCIDXSH)
#define PPCGETD(x)      (((x)&PPCDMASK)>>PPCDSH)
#define PPCGETA(x)      (((x)&PPCAMASK)>>PPCASH)
#define PPCGETB(x)      (((x)&PPCBMASK)>>PPCBSH)
#define PPCGETC(x)      (((x)&PPCCMASK)>>PPCCSH)
#define PPCGETM(x)      (((x)&PPCMMASK)>>PPCMSH)
#define PPCGETCRD(x)    (((x)&PPCCRDMASK)>>PPCCRDSH)
#define PPCGETCRA(x)    (((x)&PPCCRAMASK)>>PPCCRASH)
#define PPCGETL(x)      (((x)&PPCLMASK)>>PPCLSH)
#define PPCGETIDX2(x)   (((x)&PPCIDX2MASK)>>PPCIDX2SH)
#define PPCGETSTRM(x)   (((x)&PPCSTRM)>>PPCDSH)

typedef enum
{
	PPC_BRANCH,
	PPC_BRANCHCOND,
	PPC_BRANCHTOLINK,
	PPC_BRANCHTOLINKCOND,
	PPC_BRANCHTOCTR,
	PPC_BRANCHTOCTRCOND,
	PPC_OTHER
} ppctype;

typedef enum
{
	PPCIN_LINK,
	PPCIN_CTR,
	PPCIN_NONE
} ppcintype;

ppctype PPC_DisassembleBranchInstr(uint32 instr, int32 *reladdr)
{
	unsigned char *p = (unsigned char *)&instr;
	uint32 in = p[0]<<24 | p[1]<<16 | p[2]<<8 | p[3];
	switch (PPCGETIDX(in))
	{
		case 16:
		{
			int d = (int)(in & 0xfffc);

			if (d >= 0x8000)
				d -= 0x10000;
			*reladdr = d;
			return PPC_BRANCHCOND;
			break;
		}
		case 18:
		{
			int d = (int)(in & 0x3fffffc);

			if (d >= 0x2000000)
				d -= 0x4000000;
    		*reladdr = d;
		    return PPC_BRANCH;
			break;
		}
		case 19:
			switch (PPCGETIDX2(in))
			{
				case 16:
				{
					//return branch(dp,in,"lr",0,0);  /* bclr */
					int bo = (int)PPCGETD(in);
					if((bo & 4) && (bo & 16))
						return PPC_BRANCHTOLINK;
					else
						return PPC_BRANCHTOLINKCOND;
				}

				case 528:
				{
					//return branch(dp,in,"ctr",0,0);  /* bcctr */
					int bo = (int)PPCGETD(in);
					if((bo & 4) && (bo & 16))
						return PPC_BRANCHTOCTR;
					else
						return PPC_BRANCHTOCTRCOND;
				}	
				default:
					return PPC_OTHER;
			}
			break;
		default:
			return PPC_OTHER;
	}
}

extern struct MMUIFace *IMMU;

struct Hook tracking_hook;
struct tmsg
{
	char path[1024];
	BPTR seglist;
} tracking_msg;

struct List tracking_list;
int tracking_freemem_hook = -1;

struct tracking_list_entry
{
	struct Node node;
	char *path;
	BOOL isallowed;
};

int32 tracking_hook_func(struct Hook *hook, APTR address, struct FindTrackedAddressMsg *ftam)
{
	struct tmsg *msg = (struct tmsg *)hook->h_Data;
	strcpy(msg->path, ftam->ftam_Name);
	msg->seglist = (BPTR)ftam->ftam_Segment;
	
    dprintf("ftam_Segment = 0x%x, ftam_SegmentNumber = 0x%x, ftam_SegmentOffset = 0x%x\n",
            ftam->ftam_Segment, ftam->ftam_SegmentNumber, ftam->ftam_SegmentOffset);
    dprintf("ftam_Name = %s\n", ftam->ftam_Name ? ftam->ftam_Name : "<unknown>");

	
	return FALSE;
}

static int
strlcmp(const char *s, const char *t, size_t n)
{
	while (n-- && *t != '\0')
		if (*s != *t)
			return ((unsigned char)*s - (unsigned char)*t);
		else
 			++s, ++t;
	return ((unsigned char)*s);
}

void tracking_init()
{
	IExec->NewList(&tracking_list);
	tracking_freemem_hook = freemem_alloc_hook();
}

void tracking_cleanup()
{
	freemem_free_hook(tracking_freemem_hook);
}

void tracking_clear()
{
	freemem_clear(tracking_freemem_hook);
	IExec->NewList(&tracking_list);
}

struct tracking_list_entry *tracking_lookup(char *path)
{
	struct tracking_list_entry *n = (struct tracking_list_entry *)IExec->GetHead(&tracking_list);
	while(n)
	{
		if(strcmp(path, n->path) == 0)
			return n;
		n = (struct tracking_list_entry *)IExec->GetSucc((struct Node *)n);
	}
	return NULL;
}

char bodystring[1024];
BOOL ask_if_allowed(char *name)
{
	Object *reqobj;

	sprintf(bodystring, "You are about to asmstep into the unknown segment \"%s\". This is NOT allowed in the current version!", name);

	reqobj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
            									REQ_Type,       REQTYPE_INFO,
												REQ_TitleText,  "ATTENTION!!!",
									            REQ_BodyText,   bodystring,
									            //REQ_Image,      REQ_IMAGE,
												//REQ_TimeOutSecs, 10,
									            REQ_GadgetText, "OK",
									            TAG_END
        );

	uint32 code = TRUE;
	if ( reqobj )
	{
		IIntuition->IDoMethod( reqobj, RM_OPENREQ, NULL, mainwin, NULL, TAG_END );
		//IIntuition->GetAttr(REQ_ReturnCode, reqobj, &code);
		IIntuition->DisposeObject( reqobj );
	}

	//return !code;
	return FALSE;
}

branch is_branch_allowed()
{
	int32 offset;
	uint32 baddr;
	branch type = NOBRANCH;
	switch(PPC_DisassembleBranchInstr(*(uint32 *)context_copy.ip, &offset))
	{
		case PPC_OTHER:
			return NOBRANCH;
		case PPC_BRANCHTOLINK:
			baddr = context_copy.lr;
			type = NORMALBRANCH;
			break;
		case PPC_BRANCHTOLINKCOND:
			baddr = context_copy.lr;
			type = NORMALBRANCHCOND;
			break;
		case PPC_BRANCHTOCTR:
			baddr = context_copy.ctr;
			type = NORMALBRANCH;
			break;
		case PPC_BRANCHTOCTRCOND:
			baddr = context_copy.ctr;
			type = NORMALBRANCHCOND;
			break;
		case PPC_BRANCHCOND:
			baddr = context_copy.ip + offset;
			type = NORMALBRANCHCOND;
			break;
		case PPC_BRANCH:
			baddr = context_copy.ip + offset;
			type = NORMALBRANCH;
			break;
	}

	tracking_hook.h_Entry = (ULONG (*)())tracking_hook_func;
	tracking_hook.h_Data = (APTR)&tracking_msg;
	IDOS->FindTrackedAddress((CONST_APTR)baddr, &tracking_hook);

	if(tracking_msg.seglist == exec_seglist)
		return type;
		
	struct tracking_list_entry *n = tracking_lookup(tracking_msg.path);
	if(!n)
	{
		n = freemem_malloc(tracking_freemem_hook, sizeof(struct tracking_list_entry));
		n->path = freemem_strdup(tracking_freemem_hook, tracking_msg.path);
		if(ask_if_allowed(n->path))
		{
			//if(n->seglist != 0x0 && strlcmp("Kickstart", tracking_msg.path, 9)) //if not kickstart
			if(stabs_import_external((APTR)baddr))
				n->isallowed = TRUE;
			else
				n->isallowed = FALSE;
		}
		else
			n->isallowed = FALSE;
		IExec->AddTail(&tracking_list, (struct Node *)n);
	}

	if(n->isallowed)
	{
		if(type == NORMALBRANCH)
			type = ALLOWEDBRANCH;
		if(type == NORMALBRANCHCOND)
			type = ALLOWEDBRANCHCOND;
	}
	else
	{
		if(type == NORMALBRANCH)
			type = DISALLOWEDBRANCH;
		if(type == NORMALBRANCHCOND)
			type = DISALLOWEDBRANCHCOND;
	}
	return type;
}

BOOL asm_trace = FALSE;

uint32 asmstep_addr[2] = { 0, 0};
uint32 asmstep_buffer[2] = { 0, 0 };
BOOL asmtrace_should_restore[2] = { FALSE, FALSE };
int no_asm_breaks = 0;

#define    MSR_TRACE_ENABLE           0x00000400

void asmstep_install()
{
	if(has_tracebit)
	{
		struct ExceptionContext ctx;;
		IDebug->ReadTaskContext((struct Task *)process, &ctx, RTCF_STATE);
		//this is not supported on the sam cpu:
		ctx.msr |= MSR_TRACE_ENABLE;
		ctx.ip = context_copy.ip;
		IDebug->WriteTaskContext((struct Task *)process, &ctx, RTCF_STATE);
	}
	else
	{
		int32 offset;

		char dummy1[5], dummy2[40];
		asmstep_addr[0] = (uint32)IDebug->DisassembleNative((APTR)context_copy.ip, dummy1, dummy2);

		switch(PPC_DisassembleBranchInstr(*(uint32 *)context_copy.ip, &offset))
		{
			case PPC_OTHER:
				no_asm_breaks = 1;
				break;
			case PPC_BRANCHTOLINK:
				no_asm_breaks = 2;
				asmstep_addr[1] = context_copy.lr;
				break;
			case PPC_BRANCHTOCTR:
				no_asm_breaks = 2;
				asmstep_addr[1] = context_copy.ctr;
				break;
			case PPC_BRANCH:
				no_asm_breaks = 1;
				asmstep_addr[0] = context_copy.ip + offset;
				break;
			case PPC_BRANCHCOND:
				no_asm_breaks = 2;
				asmstep_addr[1] = context_copy.ip + offset;
				break;
			default:
				break;
		}
		int i;
		for(i = 0; i < no_asm_breaks; i++)
		{
			if (is_breakpoint_at(asmstep_addr[i]))
			{
				asmtrace_should_restore[i] = FALSE;
			}
#if 0
			else if(!is_readable_address(asmstep_addr[i]))
			{
				asmtrace_should_restore[i] = FALSE;
			}
#endif
			else
			{
				asmtrace_should_restore[i] = TRUE;
				memory_insert_breakpoint (asmstep_addr[i], &asmstep_buffer[i]);
			}
		}
	}
	asm_trace = TRUE;
}

void asmstep_remove()
{
	if(has_tracebit)
	{
		struct ExceptionContext ctx;;
		IDebug->ReadTaskContext((struct Task *)process, &ctx, RTCF_STATE);
		//this is not supported on the sam cpu:
		ctx.msr &= ~MSR_TRACE_ENABLE;
		ctx.ip = context_copy.ip;
		IDebug->WriteTaskContext((struct Task *)process, &ctx, RTCF_STATE);
	}
	else
	{
		int i;
		for(i = 0; i < no_asm_breaks; i++)
		{
			if (asmtrace_should_restore[i])
				memory_remove_breakpoint(asmstep_addr[i], &asmstep_buffer[i]);
		}
	}
	asm_trace = FALSE;
}
