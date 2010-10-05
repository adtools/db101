#include "suspend.h"
#include "gui.h"
#include "breakpoints.h"

int main(int argc, char *argv[])
{
	init();
	init_breakpoints();
	main_open_window();
	event_loop();
	cleanup();
	//remove_hook();
	end();
	return 0;

}
