/* symbols.h */

#include <libraries/elf.h>

void get_symbols (void);
void free_symbols (void);
void list_symbols (void);
uint32 get_symval_from_name (char *);


struct stab_function * select_symbol(void);

Elf32_Handle open_elfhandle(void);
void close_elfhandle (Elf32_Handle);

char *symlist[1024];
uint32 symval;
