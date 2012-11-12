/*variables.h*/
#include "stabs.h"

char *print_variable_value(struct stab_symbol *);

void locals_init();
void locals_cleanup();
void locals_clear(void);
void locals_update(void);
void locals_handle_input();

BOOL is_readable_address(uint32);

struct variables_userdata
{
	struct stab_symbol *s;
	int haschildren;
	int isopen;
};
