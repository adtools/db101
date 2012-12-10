/*variables.h*/
#include "stabs.h"

char *print_variable_value(struct stab_symbol *);

void variables_init();
void variables_cleanup();
void variables_clear(void);
void variables_update(void);
void variables_handle_input();

BOOL is_readable_address(uint32);

struct variables_userdata
{
	struct stab_symbol *s;
	int haschildren;
	int isopen;
};
