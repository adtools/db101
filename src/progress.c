#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <proto/fuelgauge.h>

#include <classes/window.h>
#include <gadgets/fuelgauge.h>

#include <reaction/reaction_macros.h>

#include "progress.h"

enum
{
	PRGR_WINDOW = 0,
	PRGR_FUELGAUGE,
	
	PRGR_NUM
};
Object *PrgrObj[PRGR_NUM];
struct Window *prgrwin = NULL;
extern struct Window *mainwin;

static int progress_oldcurrent;
static int progress_total;

void open_progress(char *title, int total, int initial)
{
	progress_oldcurrent = initial;
	progress_total = total;

	if((PrgrObj[PRGR_WINDOW] = WindowObject,
			WA_ScreenTitle, "Db101",
			WA_Title, title,
			WA_Activate, TRUE,
			WA_DepthGadget, TRUE,
			WA_DragBar, TRUE,
			WA_Width, 300,
			//WA_CloseGadget, TRUE,
			//WA_SizeGadget, TRUE,
			//WINDOW_IconifyGadget, TRUE,
			//WINDOW_IconTitle, "FuelGauge",
			//WINDOW_AppPort, AppPort,
			WINDOW_Position, WPOS_CENTERSCREEN,
			WINDOW_ParentGroup, VGroupObject,
				LAYOUT_SpaceOuter, TRUE,
				LAYOUT_DeferLayout, TRUE,

				LAYOUT_AddChild, PrgrObj[PRGR_FUELGAUGE] = FuelGaugeObject,
					GA_ID, PRGR_FUELGAUGE,
					FUELGAUGE_Orientation, FGORIENT_HORIZ,
					FUELGAUGE_Min, 0,
					FUELGAUGE_Max, total,
					FUELGAUGE_Level, initial,
					FUELGAUGE_Percent, TRUE,
					FUELGAUGE_TickSize, 5,
					FUELGAUGE_Ticks, 5,
					FUELGAUGE_ShortTicks, TRUE,
				FuelGaugeEnd,
			EndGroup,
		EndWindow))
		if((prgrwin = (struct Window *)RA_OpenWindow(PrgrObj[PRGR_WINDOW])))
			IIntuition->SetWindowAttr(mainwin, WA_BusyPointer, (APTR)TRUE, 0L);
}

void update_progress(char *title, int total, int current)
{
	progress_total = total;
	progress_oldcurrent = current;
	
	if(prgrwin)
	{
		IIntuition->SetGadgetAttrs((struct Gadget *)PrgrObj[PRGR_FUELGAUGE], prgrwin, NULL,
							FUELGAUGE_Max, total,
							FUELGAUGE_Level, current,
							TAG_DONE);
		IIntuition->SetAttrs(PrgrObj[PRGR_WINDOW], WA_Title, title, TAG_END);
	}
}

void update_progress_val(int current)
{
	if(prgrwin)
	{
		if(progress_total/(current-progress_oldcurrent) <= 20)
		{
			IIntuition->SetGadgetAttrs((struct Gadget *)PrgrObj[PRGR_FUELGAUGE], prgrwin, NULL,
							FUELGAUGE_Level, current,
							TAG_DONE);
			progress_oldcurrent = current;
		}
	}
}

void close_progress()
{
	IIntuition->DisposeObject(PrgrObj[PRGR_WINDOW]);
	prgrwin = NULL;
	IIntuition->SetWindowAttr(mainwin, WA_BusyPointer, (APTR)FALSE, 0L);
}
