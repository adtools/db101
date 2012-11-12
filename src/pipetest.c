#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>
#include <stdio.h>

#include "async.h"
#include "pipe.h"

int main()
{
	pipe_init();
	char *strings[] = { "hello world", "nice to see ya world", "goodbuy world" };
	
	int i;
	for(i = 0; i < 3; i++)
	{
		IDOS->Write(pipe_get_write_end(), strings[i], strlen(strings[i]));
		ULONG sig = IExec->Wait(pipe_obtain_signal());
		char buffer[1024];
		int len = pipe_read(buffer);
		if(strlen(buffer))
			printf("%s\n", buffer);
	}
	pipe_cleanup();
	return 0;
}
