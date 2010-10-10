/* freemem.h */

#include <exec/nodes.h>

void add_freelist(struct List *, uint32, void *);
void freelist (struct List *);

struct freenode
{
	struct Node node;
	uint32 size;
	void *address;
};
