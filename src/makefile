all: helloworld ../db101

helloworld: helloworld.c
	gcc -gstabs helloworld.c -o helloworld
	gcc -gstabs -S helloworld.c

../db101: suspend.c symbols.c gui.c hexview.c suspend.h symbols.h gui.h hexview.h \
			stabs.c stabs.h variables.h variables.c registers.h registers.c \
			disassembler.c main.c breakpoints.h breakpoints.c stacktrace.h stacktrace.c \
			globals.h globals.c
	gcc -gstabs main.c suspend.c symbols.c gui.c hexview.c stabs.c variables.c globals.c \
			registers.c disassembler.c breakpoints.c stacktrace.c -lauto -o ../db101
