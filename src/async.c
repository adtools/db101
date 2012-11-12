/* Demonstrating asyncronous input from a console window                      */

/* ****** This code has been kindly provided by Thomas Rapp ! :) **************/
/* ************ (Slightly modified for db101 by Alfkil ************************/

/*----------------------------------------------------------------------------*/
/* System includes                                                            */
/*----------------------------------------------------------------------------*/
#define __USE_INLINE__
	#include <proto/exec.h>
	#include <proto/dos.h>
	#include <proto/graphics.h>
	#include <proto/intuition.h>
	#include <string.h>
	#include <stdio.h>

/*----------------------------------------------------------------------------*/
/* Start asyncronous read from a file                                         */
/*----------------------------------------------------------------------------*/

	struct MsgPort *Async_StartRead (BPTR file,APTR buffer,LONG size,struct MsgPort *port)

	{
	struct FileHandle *fh = BADDR(file);
	struct DosPacket *packet;

	if (fh && fh->fh_Type)
		{
		if (packet = AllocDosObject (DOS_STDPKT,TAG_END))
			{
			packet->dp_Port = port;

			packet->dp_Type = ACTION_READ;
			packet->dp_Arg1 = fh->fh_Arg1;
			packet->dp_Arg2 = (LONG)buffer;
			packet->dp_Arg3 = (LONG)size;

			PutMsg (fh->fh_Type,packet->dp_Link);
			return (port);
			}
		}

	return (NULL);
	}

/*----------------------------------------------------------------------------*/
/* Start asyncronous write to a file                                          */
/*----------------------------------------------------------------------------*/

	struct MsgPort *Async_StartWrite (BPTR file,APTR buffer,LONG size,struct MsgPort *port)

	{
	struct FileHandle *fh = BADDR(file);
	struct DosPacket *packet;

	if (fh && fh->fh_Type)
		{
		if (packet = AllocDosObject (DOS_STDPKT,TAG_END))
			{
			packet->dp_Port = port;

			packet->dp_Type = ACTION_WRITE;
			packet->dp_Arg1 = fh->fh_Arg1;
			packet->dp_Arg2 = (LONG)buffer;
			packet->dp_Arg3 = (LONG)size;

			PutMsg (fh->fh_Type,packet->dp_Link);
			return (port);
			}
		}

	return (NULL);
	}

/*----------------------------------------------------------------------------*/
/* Wait for asyncronous read/write to complete and fetch the result           */
/*----------------------------------------------------------------------------*/

	LONG Async_WaitDosIO (struct MsgPort *port)

	{
	struct Message *msg;
	struct DosPacket *packet;
	long rc;

	WaitPort (port);
	msg = GetMsg (port);
	packet = (struct DosPacket *) msg->mn_Node.ln_Name;
	rc = packet->dp_Res1;
	FreeDosObject (DOS_STDPKT,packet);

	return (rc);
	}

#if 0
/*----------------------------------------------------------------------------*/
/* Main program                                                               */
/*----------------------------------------------------------------------------*/

	int main (void)

	{
	struct MsgPort *asyncport;	/* message port where the asyncronous request is replied to */
	BPTR console = Input();	/* AmigaDOS file handler of the console window */ 

	if (asyncport = (struct MsgPort *)AllocSysObjectTags(ASOT_PORT,
														TAG_END))
		{
		struct Window *win;

		if (win = OpenWindowTags (NULL,
				WA_Width,300,
				WA_Height,100,
				WA_Flags,WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_REPORTMOUSE | WFLG_ACTIVATE | WFLG_NOCAREREFRESH,
				WA_IDCMP,IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_MOUSEMOVE,
				TAG_END))
			{
			BOOL cont = TRUE;
			ULONG winsig = 1L << win->UserPort->mp_SigBit;
			ULONG consig = 1L << asyncport->mp_SigBit;
			char buffer[80];

			StartRead (console,buffer,79,asyncport);

			do	{
				ULONG sigs = Wait (winsig | consig | SIGBREAKF_CTRL_C);

				if (sigs & winsig)
					{
					struct IntuiMessage *imsg;
					char text[80];
					while (imsg = (struct IntuiMessage *) GetMsg (win->UserPort))			
						{
						switch (imsg->Class)
							{
						case IDCMP_MOUSEMOVE:
							sprintf (text,"Mouse moved to x=%ld; y=%ld    ",imsg->MouseX,imsg->MouseY);
							Move (win->RPort,win->BorderLeft + 10,win->BorderTop + 10 + win->RPort->TxBaseline);
							SetABPenDrMd (win->RPort,1,0,JAM2);
							Text (win->RPort,text,strlen(text));
							break;
						case IDCMP_VANILLAKEY:
							if (imsg->Code == 0x1b) /* Esc */
								cont = FALSE;
							break;
						case IDCMP_CLOSEWINDOW:
							cont = FALSE;
							break;
							}
						ReplyMsg ((struct Message *) imsg);
						}
					}

				if (sigs & consig)
					{
					long bytes_read = WaitDosIO (asyncport);
					buffer[bytes_read] = '\0';
					Printf ("input from the conole: %s\n",buffer);
					StartRead (console,buffer,79,asyncport);
					}

				if (sigs & SIGBREAKF_CTRL_C)
					cont = FALSE;
				}
			while (cont);

			CloseWindow (win);

			Printf ("Please press enter to end the program\n");
			WaitDosIO (asyncport);
			}

		FreeSysObject(ASOT_PORT, asyncport);
		}

	return (RETURN_OK);
	}

/*----------------------------------------------------------------------------*/
/* End of source text                                                         */
/*----------------------------------------------------------------------------*/
#endif
