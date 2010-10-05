/* stabs.h */

#ifndef STABS_H
#define STABS_H

#include <exec/lists.h>

extern char * stabs_get_function_for_address (uint32);

struct stab_function
{
	char *name;
	uint32 address;
	uint32 size;
	char *sourcename;
	uint32 numberoflines;
	uint32 lines[1024];      //list of addresses relative to funtion address
	uint32 lineinfile[1024];
	uint32 linetype[1024];
	uint32 currentline;
	struct List symbols;
};

enum __line_types
{
	LINE_NORMAL,
	LINE_LOOP
};


struct stab_symbol
{
	struct Node node;
	char *name;
	struct stab_typedef *type;
	uint32 location;
	union {
		uint32 address;
		uint32 reg;
		uint32 offset;
	};
};


enum __stab_location
{
	L_REGISTER,
	L_STACK,
	L_ABSOLUTE
};

enum __stab_symbol_types
{
	T_UNKNOWN = 0,
	T_STRUCT,
	T_VOID,
	T_BOOL,
	T_U8,
	T_8,
	T_U16,
	T_16,
	T_U32,
	T_32,
	T_U64,
	T_64,
	T_FLOAT32,
	T_FLOAT64
};

struct stab_typedef
{
	struct Node node;
	char *name;
	uint32 type;
	uint32 array;
	uint32 number;
	uint32 ispointer;
};

struct stab_sourcefile
{
	struct Node node;
	char *filename;
	struct List function_list;
	struct List typedef_list;
};


extern struct stab_function *stabs_interpret_function(char *, uint32);
extern void stabs_make_function_list();
void stabs_interpret_typedefs(void);
void stabs_interpret_globals(void);

extern char *stabs_strdup_strip(char *);
char *skip_in_string (char *, char *);

struct stab_typedef * stabs_get_type_from_string (char *, char *);

extern uint32 numberoffunctions;
extern uint32 function_addresses[];
extern char * function_names[];

extern struct List global_symbols;

struct symtab_entry {
	unsigned long n_strx;
	unsigned char n_type;
	unsigned char n_other;
	unsigned short n_desc;
	unsigned int n_value;
};


enum __stab_debug_code
{
N_UNDF = 0x00,
N_GSYM = 0x20,
N_FNAME = 0x22,
N_FUN = 0x24,
N_STSYM = 0x26,
N_LCSYM = 0x28,
N_MAIN = 0x2a,
N_ROSYM = 0x2c,
N_PC = 0x30,
N_NSYMS = 0x32,
N_NOMAP = 0x34,
N_OBJ = 0x38,
N_OPT = 0x3c,
N_RSYM = 0x40,
N_M2C =  0x42,
N_SLINE = 0x44,
N_DSLINE = 0x46,
N_BSLINE = 0x48,
N_BROWS = 0x48,
N_DEFD = 0x4a,
N_FLINE = 0x4C,
N_EHDECL = 0x50,
N_MOD2 = 0x50,
N_CATCH = 0x54,
N_SSYM = 0x60,
N_ENDM = 0x62,
N_SO = 0x64,
N_ALIAS = 0x6c,
N_LSYM = 0x80,
N_BINCL = 0x82,
N_SOL = 0x84,
N_PSYM = 0xa0,
N_EINCL = 0xa2,
N_ENTRY = 0xa4,
N_LBRAC = 0xc0,
N_EXCL = 0xc2,
N_SCOPE = 0xc4,
N_PATCH = 0xd0,
N_RBRAC = 0xe0,
N_BCOMM = 0xe2,
N_ECOMM = 0xe4,
N_ECOML = 0xe8,
N_WITH = 0xea,
N_NBTEXT = 0xF0,
N_NBDATA = 0xF2,
N_NBBSS =  0xF4,
N_NBSTS =  0xF6,
N_NBLCS =  0xF8,
N_LENG = 0xfe,
LAST_UNUSED_STAB_CODE
};

/* Definitions of "desc" field for N_SO stabs in Solaris2.  */

#define	N_SO_AS		1
#define	N_SO_C		2
#define	N_SO_ANSI_C	3
#define	N_SO_CC		4	/* C++ */
#define	N_SO_FORTRAN	5
#define	N_SO_PASCAL	6

/* Solaris2: Floating point type values in basic types.  */

#define	NF_NONE		0
#define	NF_SINGLE	1	/* IEEE 32-bit */
#define	NF_DOUBLE	2	/* IEEE 64-bit */
#define	NF_COMPLEX	3	/* Fortran complex */
#define	NF_COMPLEX16	4	/* Fortran double complex */
#define	NF_COMPLEX32	5	/* Fortran complex*16 */
#define	NF_LDOUBLE	6	/* Long double (whatever that is) */


#endif