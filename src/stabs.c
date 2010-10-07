/* stabs.c */

#include <proto/exec.h>
#include <proto/elf.h>

#include "suspend.h"
#include "stabs.h"

#include <stdio.h>
#include <string.h>

uint32 builtin_typetable[] =
{
	T_UNKNOWN,
	T_32,
	T_U8,
	T_32,
	T_U32,
	T_U32,
	T_U32,
	T_BOOL,
	T_16,
	T_U16,
	T_8,
	T_U8,
	T_FLOAT32,
	T_FLOAT64,
	T_FLOAT64,
	T_32,
	T_64,
	T_UNKNOWN, //T_128
	T_VOID
};

int numberofbuiltintypes = 19;

struct List global_symbols;
struct List sourcefile_list;
struct List function_list;

struct stab_function *stabs_get_function_from_address (uint32 faddress)
{
	struct stab_function *f = (struct stab_function *)IExec->GetHead (&function_list);

	while (f)
	{
		if (faddress >= f->address && faddress < f->address + f->size)
			return f;
		f = (struct stab_function *) IExec->GetSucc ((struct Node *)f);
	}
	return NULL;
}

struct stab_function *stabs_get_function_from_name (char *fname)
{
	struct stab_function *f = (struct stab_function *)IExec->GetHead (&function_list);

	while (f)
	{
		if (!strcmp(f->name, fname))
			return f;
		f = (struct stab_function *) IExec->GetSucc ((struct Node *)f);
	}
	return NULL;
}


void stabs_interpret_functions ()
{
	char *stabstr = IElf->GetSectionTags(exec_elfhandle, GST_SectionName, ".stabstr", TAG_DONE);
	uint32 *stab = IElf->GetSectionTags(exec_elfhandle, GST_SectionName, ".stab", TAG_DONE);
	if (!stab)
		return;

	Elf32_Shdr *header = IElf->GetSectionHeaderTags (exec_elfhandle, GST_SectionName, ".stab", TAG_DONE);
	uint32 stabsize = header->sh_size;
	struct symtab_entry *sym = (struct symtab_entry *)stab;
	char *sourcename = NULL;
	struct stab_symbol *symbol;
	BOOL ispointer = FALSE;
	char *strptr;
	struct stab_function *f;

	IExec->NewList (&function_list);

	while ((uint32)sym < (uint32)stab + stabsize - sizeof (struct symtab_entry))
	{
		switch (sym->n_type)
		{
		case N_SO:
			{
			sourcename = &stabstr[sym->n_strx];
			}
			break;

		case N_FUN:

			{
				//we reached the right symbol
				f = IExec->AllocMem (sizeof (struct stab_function), MEMF_ANY|MEMF_CLEAR);

				uint32 line = 0;

				f->name = stabs_strdup_strip(&stabstr[sym->n_strx]);
				f->address = sym->n_value;
				f->sourcename = strdup (sourcename);
				f->currentline = 0;
				f->numberoflines = 0;
				IExec->NewList (&(f->symbols));
				BOOL done = FALSE;

				while (! done )
				{
					sym++;
					if ((uint32)sym > (uint32)stab + stabsize)
						break;

					switch (sym->n_type)
					{
					case N_RBRAC:

						f->size = sym->n_value;
						done = TRUE;
						break;

					case N_SLINE:

						f->lines[f->numberoflines] = sym->n_value;
						f->lineinfile[f->numberoflines] = sym->n_desc;
						if (line > sym->n_desc)
							f->linetype[f->numberoflines] = LINE_LOOP;
						else
							f->linetype[f->numberoflines] = LINE_NORMAL;
						f->numberoflines++;
						line = sym->n_desc;

						break;

					case N_LSYM:

						symbol = IExec->AllocMem (sizeof(struct stab_symbol), MEMF_ANY|MEMF_CLEAR);

						strptr = &stabstr[sym->n_strx];
						symbol->name = stabs_strdup_strip (strptr);
						symbol->location = L_STACK;
						strptr = skip_in_string (strptr, ":");
						symbol->type = stabs_get_type_from_string(strptr, sourcename);
						symbol->offset = sym->n_value;

						IExec->AddTail(&(f->symbols), (struct Node *)symbol);
						break;

					case N_FUN:

						f->size = sym->n_value - f->address;

						sym--;
						done = TRUE;
						break;

					default:
						break;
					}
				}
				IExec->AddTail (&function_list, (struct Node *)f);
			}
			break;

		}
		sym++;
	}
}

char *skip_in_string (char *source, char *pattern)
{
	char *ptr = source;
	int patlen = strlen(pattern);

	while (*ptr != '\0')
	{
		if (*ptr == pattern[0])
		{
			int j = 0;
			while (pattern[j] == *ptr)
			{
				j++; ptr++;
				if (j==patlen)
					return ptr;
			}
		}
		else
			ptr++;
	}
	return NULL;
}

char *skip_backwards_in_string (char *source, char *end, char *pattern)
{
	char *ptr = end;
	int patlen = strlen(pattern);

	while (ptr != source)
	{
		if (*ptr == pattern[patlen-1])
		{
			int j = patlen-1;
			while (pattern[j] == *ptr)
			{
				j--; ptr--;
				if (j==0)
					return ptr;
			}
		}
		else
			ptr--;
	}
	return NULL;
}


char *strdup_until (char *source, char key)
{
	char *ret = IExec->AllocMem (256, MEMF_ANY|MEMF_CLEAR);
	int i;
	while (source[i] != key && source[i] != '\0')
	{
		ret[i] = source[i];
		i++;
	}
	ret[i] = '\0';

	return ret;
}

uint32 stabs_interpret_number_token (char *strptr, uint32 *array)
{
	if ( (*strptr != '(') &&
		!( '0' <= *strptr && *strptr <= '9') )
		strptr++;

	if (*strptr == '(')
	{
		strptr++;
		*array = atoi (strptr);
		strptr = skip_in_string (strptr, ",");
	}
	return atoi (strptr);
}

struct stab_sourcefile *stabs_get_sourcefile (char *sourcename)
{
	struct stab_sourcefile *n = (struct stab_sourcefile *)IExec->GetHead (&sourcefile_list);
	BOOL done = FALSE;

	if (n)
	while (!done)
	{
		if (!strcmp (sourcename, n->filename))
			return n;
		if (n == (struct stab_sourcefile *) IExec->GetTail (&sourcefile_list))
			break;
		n = (struct stab_sourcefile *) IExec->GetSucc ((struct Node *)n);
	}
	return NULL;
}

struct stab_typedef *stabs_get_type_from_token (struct stab_sourcefile *sourcefile, uint32 array, uint32 number)
{
	struct stab_typedef *ret = NULL;
	BOOL done = FALSE;

	if (!sourcefile)
		return NULL;

	struct stab_typedef *t = (struct stab_typedef *)IExec->GetHead(&(sourcefile->typedef_list));

	if (t)
	while (!done)
	{
		if (t->array == array && t->number == number)
			return t;

		if ( t == (struct stab_typedef *)IExec->GetTail (&(sourcefile->typedef_list)) )
			break;
		t = (struct stab_typedef *) IExec->GetSucc ((struct Node *)t);
	}

	return NULL;
}
				
struct stab_typedef *copy_typedef(struct stab_typedef *source)
{
	if (!source)
		return NULL;
	struct stab_typedef *dest = IExec->AllocMem(sizeof (struct stab_typedef), MEMF_ANY|MEMF_CLEAR);
	dest->name = source->name;
	dest->type = source->type;
	dest->array = source->array;
	dest->number = source->number;

	return dest;
}

	
struct stab_typedef * stabs_get_type_from_string (char *strptr, char *sourcename)
{
	if (!sourcename)
		return NULL;
	if (!strptr)
		return NULL;

	BOOL ispointer = FALSE;

	char *ptr = strptr;

	uint32 array = 0;
	uint32 typenum = stabs_interpret_number_token (ptr, &array);

	struct stab_sourcefile *file = stabs_get_sourcefile (sourcename);
	struct stab_typedef *t = stabs_get_type_from_token (file, array, typenum);

	if (!t)
	{
		if (array == 0 && typenum < 19)
		{
			//internal types
			t = IExec->AllocMem (sizeof (struct stab_typedef), MEMF_ANY|MEMF_CLEAR);
			t->name = "INTERNAL";
			t->type = builtin_typetable[typenum];
			t->array = 0;	
			t->number = typenum;
			t->points_to = NULL;
			IExec->AddTail (&(file->typedef_list), (struct Node *)t);
		}
		else
		{
			strptr = skip_in_string (strptr, "=");
			if (!strptr)
				return NULL;
			if (*strptr == '*')
				ispointer = TRUE;
			t = stabs_get_type_from_string(strptr, sourcename);

			if (t)
			{
				struct stab_typedef *newt = IExec->AllocMem (sizeof (struct stab_typedef), MEMF_ANY|MEMF_CLEAR);
				newt->name = NULL;
				newt->array = array;
				newt->number = typenum;
				if (ispointer)
				{
					newt->type = T_POINTER;
					newt->points_to = t;
				}
				else
				{
					newt->type = t->type;
					newt->points_to = t->points_to;
				}
				IExec->AddTail (&(file->typedef_list), (struct Node *)newt);
				t = newt;
			}
		}
	}
	return t;
}




char *stabs_strdup_strip(char *str)
{
	int i, j;
	char *ret = NULL;
	for (i = 0; i<strlen(str); i++)
		if (str[i] == ':' || str[i] == '\0')
			break;

	ret = IExec->AllocMem(i+1, MEMF_ANY);

	for (j = 0; j < i; j++)
		ret[j] = str[j];
	
	ret[j] = '\0';

	return ret;
}

BOOL str_is_typedef (char *str)
{
	char *ptr = str;
	ptr = skip_in_string (ptr, ":");
	return (*ptr == 't' ? TRUE : FALSE);
}


void stabs_interpret_typedefs()
{
	char *stabstr = IElf->GetSectionTags(exec_elfhandle, GST_SectionName, ".stabstr", TAG_DONE);
	uint32 *stab = IElf->GetSectionTags(exec_elfhandle, GST_SectionName, ".stab", TAG_DONE);
	if (!stab)
		return;

	IExec->NewList (&sourcefile_list);

	Elf32_Shdr *header = IElf->GetSectionHeaderTags (exec_elfhandle, GST_SectionName, ".stab", TAG_DONE);
	uint32 stabsize = header->sh_size;
	struct symtab_entry *sym = (struct symtab_entry *)stab;
	BOOL done = FALSE;
	char *strptr;
	struct stab_sourcefile *sourcefile = NULL;

	while ((uint32)sym < (uint32)stab + stabsize - sizeof (struct symtab_entry) && !done)
	{
		switch (sym->n_type)
		{
		case N_SO:

			if (strlen (&stabstr[sym->n_strx]))
			{
				sourcefile = IExec->AllocMem (sizeof (struct stab_sourcefile), MEMF_ANY|MEMF_CLEAR);

				sourcefile->filename = strdup(&stabstr[sym->n_strx]);
				IExec->NewList (&(sourcefile->typedef_list));
				IExec->NewList (&(sourcefile->function_list));
				IExec->AddTail (&sourcefile_list, (struct Node *)sourcefile);
			}
			break;

		case N_LSYM:

			if (!sourcefile || !sourcefile->filename)
				break;

			strptr = &stabstr[sym->n_strx];

			if (str_is_typedef(strptr))
			{
				char *name = stabs_strdup_strip (strptr);

				strptr = skip_in_string (strptr, ":");
				if (strptr)
				{
					stabs_get_type_from_string(strptr, sourcefile->filename);
				}
			}
			break;
		}
		sym++;
	}
}

void stabs_interpret_globals()
{
	IExec->NewList(&global_symbols);

	char *stabstr = IElf->GetSectionTags(exec_elfhandle, GST_SectionName, ".stabstr", TAG_DONE);
	uint32 *stab = IElf->GetSectionTags(exec_elfhandle, GST_SectionName, ".stab", TAG_DONE);
	if (!stab)
		return;

	Elf32_Shdr *header = IElf->GetSectionHeaderTags (exec_elfhandle, GST_SectionName, ".stab", TAG_DONE);
	uint32 stabsize = header->sh_size;
	struct symtab_entry *sym = (struct symtab_entry *)stab;
	BOOL done = FALSE;
	char *strptr;
	struct List *l = &global_symbols;
	struct stab_symbol *symbol;
	BOOL ispointer = FALSE;
	char *sourcefile = NULL;

	while ((uint32)sym < (uint32)stab + stabsize - sizeof (struct symtab_entry) && !done)
	{
		switch (sym->n_type)
		{
		case N_SO:

			sourcefile = &stabstr[sym->n_strx];

			break;

		case N_GSYM:
			symbol = IExec->AllocMem (sizeof(struct stab_symbol), MEMF_ANY|MEMF_CLEAR);

			strptr = &stabstr[sym->n_strx];
			symbol->name = stabs_strdup_strip (strptr);
			symbol->location = L_ABSOLUTE;
			strptr = skip_in_string (strptr, ":");
			symbol->type = stabs_get_type_from_string(strptr, sourcefile);
			symbol->address = get_symval_from_name(symbol->name);

			IExec->AddTail(l, (struct Node *)symbol);

			break;
		}
		sym++;
	}
}

