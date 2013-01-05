void modules_open_window();
void modules_close_window();
void modules_event_loop();

void modules_init();
void modules_cleanup();
void modules_clear();

void modules_add_entry(char *, char *, BOOL);
void modules_remove_entry(char *);

void modules_update_list();
void modules_update_stabs();

BOOL modules_load_list();
void modules_save_list();

void modules_update_window();

struct modules_entry
{
	struct Node node;
	char *name;
	char *sourcepath;
	BOOL accessallowed;
	BOOL hasbeenloaded;
};

struct modules_entry *modules_lookup_entry(char *);
char *modules_request_path(char *, char *);
