#include <proto/dos.h>
#include <proto/exec.h>

struct MsgPort *Async_StartRead (BPTR file,APTR buffer,LONG size,struct MsgPort *port);
struct MsgPort *Async_StartWrite (BPTR file,APTR buffer,LONG size,struct MsgPort *port);
LONG Async_WaitDosIO (struct MsgPort *port);
