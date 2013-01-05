void preferences_open_window();
void preferences_event_loop();
void preferences_close_window();

enum
{
	ENTRY_LOAD_ALL,
	ENTRY_LOAD_SOURCELIST,
	ENTRY_LOAD_NOTHING
};

enum
{
	EXT_ALWAYS,
	EXT_ASK,
	EXT_NEVER
};

struct preferences
{
	int prefs_load_at_entry;
	int prefs_import_externals;
	int prefs_allow_kernel_access; //BOOL
	int prefs_keep_elf_open;
	int prefs_suspend_when_attach;
	int prefs_show_main_at_entry;
	int prefs_auto_load_globals;
};

extern struct preferences db101_prefs;
