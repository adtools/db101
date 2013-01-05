#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/elf.h>

#include "stabs.h"
#include "console.h"

extern struct List modules_list;

Elf32_Handle open_elfhandle(Elf32_Handle elfhandle)
{
	return IElf->OpenElfTags(OET_ElfHandle, elfhandle, TAG_DONE);
}

void close_elfhandle (Elf32_Handle handle)
{
	IElf->CloseElfTags (handle, CET_ReClose, TRUE, TAG_DONE);
}

int relocate_elfhandle (Elf32_Handle elfhandle)
{
    IElf->RelocateSectionTags( elfhandle, GST_SectionName, ".stabstr", GST_Load, TRUE, TAG_DONE );
    return IElf->RelocateSectionTags( elfhandle, GST_SectionName, ".stab", GST_Load, TRUE, TAG_DONE );
}

void close_all_elfhandles()
{
	struct stab_module *m = (struct stab_module *)IExec->GetHead(&modules_list);
	while(m)
	{
		if(m->elfhasbeenopened)
		{
			printf("Closing elfhandle...\n");
			close_elfhandle(m->elfhandle);
		}
		m = (struct stab_module *)IExec->GetSucc((struct Node *)m);
	}
}

void lock_elf(Elf32_Handle handle)
{
	int numsections;
	int index;
	char *strtable;
	IElf->GetElfAttrsTags (handle,  EAT_NumSections, &numsections, TAG_DONE);

	int i;
	for (i = 0; i < numsections; i++)
	{
		uint32 *section = IElf->GetSectionTags(handle, GST_SectionIndex, i, TAG_DONE);
		Elf32_Shdr *header = IElf->GetSectionHeaderTags (handle, GST_SectionIndex, i, TAG_DONE);
		if (!section)
			continue;
		int size = header->sh_size;
		console_printf(OUTPUT_SYSTEM, "Locking memory: 0x%x %d", section, size);
		if(!IExec->LockMem(section, size))
			console_printf(OUTPUT_WARNING, "Failed to lock memory!");
	}
}
