Elf32_Handle open_elfhandle(Elf32_Handle);
void close_elfhandle (Elf32_Handle);
int relocate_elfhandle(Elf32_Handle);
void close_all_elfhandles();
void lock_elf(Elf32_Handle);
