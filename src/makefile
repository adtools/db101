# If Kickstart isn't defined, we don't run on AmigaOS
ifndef Kickstart
CROSS_COMPILE = ppc-amigaos-
RM     = rm -R
MKDIR  = mkdir -p
else
RM = delete all
MKDIR = makedir
endif

CC     = $(CROSS_COMPILE)gcc
STRIP  = $(CROSS_COMPILE)strip
CXX    = $(CROSS_COMPILE)c++
AS     = $(CROSS_COMPILE)as
LD     = $(CROSS_COMPILE)ld
RANLIB = $(CROSS_COMPILE)ranlib

all: helloworld ../db101

helloworld: helloworld.c helloworld2.c
	$(CC) -gstabs helloworld.c helloworld2.c -o helloworld
	$(CC) -gstabs -S helloworld.c

suspend.o: suspend.c suspend.h symbols.h gui.h breakpoints.h stabs.h
	$(CC) -c -gstabs -o suspend.o -Wa,-mregnames suspend.c

symbols.o: symbols.c symbols.h suspend.h stabs.h
	$(CC) -c -gstabs -o symbols.o symbols.c

gui.o: gui.c gui.h suspend.h stabs.h symbols.h variables.h disassembler.h breakpoints.h stacktrace.h freemem.h unix.h signalswindow.h
	$(CC) -c -gstabs -o gui.o gui.c

hexview.o: hexview.c hexview.h suspend.h symbols.h freemem.h
	$(CC) -c -gstabs -o hexview.o hexview.c

main.o: main.c suspend.h gui.h breakpoints.h freemem.h
	$(CC) -c -gstabs -o main.o main.c

stabs.o: stabs.c stabs.h suspend.h freemem.h symbols.h
	$(CC) -c -gstabs -o stabs.o stabs.c

variables.o: variables.c variables.h suspend.h stabs.h gui.h freemem.h
	$(CC) -c -gstabs -o variables.o variables.c

disassembler.o: disassembler.c disassembler.h suspend.h stabs.h gui.h freemem.h
	$(CC) -c -gstabs -o disassembler.o disassembler.c

breakpoints.o: breakpoints.c breakpoints.h suspend.h gui.h stabs.h
	$(CC) -c -gstabs -o breakpoints.o breakpoints.c

stacktrace.o: stacktrace.c stacktrace.h suspend.h stabs.h gui.h freemem.h
	$(CC) -c -gstabs -o stacktrace.o stacktrace.c

attach.o: attach.c attach.h freemem.h console.h
	$(CC) -c -gstabs -o attach.o attach.c

breakpointswindow.o: breakpointswindow.c breakpointswindow.h stabs.h suspend.h
	$(CC) -c -gstabs -o breakpointswindow.o breakpointswindow.c

freemem.o: freemem.c freemem.h
	$(CC) -c -gstabs -o freemem.o freemem.c

arexxport.o: arexxport.c arexxport.h suspend.h breakpoints.h gui.h
	$(CC) -c -gstabs -o arexxport.o arexxport.c

console.o: console.c console.h freemem.h gui.h
	$(CC) -c -gstabs -o console.o console.c

source.o: source.c source.h freemem.h console.h stabs.h gui.h breakpoints.h
	$(CC) -c -gstabs -o source.o source.c

pipe.o: pipe.c pipe.h
	$(CC) -c -gstabs -o pipe.o pipe.c

async.o: async.c async.h
	$(CC) -c -gstabs -o async.o async.c

sourcelist.o: sourcelist.c sourcelist.h source.h freemem.h console.h stabs.h gui.h
	$(CC) -c -gstabs -o sourcelist.o sourcelist.c

signalswindow.o: signalswindow.c signalswindow.h suspend.h
	$(CC) -c -gstabs -o signalswindow.o signalswindow.c

setbreak.o: setbreak.s
	$(CC) -c -o setbreak.o -Wa,-mregnames setbreak.s
	
__unix_to_amiga_path_name.o: __unix_to_amiga_path_name.c unix.h
	$(CC) -c -gstabs -o __unix_to_amiga_path_name.o __unix_to_amiga_path_name.c

../db101: suspend.o symbols.o gui.o hexview.o console.o source.o \
			stabs.o variables.o pipe.o async.o sourcelist.o signalswindow.o \
			disassembler.o main.o breakpoints.o stacktrace.o attach.o breakpointswindow.o \
			freemem.o arexxport.o setbreak.o __unix_to_amiga_path_name.o
	$(CC) -gstabs main.o suspend.o symbols.o gui.o hexview.o stabs.o variables.o \
			disassembler.o breakpoints.o stacktrace.o attach.o async.o sourcelist.o \
			breakpointswindow.o freemem.o arexxport.o console.o source.o pipe.o \
			signalswindow.o \
			setbreak.o __unix_to_amiga_path_name.o   -lauto -o ../db101
