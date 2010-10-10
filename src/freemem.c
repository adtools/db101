/* freemem.c */

#include <proto/exec.h>

#include "freemem.h"

void add_freelist (struct List *l, uint32 size, void *address)
{
	struct freenode *n = IExec->AllocMem (sizeof (struct freenode), MEMF_ANY|MEMF_CLEAR);

	n->address = address;
	n->size = size;

	IExec->AddTail (l, (struct Node *)n);
}

void freelist (struct List *l)
{
	struct freenode *n = (struct freenode *)IExec->GetHead(l);
	while (n)
	{
		struct freenode *next = (struct freenode *)IExec->GetSucc((struct Node *)n);
		IExec->Remove ((struct Node *) n);

		IExec->FreeMem (n->address, n->size);
		IExec->FreeMem (n, sizeof (struct freenode));

		n = next;
	}
}
