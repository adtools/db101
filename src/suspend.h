#include <exec/types.h>
#include <dos/dos.h>
#include <libraries/elf.h>

extern int load_inferior(char *, char *, char *);
extern BOOL play(void);
extern void pause(void);
extern void step(void);
extern void into(void);
extern void out(void);
extern void asmstep(void);
extern void asmskip(void);
extern void killtask(void);
extern void crash(void);

BOOL attach();
void detach();

int memory_insert_breakpoint(uint32, uint32 *);
int memory_remove_breakpoint(uint32, uint32 *);

extern BOOL task_exists;
extern BOOL task_playing;
extern BOOL arrived_at_breakpoint;

extern ULONG debug_sigfield;
extern ULONG debug_data;

extern struct Process *process;
extern BPTR exec_seglist;
extern Elf32_Handle exec_elfhandle;
extern Elf32_Addr code_elf_addr;
extern int code_size;
extern void *code_relocated_addr;

extern struct Hook debug_hook;
extern struct ExceptionContext *context_ptr, context_copy;

extern BOOL asm_trace;
extern BOOL breakpoint_set;
extern BOOL should_continue;
extern BOOL entering_function;

extern struct DebugIFace *IDebug;
extern struct MMUIFace *IMMU;

