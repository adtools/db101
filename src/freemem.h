/* freemem.h */

#include <exec/nodes.h>

void freemem_init();
int freemem_alloc_hook();
void freemem_free_hook();
void *freemem_malloc(int, int);
char *freemem_strdup(int, char *);
void freemem_clear();

void add_freelist(struct List *, void *);
void freelist (struct List *);

struct freenode
{
	struct Node node;
	void *address;
};
