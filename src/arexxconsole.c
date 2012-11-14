/* arexxconsole.c */

#include <exec/types.h>
#include <exec/lists.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/button.h>
#include <proto/string.h>
#include <proto/layout.h>
#include <proto/label.h>
#include <proto/asl.h>
#include <proto/elf.h>
#include <proto/chooser.h>
#include <proto/arexx.h>

#include <classes/arexx.h>
#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <gadgets/chooser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

#include <libraries/asl.h>
#include <interfaces/asl.h>

#include <libraries/elf.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "arexxconsole.h"

#if 0
enum
{
	AREXX_BUTTON = 1,
	AREXX_STRING
};
#endif

extern Object *ArexxSendButtonObj, *ArexxStringObj;
extern struct Window *mainwin;
extern Object *arexx_obj;

#if 0
void arexxconsole_open_window()
{
	if (arexx_window_is_open)
		return;

    /* Create the window object. */
    if(( ArexxWinObj = WindowObject,
            WA_ScreenTitle, "DEbug 101",
            WA_Title, "Arexx Console",
            WA_Width, 400,
            WA_DepthGadget, TRUE,
			WA_SizeGadget, TRUE,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_Activate, TRUE,
            WINDOW_Position, WPOS_CENTERSCREEN,

            WINDOW_ParentGroup, HLayoutObject,
                LAYOUT_SpaceOuter, TRUE,
                LAYOUT_DeferLayout, TRUE,

                LAYOUT_AddChild, ArexxStringObj = StringObject,
                    GA_ID, AREXX_STRING,
                    GA_RelVerify, TRUE,
					GA_Disabled, FALSE,
					GA_ReadOnly, FALSE,
					STRINGA_TextVal, "",
                StringEnd,

                LAYOUT_AddChild, ArexxSendButtonObj = ButtonObject,
                    GA_ID, AREXX_BUTTON,
					GA_RelVerify, TRUE,
                    GA_Text, "Send",
                ButtonEnd,
				CHILD_WeightedWidth, 0,

			EndMember,
        EndWindow))

      /*  Open the window. */
        if( arexxwin = (struct Window *) RA_OpenWindow(ArexxWinObj) )
		{
			arexx_window_is_open = TRUE;
		}

	return;
}



uint32 arexxconsole_obtain_window_signal()
{
	uint32 signal = 0x0;
	if (arexx_window_is_open)
		IIntuition->GetAttr( WINDOW_SigMask, ArexxWinObj, &signal );

	return signal;
}
#endif

struct apExecute execute;
char acommandstring[1024];

void arexxconsole_event_handler()
{	
            ULONG wait, result, done = FALSE;
            WORD Code;
			char *string;

            /* Input Event Loop */
                while ((result = RA_HandleInput(ArexxWinObj, &Code)) != WMHI_LASTMSG)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;

                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {

                                case AREXX_BUTTON:
								case AREXX_STRING:

                                    IIntuition->GetAttrs( ArexxStringObj, STRINGA_TextVal, &string, TAG_DONE );
									strcpy (acommandstring, string);

									execute.MethodID = AM_EXECUTE;
									execute.ape_CommandString = acommandstring;
									execute.ape_PortName = "AREXXDB101";
									execute.ape_IO = NULL;

                                    IIntuition->SetAttrs( ArexxStringObj, STRINGA_TextVal, "", TAG_DONE );
									IIntuition->RefreshGadgets ((struct Gadget *)ArexxStringObj, arexxwin, NULL);

									IIntuition->IDoMethodA(arexx_obj, (struct Msg *)&execute);

                                    break;
                            }
                            break;
                    }
					if (done)
					{
						arexxconsole_close_window();
						break;
					}
                }
}

void arexxconsole_close_window()
{
	if (arexx_window_is_open)
	{
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( ArexxWinObj );

		arexx_window_is_open = FALSE;
    }
    return;
}
