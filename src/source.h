/* source.h */

#include "stabs.h"

void source_init();
void source_cleanup();
void source_clear();
void source_load_file(struct stab_sourcefile *, char *);
void source_handle_input();
void source_show_currentline();
