#include "suspend.h"
#include "gui.h"
#include "breakpoints.h"
#include "arexxport.h"
#include "freemem.h"
#include "preferences.h"
#include "stabs.h"

#include <string.h>

int main(int argc, char *argv[])
{
	init();
	freemem_init();
	init_breakpoints();
	//tracking_init();
	modules_init();
	stabs_init();
	modules_load_list();
	arexx_open_port();
	if (argc < 2 || strcmp("NOGUI", argv[1]) )
		main_open_window();
	load_preferences();
	event_loop();
	cleanup();
	//tracking_cleanup();
	modules_cleanup();
	//remove_hook();
	end();
	return 0;

}
