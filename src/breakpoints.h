/*breakpoints.h*/

void init_breakpoints();
BOOL is_breakpoint_at(uint32);
void install_all_breakpoints(void);
void suspend_all_breakpoints(void);
void insert_breakpoint (uint32, uint32);
void remove_breakpoint (uint32);
void insert_line_breakpoints(void);
void remove_line_breakpoints(void);
void guess_line_in_function(void);
uint32 translate_sline_to_nline(uint32);
int get_nline_from_address(uint32);

BOOL breakpoint_line_in_file(uint32, char *);
BOOL breakpoint_function (char *);

void asmstep_install(void);
void asmstep_remove(void);


struct breakpoint
{
	struct Node node;
	uint32 address;
	uint32 save_buffer;
	uint32 type;
};

enum __breakpoint_type
{
	BR_NORMAL,
	BR_LINE,
	BR_TRACE
};

extern struct List breakpoint_list;
extern BOOL breakpoints_installed;
