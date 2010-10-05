/* stacktrace.h */

void stacktrace_open_window(void);
void stacktrace_close_window(void);
uint32 stacktrace_obtain_window_signal(void);
void stacktrace_event_handler(void);
void stacktrace_update(void);
