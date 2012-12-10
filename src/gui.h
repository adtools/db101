#include <proto/dos.h>
#include <proto/intuition.h>

#include "stabs.h"

void main_open_window(void);
void main_close_window(void);
void main_event_handler(void);

void main_play(void);
void main_pause(void);
void main_step(void);
void main_kill(void);
void main_load(char *, char *, char *);
BOOL main_attach(struct Process *);

void event_loop(void);
void cleanup(void);

extern struct stab_function *current_function;
extern BOOL hasfunctioncontext;
extern BOOL isattached;

enum
{
	GAD_TOPLAYOUT = 1,
	
    GAD_SELECT_BUTTON,
   	GAD_FILENAME_STRING,
    GAD_RELOAD_BUTTON,
	GAD_ATTACH_BUTTON,
	GAD_START_BUTTON,
	GAD_PAUSE_BUTTON,
	GAD_STEPOVER_BUTTON,
	GAD_STEPINTO_BUTTON,
	GAD_STEPOUT_BUTTON,
	GAD_KILL_BUTTON,
	GAD_CRASH_BUTTON,
	GAD_SETBREAK_BUTTON,
	GAD_HEX_BUTTON,
	GAD_VARIABLES_LISTBROWSER,
	GAD_STACKTRACE_LISTBROWSER,
	GAD_CLICKTAB,
	GAD_SOURCE_LISTBROWSER,
	GAD_DISASSEMBLER_LISTBROWSER,
	GAD_DISASSEMBLER_STEP_BUTTON,
	GAD_DISASSEMBLER_SKIP_BUTTON,
	GAD_SOURCELIST_LISTBROWSER,
	GAD_CONSOLE_LISTBROWSER,
	GAD_AREXX_STRING,
	GAD_AREXX_BUTTON,
	GAD_SIGNAL_BUTTON,
	GAD_X_BUTTON,
	
	GAD_NUM
};

extern Object *MainObj[GAD_NUM];
