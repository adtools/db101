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

all: helloworld db101

helloworld: helloworld.c
	$(CC) -gstabs helloworld.c -o helloworld
	$(CC) -gstabs -S helloworld.c

db101: suspend.c symbols.c gui.c hexview.c suspend.h symbols.h gui.h hexview.h \
			stabs.c stabs.h variables.h variables.c registers.h registers.c \
			disassembler.c main.c breakpoints.h breakpoints.c stacktrace.h stacktrace.c \
			globals.h globals.c
	$(CC) -gstabs main.c suspend.c symbols.c gui.c hexview.c stabs.c variables.c globals.c \
			registers.c disassembler.c breakpoints.c stacktrace.c -lauto -o db101
