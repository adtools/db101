/*variables.h*/

void locals_update_window(void);
void locals_open_window(void);
void locals_close_window(void);
uint32 locals_obtain_window_signal(void);
void locals_event_handler(void);
char *print_variable_value(struct stab_symbol *);

BOOL is_readable_address(uint32);
