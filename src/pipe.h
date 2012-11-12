#include <proto/dos.h>

void pipe_init();
void pipe_cleanup();
ULONG pipe_obtain_signal();
BPTR pipe_get_write_end();
BPTR pipe_get_read_end();
int pipe_read(char *);
