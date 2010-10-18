#include "suspend.h"
#include "gui.h"
#include "breakpoints.h"
#include "arexxport.h"

int main(int argc, char *argv[])
{
	init();
	init_breakpoints();
	arexx_open_port();
	main_open_window();
	event_loop();
	cleanup();
	//remove_hook();
	end();
	return 0;

}
