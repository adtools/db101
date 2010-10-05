;/* execute me to compile
gcc -o DynamicHelpHints DynamicHelpHints.c -lauto
quit
*/

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/button.h>
#include <proto/string.h>
#include <proto/layout.h>
#include <proto/label.h>
#include <proto/listbrowser.h>
#include <proto/asl.h>
#include <proto/requester.h>

#include <classes/window.h>
#include <classes/requester.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

#include <stdio.h>

enum
{
    GAD_BUTTON = 1,
};

struct Window *win;


char * PrintMsg(CONST_STRPTR text, int REQ_TYPE, int REQ_IMAGE)
{
	Object *reqobj;

	char *ret = malloc(1024);

	if (REQ_TYPE  == 0) REQ_TYPE	= REQTYPE_STRING;
	if (REQ_IMAGE == 0) REQ_IMAGE	= REQIMAGE_DEFAULT;

	reqobj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
            									REQ_Type,       REQ_TYPE,
												REQ_TitleText,  "Test",
												REQS_Buffer,	ret,
												REQS_MaxChars,	1024,
									            REQ_BodyText,   text,
									            REQ_Image,      REQ_IMAGE,
												REQ_TimeOutSecs, 10,
									            REQ_GadgetText, "Ok",
									            TAG_END
        );

	if ( reqobj )
	{
		IIntuition->IDoMethod( reqobj, RM_OPENREQ, NULL, win, NULL, TAG_END );
		IIntuition->DisposeObject( reqobj );
	}

	return ret;
}

int main( int argc, char *argv[] )
{
    Object *WinObj, *ButtonObj;


    /* Create the window object. */
    if(( WinObj = WindowObject,
            WA_ScreenTitle, "Suspender",
            WA_Title, "Suspender v.0.1",
            WA_DepthGadget, TRUE,
			WA_SizeGadget, TRUE,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_Activate, TRUE,
            WINDOW_Position, WPOS_CENTERSCREEN,
            /* there is no HintInfo array set up here, instead we define the 
            ** strings directly when each gadget is created (OM_NEW)
            */
	        WINDOW_Layout, VLayoutObject,
	                LAYOUT_AddChild, ButtonObj = ButtonObject,
	                    GA_ID, GAD_BUTTON,
	                    GA_RelVerify, TRUE,
	                    GA_Text, "Select file",
	                ButtonEnd,
	        End,

        EndWindow))
    {
        /*  Open the window. */
        if( win = (struct Window *) RA_OpenWindow(WinObj) )
        {   
            ULONG wait, signal, result, done = FALSE;
            WORD Code;
			char *msg;

            /* Obtain the window wait signal mask. */
            IIntuition->GetAttr( WINDOW_SigMask, WinObj, &signal );
            
            /* Input Event Loop */
            while( !done )
            {
                wait = IExec->Wait(signal|SIGBREAKF_CTRL_C);
                
                if (wait & SIGBREAKF_CTRL_C) done = TRUE;
                else
                while ((result = RA_HandleInput(WinObj, &Code)) != WMHI_LASTMSG)
                {
                    switch (result & WMHI_CLASSMASK)
                    {
printf("result = 0x%x\n", result);
                        case WMHI_CLOSEWINDOW:
                            done = TRUE;
                            break;

                         case WMHI_GADGETUP:
                            switch(result & WMHI_GADGETMASK)
                            {
                                case GAD_BUTTON:
                                    /* if the user has entered a new text for the buttons
                                    ** help text, get it, and set it
                                    */

									msg = PrintMsg("Hello world!", 0, 0);
									printf("got the msg: %s\n", msg);

                                    break;
                            }
                            break;
                    }
                }
            }
        }
        
        /* Disposing of the window object will
         * also close the window if it is
         * already opened and it will dispose of
         * all objects attached to it.
         */
        IIntuition->DisposeObject( WinObj );
    }
    
    return RETURN_OK;
}
