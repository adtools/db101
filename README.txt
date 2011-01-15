===== LICENCE ========

This software is released under the GNU PUBLIC LICENSE (see LICENCE.txt).

===== ABOUT ==========

DEbug 101 is a source debugger for amiga os 4.1. It has
a lot of the features found in other debuggers like GDB,
but comes with a nice reaction gui.

==== REQUIREMENTS ====

Needs at least elf.library 53.13 (currently beta) to work.

==== KNOWN ISSUES ====

- There is a bug in the os, that prevents setting
the msr register. Therefore any attempt to set
a breakpoint on top of a br instruction will
have unpredictable results. Be warned!

- There is a bug in the gnu linker (dl), that
throws away a lot of the debug info when compiling
multiple files into one executable. What this means
is, that when working on such a project, it is not
possible to properly read the typedef info, which
means, that your local and global variables will
show up as "UNKNOWN". There is currently no fix
for this, sorry!

- There is no system friendly way to get an
exception context out of the debug hook. What this
means is, that when you attach to a process, it is
not possible to straight away get registers, stack,
variables etc. before you have manually set up a breakpoint
and actually hit it. This of course limits the
usability of the attach function a lot, but there
is no system friendly way to work around it. Sorry!

- Nested functions is currently not supported.

- Stacktracing is not really functional yet.

==== DONATE ==========

If you like this, feel free to make a donation via Paypal:

alfkil@gmail.com

==== COMMENTS =====

If you have any problems or comments, please write to: alfkil@gmail.com




history:

v. 0.9: added Arexx port and console
v. 0.8: added attach mode
v. 0.7: added globals and correct reading of variable types
