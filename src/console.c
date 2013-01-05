/* console.c */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/listbrowser.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "gui.h"
#include "freemem.h"
#include "console.h"
#include "stabs.h"

//Object *MainObj[GAD_CONSOLE_LISTBROWSER];
struct List console_list;

int console_freemem_hook = -1;

extern char childpath[1024];
extern struct Window *mainwin;

int16 console_pens[OUTPUT_NO_TYPES];
uint32 console_colors[OUTPUT_NO_TYPES] =
{
	0xff000000,
	0xffaa0000,
	0xff0000aa,
	0xff00aa00,
	0xffaa00aa
};

void console_init()
{
	IExec->NewList (&console_list);
	console_freemem_hook = freemem_alloc_hook();
	
	struct Screen *scrn  = IIntuition->LockPubScreen(NULL);
	struct ColorMap *cm = scrn->ViewPort.ColorMap;
	int i;
	for(i = 0; i < OUTPUT_NO_TYPES; i++)
	{
		console_pens[i] = IGraphics->ObtainBestPen(cm, console_colors[i] << 8, console_colors[i] << 16, console_colors[i] << 24, TAG_DONE);
	}
	IIntuition->UnlockPubScreen(NULL, scrn);
}

void console_cleanup()
{
	struct Screen *scrn  = IIntuition->LockPubScreen(NULL);
	struct ColorMap *cm = scrn->ViewPort.ColorMap;
	int i;
	for(i = 0; i < OUTPUT_NO_TYPES; i++)
		IGraphics->ReleasePen(cm, console_pens[i]);
	IIntuition->UnlockPubScreen(NULL, scrn);
	
	IListBrowser->FreeListBrowserList(&console_list);
	freemem_free_hook(console_freemem_hook);
}

void console_clear()
{
	IIntuition->SetAttrs(MainObj[GAD_CONSOLE_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
	IListBrowser->FreeListBrowserList(&console_list);
	IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_CONSOLE_LISTBROWSER], mainwin, NULL,
												LISTBROWSER_Labels, &console_list,
												TAG_END);
	freemem_clear(console_freemem_hook);
}

void strip_for_tabs (char *str)
{
	if (!str) return;
	char *ptr = str;
	while (*ptr != '\0')
	{
		if (*ptr == '\t' || *ptr == '\n')
			*ptr = ' ';
		ptr++;
	}
}

char *getlinefromfile (int line)
{
	char *ret = NULL;
	char fullpath[1024];

	if (!hasfunctioncontext)
		return ret;

	// if current function comes from an included file, the included name is what we are looking for
	//char *sourcename = (current_function->isinclude ? current_function->includename : current_function->sourcename);
	char *sourcename = current_function->sourcefile->filename;
	char *realname;
	int u = 0;
	if((u=__unix_to_amiga_path_name(sourcename, &realname)) == 1)
		sourcename = realname;

	if (strlen (sourcename) == 0)
		return ret;
	if (strlen (childpath) == 0)
		strcpy (fullpath, sourcename);
	else if (childpath[strlen(childpath)-1] == '/')
		sprintf(fullpath, "%s%s", childpath, sourcename);
	else
		sprintf(fullpath, "%s/%s", childpath, sourcename);


	ret = freemem_malloc(console_freemem_hook, 1024);

	FILE *stream = fopen (fullpath, "r");
	if (stream)
	{
		int i;
		for(i = 1; ; i++)
		{
			if (0 >= fgets (ret, 1024, stream))
				break;
			if (i == line)
				break;
		}
		fclose (stream);
	}
	else
		sprintf(ret,  "-- <%s> --", sourcename);

	strip_for_tabs(ret);
	if (u == 1)
		free(realname);
	return ret;
}

void console_printf(enum output_types type, char *fmt, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	IIntuition->SetAttrs(MainObj[GAD_CONSOLE_LISTBROWSER], LISTBROWSER_Labels, ~0, TAG_DONE);
	struct Node *node;
	char *str = freemem_strdup (console_freemem_hook, buffer);
	if (node = IListBrowser->AllocListBrowserNode(1,
												LBNA_Flags, LBFLG_CUSTOMPENS,
            									LBNA_Column, 0,
            									LBNCA_FGPen, console_pens[type],
                								LBNCA_Text, str,
            									TAG_DONE))
        							{
							            IExec->AddTail(&console_list, node);
									}
	IIntuition->SetGadgetAttrs((struct Gadget *)MainObj[GAD_CONSOLE_LISTBROWSER], mainwin, NULL,
												LISTBROWSER_Labels, &console_list,
												LISTBROWSER_Position, LBP_BOTTOM,
												TAG_END);
}

void console_write_raw_data(enum output_types type, char *buffer, int len)
{
	char temp[1024];
	int i;
	int t = 0;
	for(i = 0; i < len; i++)
	{
		switch(buffer[i])
		{
			case '\n':
			case '\0':
				temp[t] = '\0';
				if(strlen(temp))
					console_printf(type, temp);
				t = 0;
				break;
			default:
				temp[t++] = buffer[i];
				break;
		}
	}
}

void output_lineinfile (uint32 line)
{
	console_printf(OUTPUT_NORMAL, "[line %d]: %s", line, getlinefromfile(line));
}

void output_functionheader ()
{
	console_printf(OUTPUT_NORMAL, "entering: %s() in <%s>", current_function->name, current_function->sourcefile->filename);
}
