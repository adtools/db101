/* stabs.c */

#include <proto/exec.h>
#include <proto/elf.h>

#include "suspend.h"
#include "stabs.h"
#include "freemem.h"
#include "console.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

//debug stuff
#include <proto/exec.h>
#define dprintf(format...)	IExec->DebugPrintF(format)


#define MAX_NESTED_INCLUDES 256

struct List global_symbols;
struct List sourcefile_list;
//struct List function_list;

int stabs_freemem_hook = -1;


struct stab_function *stabs_sline_to_nline(char *sourcename, uint32 sline, uint32 *nline)
{
	struct stab_sourcefile *s = stabs_get_sourcefile (sourcename);
	if(s)
	{
		struct stab_function *f = (struct stab_function *)IExec->GetHead(&s->function_list);
		int i;

		while(f)
		{
			for( i = 0; i < f->numberoflines; i++)
			{
				if( f->line[i].infile == sline )
				{
					*nline = i;
					return f;
				}
			}
			f = (struct stab_function *) IExec->GetSucc((struct Node *)f);
		}
	}
	return NULL;
}

struct stab_function *stabs_get_function_from_address (uint32 faddress)
{
	struct stab_sourcefile *s = (struct stab_sourcefile *)IExec->GetHead(&sourcefile_list);
	while(s)
	{
		struct stab_function *f = (struct stab_function *)IExec->GetHead (&s->function_list);

		while (f)
		{
			if (faddress >= f->address && faddress < f->address + f->size)
				return f;
			f = (struct stab_function *) IExec->GetSucc ((struct Node *)f);
		}
		s = (struct stab_sourcefile *) IExec->GetSucc((struct Node *)s);
	}
	return NULL;
}

struct stab_function *stabs_get_function_from_name (char *fname)
{
	struct stab_sourcefile *s = (struct stab_sourcefile *)IExec->GetHead(&sourcefile_list);
	while(s)
	{
		struct stab_function *f = (struct stab_function *)IExec->GetHead (&s->function_list);

		while (f)
		{
			if (!strcmp(f->name, fname))
				return f;
			f = (struct stab_function *) IExec->GetSucc ((struct Node *)f);
		}
		s = (struct stab_sourcefile *) IExec->GetSucc((struct Node *)s);
	}
	return NULL;
}


void stabs_interpret_functions ()
{
	char *stabstr = IElf->GetSectionTags(exec_elfhandle, GST_SectionName, ".stabstr", TAG_DONE);
	uint32 *stab = IElf->GetSectionTags(exec_elfhandle, GST_SectionName, ".stab", TAG_DONE);
	if (!stab)
	{
		console_printf(OUTPUT_WARNING, "failed to find .stab section...");
		return;
	}

	Elf32_Shdr *header = IElf->GetSectionHeaderTags (exec_elfhandle, GST_SectionName, ".stab", TAG_DONE);
	uint32 stabsize = header->sh_size;
	struct symtab_entry *sym = (struct symtab_entry *)stab;
	char *sourcename = NULL;
	char *solname = NULL;
	char *includename[MAX_NESTED_INCLUDES];
	int includelevel = FALSE;
	int soloverride = FALSE;
	struct stab_symbol *symbol;
	BOOL ispointer = FALSE;
	char *strptr;
	struct stab_function *f;

	while ((uint32)sym < (uint32)stab + stabsize - sizeof (struct symtab_entry))
	{
		switch (sym->n_type)
		{
		case N_SO:
			{
			sourcename = &stabstr[sym->n_strx];
			if(includelevel > 0)
				printf("Source file malfunction...\n");
			soloverride = FALSE;
			}
			break;
			
		case N_SOL:
			{
			solname = &stabstr[sym->n_strx];
			soloverride = TRUE;
			}
			break;
						
		case N_BINCL:
			{
			if(includelevel >= MAX_NESTED_INCLUDES)
				printf("Fatal error: MAX_NESTED_INCLUDES overridden!!!\n");
			includename[includelevel] = &stabstr[sym->n_strx];
			includelevel++;
			soloverride = FALSE;
			}
			break;
		
		case N_EINCL:
			{
			if(includelevel <= 0)
				printf("Fatal error: negative include level!!!\n");
			includelevel--;
			}
			break;
			
		case N_FUN:
			{
				f = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_function));

				uint32 line = 0;
				int brac = 0;
				int bracstart = 0;
				int size = 0;
				
				f->name = stabs_strdup_strip(&stabstr[sym->n_strx]);
				f->address = sym->n_value;
				if(includelevel && !soloverride)
					f->sourcename = freemem_strdup(stabs_freemem_hook, includename[includelevel-1]);
				else if (soloverride)
					f->sourcename = freemem_strdup(stabs_freemem_hook, solname);
				else
					f->sourcename = freemem_strdup(stabs_freemem_hook, sourcename);

				f->currentline = 0;
				f->numberoflines = 0;
				IExec->NewList (&(f->symbols));
				IExec->NewList(&f->params);
				BOOL done = FALSE;
				f->line = 0;

				while (! done )
				{
					sym++;
					if ((uint32)sym > (uint32)stab + stabsize)
						break;

					switch (sym->n_type)
					{
					case N_LBRAC:
						brac++;
						if(brac==1)
							bracstart = sym->n_value;
						break;
						
					case N_RBRAC:
						brac--;
						if(brac == 0)
						{
							f->size = sym->n_value; //should this be n_value-bracstart??
							done = TRUE;
						}
						if(brac<0)
							printf("Closing bracket without opening bracket!!!\n");

						break;

					case N_SLINE:
						f->line = (struct sline *)realloc(f->line, sizeof(struct sline) * (f->numberoflines + 1));
						if( !f->line )
						{
							printf("Out of memory! (N_SLINE %ld)\n", f->numberoflines);
							done = TRUE;
							break;
						}
						f->line[f->numberoflines].adr = sym->n_value;
						f->line[f->numberoflines].infile = sym->n_desc;
						if (line > sym->n_desc)
							f->line[f->numberoflines].type = LINE_LOOP;
						else
						{
							size = sym->n_value+4;
							f->line[f->numberoflines].type = LINE_NORMAL;
						}
						f->numberoflines++;
						line = sym->n_desc;

						break;

					case N_LSYM:
					
						symbol = freemem_malloc(stabs_freemem_hook, sizeof(struct stab_symbol));

						strptr = &stabstr[sym->n_strx];
						symbol->name = stabs_strdup_strip (strptr);
						symbol->location = L_STACK;
						strptr = skip_in_string (strptr, ":");
						symbol->type = stabs_get_type_from_string(strptr, sourcename);
						symbol->offset = sym->n_value;

						IExec->AddTail(&f->symbols, (struct Node *)symbol);

						break;

					case N_PSYM:
					
						symbol = freemem_malloc(stabs_freemem_hook, sizeof(struct stab_symbol));

						strptr = &stabstr[sym->n_strx];
						symbol->name = stabs_strdup_strip (strptr);
						symbol->location = L_STACK;
						strptr = skip_in_string (strptr, ":");
						symbol->type = stabs_get_type_from_string(strptr, sourcename);
						symbol->offset = sym->n_value;

						IExec->AddTail(&f->params, (struct Node *)symbol);

						break;

					case N_SOL:

						solname = f->sourcename = freemem_strdup(stabs_freemem_hook, &stabstr[sym->n_strx]);
						soloverride = TRUE;
						break;
						
					case N_FUN:

						f->size = size;

						sym--;
						done = TRUE;
						break;
					
					case N_SO:
					
						f->size = size;
						sym--;
						done = TRUE;

						break;

					default:
						break;
					}
				}
				struct stab_sourcefile *s = stabs_get_sourcefile(f->sourcename);
				if(s)
					IExec->AddTail(&s->function_list, (struct Node *)f);
			}
			break;
			
			default:
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
	return ptr; //return a null string
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
	char *ret = freemem_malloc(stabs_freemem_hook, 256);
	int i;
	while (source[i] != key && source[i] != '\0')
	{
		ret[i] = source[i];
		i++;
	}
	ret[i] = '\0';

	return ret;
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

int stabs_interpret_number_token(char *strptr, int *filenum)
{
	if(!strptr)	return -1;

	if ( (*strptr != '(') &&
		!( '0' <= *strptr && *strptr <= '9') )
		strptr++;

	*filenum = 0;
	if (*strptr == '(')
	{
		strptr++;
		*filenum = atoi (strptr);
		strptr = skip_in_string (strptr, ",");
	}
	return atoi (strptr);
}

struct stab_typedef *stabs_get_type_from_typenum (struct List *l, int filenum, int typenum)
{
	struct stab_typedef *ret = NULL;
	BOOL done = FALSE;

	if (!l)
		return NULL;

	struct stab_typedef *t = (struct stab_typedef *)IExec->GetHead(l);
	
	while (t)
	{
		if (t->filenum == filenum && t->typenum == typenum)
			return t;

		t = (struct stab_typedef *) IExec->GetSucc ((struct Node *)t);
	}
	return NULL;
}

enum __stab_symbol_types stabs_interpret_range(char *strptr)
{
	if(*strptr != 'r')
		return T_UNKNOWN;
	strptr = skip_in_string(strptr, ";");
	if(!strptr)
		return T_UNKNOWN;
	long long lower_bound = atoi(strptr);
	strptr = skip_in_string(strptr, ";");
	long long upper_bound = atoi(strptr);
	enum __stab_symbol_types ret = T_UNKNOWN;
	if(lower_bound == 0 && upper_bound == -1)
		;
	else if(lower_bound > 0 && upper_bound == 0)
	{
		switch(lower_bound)
		{
			case 4:
				ret = T_FLOAT32;
				break;
			case 8:
				ret = T_FLOAT64;
				break;
			case 16:
				ret = T_FLOAT128;
				break;
			default:
				ret = T_UNKNOWN;
				break;
		}
	}
	else
	{
		long long range = upper_bound - lower_bound;
		if(lower_bound < 0)
		{
			if(range <= 0xff)
				ret = T_8;
			else if(range <= 0xffff)
				ret = T_16;
			else if(range <= 0xffffffff)
				ret = T_32;
			//else if(range <= 0xffffffffffffffff)
			//	ret = T_64;
			//else if(range <= 0xffffffffffffffffffffffffffffffff)
			//	ret = T_128;
			else
				ret = T_UNKNOWN;
		}
		else // if unsigned
		{
			if(range <= 0xff)
				ret = T_U8;
			else if(range <= 0xffff)
				ret = T_U16;
			else if(range <= 0xffffffff)
				ret = T_U32;
			//else if(range <= 0xffffffffffffffff)
			//	ret = T_U64;
			//else if(range <= 0xffffffffffffffffffffffffffffffff)
			//	ret = T_U128;
			else
				ret = T_UNKNOWN;
		}
	}
	return ret;
}

char *stabs_skip_numbers(char *strptr)
{
	char *ret = strptr;
	while(*ret >= '0' && *ret <= '9')
		ret++;
	return ret;
}

struct stab_structure * stabs_interpret_struct_or_union(char *strptr, char *sourcename)
{
	struct stab_sourcefile *file = stabs_get_sourcefile(sourcename);
	struct stab_structure *s = freemem_malloc(stabs_freemem_hook, sizeof(struct stab_structure));
	IExec->NewList(&(s->list));
	s->size = atoi(strptr);
	strptr = stabs_skip_numbers(strptr);
	while(*strptr != ';')
	{
		struct stab_struct_element *e = freemem_malloc(stabs_freemem_hook, sizeof(struct stab_struct_element));
		e->name = stabs_strdup_strip(strptr);
		strptr = skip_in_string(strptr, ":");
		e->type = stabs_get_type_from_string(strptr, sourcename);
		if(!e->type)
		{
			//IExec->FreeVec(e);
			return NULL;
		}
		if(e->type->type == T_CONFORMANT_ARRAY || (e->type->type == T_POINTER && e->type->points_to->type == T_CONFORMANT_ARRAY))
		{
			strptr = skip_in_string(strptr, "=x");	//skip over 'unknown size' marker
			strptr++;								//skip over 's' or 'u' or 'a'
			strptr = skip_in_string(strptr, ":");	//skip to the numerals
			if(*strptr != ',')
			{
				e->bitoffset = -1;
				e->bitsize = -1;
				IExec->AddTail(&(s->list), (struct Node *)e);
				return s;
			}
		}
		else
			strptr = skip_in_string(strptr, "),");
		e->bitoffset = atoi(strptr);
		strptr = skip_in_string(strptr, ",");
		e->bitsize = atoi(strptr);
		strptr = skip_in_string(strptr, ";");
		IExec->AddTail (&(s->list), (struct Node *)e);
	}
	return s;
}

struct stab_enum * stabs_interpret_enum(char *strptr, char *sourcename)
{
	struct stab_sourcefile *file = stabs_get_sourcefile(sourcename);
	struct stab_enum *s = freemem_malloc(stabs_freemem_hook, sizeof(struct stab_enum));
	IExec->NewList(&(s->list));
	while(*strptr != ';' && *strptr != '\0')
	{
		struct stab_enum_element *e = freemem_malloc(stabs_freemem_hook, sizeof(struct stab_enum_element));
		e->name = stabs_strdup_strip(strptr);
		strptr = skip_in_string(strptr, ":");
		e->value = atoi(strptr);
		strptr = skip_in_string(strptr, ",");
		IExec->AddTail(&(s->list), (struct Node *)e);
	}
	return s;
}

struct stab_typedef * stabs_get_type_from_string (char *strptr, char *sourcename)
{
	if (!sourcename)
		return NULL;
	if (!strptr)
		return NULL;

	BOOL ispointer = FALSE;
	BOOL isarray = FALSE;
	int array_lowerbound = 0;
	int array_upperbound = 0;
	int filenum = 0, typenum = 0;
	struct stab_typedef *t = NULL;
	
	struct stab_sourcefile *file = stabs_get_sourcefile (sourcename);
	
	typenum = stabs_interpret_number_token(strptr, &filenum);
	t = stabs_get_type_from_typenum (&file->typedef_list, filenum, typenum);

	if(!t)
	{
		strptr = skip_in_string (strptr, "=");
		if (!strptr || *strptr == '\0')
			return NULL;
		if (*strptr == '*')
		{
			ispointer = TRUE;
			strptr++;
		}
		if (*strptr == 'a')
		{
			isarray = TRUE;
			strptr = skip_in_string(strptr, ")");
			if(*strptr == '=' && strptr++ && *strptr == 'r')
			{
				//skip additional range info
				int i;
				for (i = 0; i < 3; i++)
					strptr = skip_in_string(strptr, ";");
			}
			else if(*strptr == ';')
			{
				strptr++;
				array_lowerbound = atoi(strptr);
				strptr = skip_in_string(strptr, ";");
				array_upperbound = atoi(strptr);
				strptr = skip_in_string(strptr, ";");
				int size = array_upperbound - array_lowerbound;
				t = stabs_get_type_from_string(strptr, sourcename);
				struct stab_typedef *newt = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_typedef));
				newt->name = NULL;
				newt->filenum = filenum;
				newt->typenum = typenum;
				newt->type = T_ARRAY;
				newt->arraysize = size;
				newt->points_to = t;
				IExec->AddTail (&(file->typedef_list), (struct Node *)newt);
				t = newt;
			}
		}
		else if(*strptr == 'r')
		{
			enum __stab_symbol_types type = stabs_interpret_range(strptr);
			t = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_typedef));
			t->name = NULL;
			t->filenum = filenum;
			t->typenum = typenum;
			t->type = type;
			t->points_to = NULL;
			IExec->AddTail (&(file->typedef_list), (struct Node *)t);
		}
		else if(*strptr == 's')
		{
			// structure
			t = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_typedef));
			t->filenum = filenum;
			t->typenum = typenum;
			t->type = T_STRUCT;
			t->points_to = NULL;
			IExec->AddTail (&(file->typedef_list), (struct Node *)t); // has to do this before to be able to have pointers to instances of self in the struct
			struct stab_structure *s = stabs_interpret_struct_or_union(++strptr, sourcename);
			t->struct_ptr = s;
		}
		else if (*strptr == 'u')
		{
			// union
			struct stab_structure *s = stabs_interpret_struct_or_union(++strptr, sourcename);
			t = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_typedef));
			t->filenum = filenum;
			t->typenum = typenum;
			t->type = T_UNION;
			t->struct_ptr = s;
			t->points_to = NULL;
			IExec->AddTail (&(file->typedef_list), (struct Node *)t);
		}
		else if (*strptr == 'e')
		{
			// union
			struct stab_enum *s = stabs_interpret_enum(++strptr, sourcename);
			t = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_typedef));
			t->filenum = filenum;
			t->typenum = typenum;
			t->type = T_ENUM;
			t->enum_ptr = s;
			t->points_to = NULL;
			IExec->AddTail (&(file->typedef_list), (struct Node *)t);
		}
		else if(*strptr == 'x')	//unknown size
		{
			strptr++;

			t = freemem_malloc(stabs_freemem_hook, sizeof(struct stab_typedef));
			t->filenum = filenum;
			t->typenum = typenum;
			t->type = T_CONFORMANT_ARRAY;
			t->struct_ptr = NULL;
			t->points_to = NULL;
			IExec->AddTail(&(file->unknown_list), (struct Node *)t);
		}
		else
		{
			if(*strptr == '(')
			{
				int filenum2, typenum2;
				typenum2 = stabs_interpret_number_token(strptr, &filenum2);
				if (typenum2 == typenum && filenum2 == filenum)
				{
					// void
					struct stab_typedef *t = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_typedef));
					
					t->name = NULL;
					t->filenum = filenum;
					t->typenum = typenum;
					t->type = T_VOID;
					IExec->AddTail (&(file->typedef_list), (struct Node *)t);
					return t;
				}
				
			}

			t = stabs_get_type_from_string(strptr, sourcename);

			if (t)
			{
				struct stab_typedef *newt = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_typedef));
				newt->name = NULL;
				newt->filenum = filenum;
				newt->typenum = typenum;
				
				if(ispointer)
				{
					newt->type = T_POINTER;
					newt->points_to = t;
				}
				else
				{
					newt->type = t->type;
					if(t->points_to)
						newt->points_to = t->points_to;
					else
						newt->points_to = t;
				}
				IExec->AddTail (&(file->typedef_list), (struct Node *)newt);
				t = newt;
			}
		}
	}
	if(t)
	{
		struct stab_typedef *oldt = stabs_get_type_from_typenum (&file->unknown_list, filenum, typenum);
		if(oldt)
			IExec->Remove((struct Node *)oldt);
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

	ret = freemem_malloc(stabs_freemem_hook, i+1);

	for (j = 0; j < i; j++)
		ret[j] = str[j];
	
	ret[j] = '\0';

	return ret;
}

BOOL str_is_typedef (char *str)
{
	char *ptr = str;
	ptr = skip_in_string (ptr, ":");
	return (*ptr == 't' || *ptr == 'T' ? TRUE : FALSE);
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
				sourcefile = freemem_malloc(stabs_freemem_hook, sizeof (struct stab_sourcefile));

				sourcefile->filename = freemem_strdup(stabs_freemem_hook, &stabstr[sym->n_strx]);
				IExec->NewList (&(sourcefile->typedef_list));
				IExec->NewList (&(sourcefile->function_list));
				IExec->NewList (&(sourcefile->unknown_list));
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
					struct stab_typedef * t = stabs_get_type_from_string(strptr, sourcefile->filename);
					if(t)
						t->name = name;
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
			symbol = freemem_malloc(stabs_freemem_hook, sizeof(struct stab_symbol));

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

void stabs_interpret_stabs()
{
	stabs_freemem_hook = freemem_alloc_hook();
	stabs_interpret_typedefs();
	stabs_interpret_functions();
	stabs_interpret_globals();
}

void stabs_free_stabs()
{
	freemem_free_hook(stabs_freemem_hook);
}
