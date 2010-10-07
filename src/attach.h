/* symbols.h */

#include <dos/dosextens.h>

void get_process_list (void);
void free_process_list (void);

struct Process * attach_select_process(void);
