/* freemem.c */

#include <proto/exec.h>
#include <string.h>
#include <stdio.h>

#include "freemem.h"

void *freemem_hooks[1024];

void freemem_init()
{
	int i;
	for(i = 0; i < 1024; i++)
		freemem_hooks[i] = NULL;
}

int freemem_alloc_hook()
{
	int i;
	for(i = 0; i < 1024; i++)
	{
		if(freemem_hooks[i] == NULL)
		{
			freemem_hooks[i] = IExec->AllocVecTags(sizeof(struct List), TAG_DONE);
			IExec->NewList(freemem_hooks[i]);
			return i;
		}
	}
	return -1;
}

void freemem_free_hook(int hook)
{
	if(hook >= 0 && freemem_hooks[hook])
	{
		freelist(freemem_hooks[hook]);
		IExec->FreeVec(freemem_hooks[hook]);
		freemem_hooks[hook] = NULL;
	}
}

void *freemem_malloc(int hook, int size)
{
	if(hook >= 0 && freemem_hooks[hook])
	{
		void *a = IExec->AllocVecTags(size, AVT_ClearWithValue, 0, TAG_DONE);
		if(a)
		{
			add_freelist(freemem_hooks[hook], a);
			return a;
		}
	}
	return NULL;
}

char *freemem_strdup(int hook, char *str)
{
	int len = strlen(str);
	char *ret = freemem_malloc(hook, len+1);
	if(ret)
	{
		memcpy(ret, str, len+1);
		return ret;
	}
	return NULL;
}

void freemem_clear(int hook)
{
	if(hook >= 0 && freemem_hooks[hook])
		freelist(freemem_hooks[hook]);
}

void add_freelist (struct List *l, void *address)
{
	struct freenode *n = IExec->AllocVecTags (sizeof (struct freenode), TAG_DONE);
	n->address = address;
	IExec->AddTail (l, (struct Node *)n);
}

void freelist (struct List *l)
{
	struct freenode *n = (struct freenode *)IExec->GetHead(l);
	while (n)
	{
		struct freenode *next = (struct freenode *)IExec->GetSucc((struct Node *)n);
		IExec->Remove ((struct Node *) n);

		IExec->FreeVec (n->address);
		IExec->FreeVec (n);

		n = next;
	}
}
