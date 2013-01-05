/* symbols.h */

#include <dos/dos.h>
#include <libraries/elf.h>

void init_symbols(void);
void get_symbols (Elf32_Handle);
void free_symbols (void);
void list_symbols (void);
uint32 get_symval_from_name (char *);
char *get_symbol_from_value(uint32);

void symbols_dummy(Elf32_Handle);

extern struct List symbols_list;

struct amigaos_symbol
{
	struct Node node;
	char *name;
	uint32 value;
};
