#include "stabs.h"

void main_open_window(void);
void main_close_window(void);
void main_event_handler(void);

void event_loop(void);
void cleanup(void);

extern struct stab_function *current_function;
extern BOOL hasfunctioncontext;
