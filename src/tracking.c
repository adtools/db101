#include <exec/types.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/elf.h>
#include <proto/intuition.h>
#include <proto/requester.h>

#include <classes/requester.h>

#include <stdio.h>
#include <string.h>

#include "tracking.h"
#include "console.h"
#include "preferences.h"
#include "suspend.h"
#include "freemem.h"
#include "stabs.h"
#include "elf.h"
#include "modules.h"

#define dprintf(format, args...) ((struct ExecIFace *)((*(struct ExecBase **)4)->MainInterface))->DebugPrintF("[%s] " format, __PRETTY_FUNCTION__ , ## args)

extern struct Window *mainwin;

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

## REMOVED BY THE FBI ##
########################
########################
########################
########################
########################
########################
########################
########################

struct Hook tracking_hook;
struct tmsg
{
	char path[1024];
	BPTR seglist;
	BOOL issolib;
	Elf32_SOLibNode solibNode;
} tracking_msg;

struct List tracking_list;
int tracking_freemem_hook = -1;




int32 tracking_hook_func(struct Hook *hook, APTR address, struct FindTrackedAddressMsg *ftam)
{
	struct tmsg *msg = (struct tmsg *)hook->h_Data;
	strcpy(msg->path, ftam->ftam_Name);
	msg->seglist = (BPTR)ftam->ftam_Segment;
	
    dprintf("ftam_Segment = 0x%x, ftam_SegmentNumber = 0x%x, ftam_SegmentOffset = 0x%x\n",
            ftam->ftam_Segment, ftam->ftam_SegmentNumber, ftam->ftam_SegmentOffset);
    dprintf("ftam_Name = %s\n", ftam->ftam_Name ? ftam->ftam_Name : "<unknown>");

	msg->issolib = FALSE;
    if (ftam->ftam_ExtraInfo
           && ftam->ftam_ExtraInfoSize == sizeof(Elf32_SOLibNode))
    {
		msg->issolib = TRUE;
        IExec->CopyMem(ftam->ftam_ExtraInfo, &msg->solibNode,
             sizeof(Elf32_SOLibNode));
    }
	
	return FALSE;
}

int
strlcmp(const char *s, const char *t, size_t n)
{
	while (n-- && *t != '\0')
		if (*s != *t)
			return ((unsigned char)*s - (unsigned char)*t);
		else
 			++s, ++t;
	return ((unsigned char)*s);
}

#if 0
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
#endif

char bodystring[1024];
BOOL ask_if_allowed(char *name)
{
	Object *reqobj;

	sprintf(bodystring, "Do you want to import the module \"%s\"?", name);

	reqobj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
            									REQ_Type,       REQTYPE_INFO,
												REQ_TitleText,  "ATTENTION!!!",
									            REQ_BodyText,   bodystring,
									            //REQ_Image,      REQ_IMAGE,
												//REQ_TimeOutSecs, 10,
									            REQ_GadgetText, "OK|Cancel",
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

BOOL try_import_segment(uint32 address)
{
	tracking_msg.seglist = 0x0;
	strcpy(tracking_msg.path, "");
	tracking_hook.h_Entry = (ULONG (*)())tracking_hook_func;
	tracking_hook.h_Data = (APTR)&tracking_msg;
	IDOS->FindTrackedAddress((CONST_APTR)address, &tracking_hook);

	if(tracking_msg.seglist == exec_seglist)
	{
		//console_printf(OUTPUT_SYSTEM, "Address is in the main exe");
		return -2;
	}
	if(tracking_msg.seglist == 0x0 && !tracking_msg.issolib)
	{
		return -3;
	}
	
	BOOL isallowed = FALSE;
	//struct tracking_list_entry *n = tracking_lookup(tracking_msg.path);
	struct modules_entry *e = modules_lookup_entry(tracking_msg.path);
	if(!e)
	{
		//n = freemem_malloc(tracking_freemem_hook, sizeof(struct tracking_list_entry));
		char *path = tracking_msg.path;
		if(!strlcmp(path, "Kickstart", 9) && !db101_prefs.prefs_allow_kernel_access)
		{
			console_printf(OUTPUT_WARNING, "Importing kernel modules is NOT allowed!");
			isallowed = FALSE;
		}
		else if(db101_prefs.prefs_import_externals == EXT_ALWAYS
				|| (db101_prefs.prefs_import_externals == EXT_ASK && ask_if_allowed(path)))
		{
			Elf32_Handle handle;
			if(tracking_msg.issolib)
				handle = tracking_msg.solibNode.SOLibHandle;
			else
				IDOS->GetSegListInfoTags(tracking_msg.seglist, GSLI_ElfHandle, &handle, TAG_DONE);
			if(handle && stabs_load_module(handle, path))
				;
			isallowed = TRUE;
		}
		else
			isallowed = FALSE;
		modules_add_entry(path, "", isallowed);
		//IExec->AddTail(&tracking_list, (struct Node *)n);
	}
	else
		isallowed = e->accessallowed;
	return isallowed;
}

struct stab_function *get_branched_function()
{
	int32 offset;
	uint32 baddr;
	switch(PPC_DisassembleBranchInstr(*(uint32 *)context_copy.ip, &offset))
	{
		case PPC_OTHER:
			return NULL;
		case PPC_BRANCHTOLINK:
		case PPC_BRANCHTOLINKCOND:
			return stabs_get_function_from_address(context_copy.lr);
		case PPC_BRANCHTOCTR:
		case PPC_BRANCHTOCTRCOND:
			return stabs_get_function_from_address(context_copy.ctr);
		case PPC_BRANCH:
		case PPC_BRANCHCOND:
			return stabs_get_function_from_address(context_copy.ip + offset);
	}
	return NULL;
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
	tracking_msg.seglist = 0x0;
	strcpy(tracking_msg.path, "");
	tracking_hook.h_Entry = (ULONG (*)())tracking_hook_func;
	tracking_hook.h_Data = (APTR)&tracking_msg;
	IDOS->FindTrackedAddress((CONST_APTR)baddr, &tracking_hook);

	if(tracking_msg.seglist == exec_seglist)
		return type;
	if(tracking_msg.seglist == 0x0 && !tracking_msg.issolib)
		return DISALLOWEDBRANCH;
	
	BOOL isallowed = FALSE;
	struct modules_entry *e = modules_lookup_entry(tracking_msg.path);
	if(!e)
	{
		//n = freemem_malloc(tracking_freemem_hook, sizeof(struct tracking_list_entry));
		char *path = tracking_msg.path;
		if(!strlcmp(path, "Kickstart", 9) && !db101_prefs.prefs_allow_kernel_access)
		{
			console_printf(OUTPUT_WARNING, "Stepping into kernel NOT allowed!");
			isallowed = FALSE;
		}
		else if(db101_prefs.prefs_import_externals == EXT_ALWAYS
				|| (db101_prefs.prefs_import_externals == EXT_ASK && ask_if_allowed(path)))
		{
			Elf32_Handle handle;
			if(tracking_msg.issolib)
				handle = tracking_msg.solibNode.SOLibHandle;
			else
				IDOS->GetSegListInfoTags(tracking_msg.seglist, GSLI_ElfHandle, &handle, TAG_DONE);
			if(handle && stabs_load_module(handle, path))
				; //nothing to do...
			isallowed = TRUE;
		}
		else
			isallowed = FALSE;
		modules_add_entry(path, "", isallowed);
		//IExec->AddTail(&tracking_list, (struct Node *)n);
	}
	else
	{
		isallowed = e->accessallowed;
		if(isallowed && !e->hasbeenloaded)
		{
			Elf32_Handle handle;
			if(tracking_msg.issolib)
				handle = tracking_msg.solibNode.SOLibHandle;
			else
				IDOS->GetSegListInfoTags(tracking_msg.seglist, GSLI_ElfHandle, &handle, TAG_DONE);
			if(handle && stabs_load_module(handle, e->name))
				e->hasbeenloaded = TRUE;
		}
	}
	if(isallowed)
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
