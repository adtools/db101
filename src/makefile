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

helloworld: helloworld.c
	$(CC) -gstabs helloworld.c -o helloworld
	$(CC) -gstabs -S helloworld.c

suspend.o: suspend.c suspend.h symbols.h gui.h breakpoints.h
	$(CC) -c -gstabs -o suspend.o suspend.c

symbols.o: symbols.c symbols.h suspend.h stabs.h
	$(CC) -c -gstabs -o symbols.o symbols.c

gui.o: gui.c gui.h suspend.h stabs.h symbols.h variables.h disassembler.h breakpoints.h stacktrace.h
	$(CC) -c -gstabs -o gui.o gui.c

hexview.o: hexview.c hexview.h suspend.h symbols.h
	$(CC) -c -gstabs -o hexview.o hexview.c

main.o: main.c suspend.h gui.h breakpoints.h
	$(CC) -c -gstabs -o main.o main.c

stabs.o: stabs.c stabs.h suspend.h
	$(CC) -c -gstabs -o stabs.o stabs.c

variables.o: variables.c variables.h suspend.h stabs.h gui.h
	$(CC) -c -gstabs -o variables.o variables.c

globals.o: globals.c globals.h suspend.h stabs.h gui.h variables.h
	$(CC) -c -gstabs -o globals.o globals.c

registers.o: registers.c registers.h suspend.h symbols.h
	$(CC) -c -gstabs -o registers.o registers.c

disassembler.o: disassembler.c disassembler.h suspend.h stabs.h gui.h
	$(CC) -c -gstabs -o disassembler.o disassembler.c

breakpoints.o: breakpoints.c breakpoints.h suspend.h gui.h stabs.h
	$(CC) -c -gstabs -o breakpoints.o breakpoints.c

stacktrace.o: stacktrace.c stacktrace.h suspend.h stabs.h gui.h
	$(CC) -c -gstabs -o stacktrace.o stacktrace.c

attach.o: attach.c attach.h
	$(CC) -c -gstabs -o attach.o attach.c

breakpointswindow.o: breakpointswindow.c breakpointswindow.h stabs.h suspend.h
	$(CC) -c -gstabs -o breakpointswindow.o breakpointswindow.c

../db101: suspend.o symbols.o gui.o hexview.o \
			stabs.o variables.o registers.o \
			disassembler.o main.o breakpoints.o stacktrace.o globals.o attach.o breakpointswindow.o
	$(CC) -gstabs main.o suspend.o symbols.o gui.o hexview.o stabs.o variables.o globals.o \
			registers.o disassembler.o breakpoints.o stacktrace.o attach.o breakpointswindow.o -lauto -o ../db101
