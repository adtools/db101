/* console.h */

void console_init();
void console_cleanup();
void console_clear();

void console_statement(char *);
void console_lineinfile(uint32);
void console_functionheader();

void strip_for_tabs (char *);

enum output_types
{
	OUTPUT_NORMAL,
	OUTPUT_WARNING,
	OUTPUT_SYSTEM,
	OUTPUT_FROM_EXECUTABLE,
	OUTPUT_AREXX,
	OUTPUT_NO_TYPES
};
void console_printf(enum output_types, char *, ...);
void console_write_raw_data(enum output_types, char *, int);
