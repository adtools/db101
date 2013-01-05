/*breakpoints.h*/

void init_breakpoints();
BOOL is_breakpoint_at(uint32);
void install_all_breakpoints(void);
void suspend_all_breakpoints(void);
void insert_breakpoint (uint32, uint32, APTR, uint32);
void remove_breakpoint (uint32);
void insert_line_breakpoints(void);
void remove_line_breakpoints(void);
int guess_line_in_function(void);
uint32 translate_sline_to_nline(uint32);
int get_nline_from_address(uint32);

BOOL breakpoint_line_in_file(uint32, char *);
BOOL breakpoint_function (char *);

void free_breakpoints(void);

void asmstep_install(void);
void asmstep_remove(void);
void stepout_install(void);
void stepout_remove(void);

struct breakpoint
{
	struct Node node;
	uint32 address;
	uint32 save_buffer;
	uint32 type;
	//uint32 state;
	union {
		struct stab_function *function;
		struct amigaos_symbol *symbol;
	};
	uint32 line;
};
#if 0
enum __breakpoint_state
{
	BR_ACTIVE,
	BR_SUSPENDED
};
#endif

enum __breakpoint_type
{
	BR_NORMAL_FUNCTION,
	BR_NORMAL_SYMBOL,
	BR_NORMAL_ABSOLUTE,
	BR_LINE,
	BR_TRACE
};

extern struct List breakpoint_list;
extern BOOL breakpoints_installed;
