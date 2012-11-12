#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

#include "console.h"
#include "async.h"

BPTR fd[2];
char pipe_filename[120] = "";
BYTE pipe_signal = -1;
ULONG pipe_sigfield = 0;
//struct NotifyRequest *notifyreq;
struct MsgPort *asyncport = NULL;
char pipe_buffer[1024];

void pipe_init()
{
	int noblock = 0;
	struct ExamineData *data;

	//char filename2[120];
	int ret=0;

	fd[1] = IDOS->Open ("PIPE:/UNIQUE/NOBLOCK", MODE_NEWFILE);
	if (fd[1] == 0)
	{
		console_printf(OUTPUT_WARNING, "DOS couldn't open PIPE:/UNIQUE");
		return;
	}
	data = IDOS->ExamineObjectTags(EX_FileHandleInput, fd[1], TAG_END);
	if (data == NULL)
	{
		IDOS->PrintFault(IDOS->IoErr(),NULL); /* failure - why ? */
		IDOS->Close(fd[1]);
		return;
	}

	strlcpy(pipe_filename, "PIPE:", sizeof(pipe_filename));
	strlcat(pipe_filename, data->Name, sizeof(pipe_filename));
	IDOS->FreeDosObject(DOS_EXAMINEDATA, data);

	if (noblock)
	{
		//strlcpy(filename2, filename, sizeof(filename));
		strlcat(pipe_filename, "/NOBLOCK", sizeof(pipe_filename));
		//printf("pipe(): filename2 = \"%s\"\n", filename2);
	}

	fd[0] = IDOS->Open (pipe_filename, MODE_OLDFILE);
	if (fd[0] == 0)
	{
		console_printf(OUTPUT_WARNING, "DOS couldn't open %s", pipe_filename);
		return;
	}
#if 0
	pipe_signal = IExec->AllocSignal(-1);
	pipe_sigfield = 1 << pipe_signal;
	notifyreq = (struct NotifyRequest *)IDOS->AllocDosObjectTags(DOS_NOTIFYREQUEST,
															ADO_NotifyName, pipe_filename,
															ADO_NotifyMethod, NRF_SEND_SIGNAL,
															ADO_NotifyTask, IExec->FindTask(NULL),
															ADO_NotifySignalNumber, pipe_signal,
															TAG_END);
	if(!notifyreq)
		console_printf(OUTPUT_WARNING, "Failed to alloc notify request!");
#endif
	asyncport = (struct MsgPort *)IExec->AllocSysObjectTags(ASOT_PORT,
														TAG_END);
	if(!asyncport)
	{
		console_printf(OUTPUT_WARNING, "Failed to allocate pipe message port!");
		return;
	}
	pipe_signal = asyncport->mp_SigBit;
	pipe_sigfield = 1 << pipe_signal;
	Async_StartRead (fd[0],pipe_buffer,1023,asyncport);
}

void pipe_cleanup()
{
	IDOS->Write(fd[1], "end", 3);
	Async_WaitDosIO(asyncport);
	IExec->FreeSysObject(ASOT_PORT, asyncport);
	//IDOS->FreeDosObject(DOS_NOTIFYREQUEST, notifyreq);
	//IExec->FreeSignal(pipe_signal);
	IDOS->Close(fd[0]);
	IDOS->Close(fd[1]);
}

ULONG pipe_obtain_signal()
{
	return pipe_sigfield;
}

BPTR pipe_get_write_end()
{
	return fd[1];
}

BPTR pipe_get_read_end()
{
	return fd[0];
}

int pipe_read(char *buffer)
{
	int bytes_read = Async_WaitDosIO (asyncport);
	pipe_buffer[bytes_read] = '\0';
	strcpy(buffer, pipe_buffer);
	Async_StartRead (fd[0],pipe_buffer,1023,asyncport);
	return bytes_read;
}
