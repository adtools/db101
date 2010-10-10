/* symbols.h */

#include <libraries/elf.h>

void get_symbols (void);
void free_symbols (void);
void list_symbols (void);
uint32 get_symval_from_name (char *);


Elf32_Handle open_elfhandle(void);
void close_elfhandle (Elf32_Handle);

extern struct List symbols_list;
//uint32 symval;

struct amigaos_symbol
{
	struct Node node;
	char *name;
	uint32 value;
};
