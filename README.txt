===== LICENCE ========

This software is released under the GNU PUBLIC LICENSE (see LICENCE.txt).

===== ABOUT ==========

DEbug 101 is a source debugger for amiga os 4.1. It has
a lot of the features found in other debuggers like GDB,
but comes with a nice reaction gui.

==== REQUIREMENTS ====

Needs at least elf.library 53.13 to work

==== KNOWN ISSUES ====

There is a bug in the os, that prevents setting
the msr register. Therefore any attempt to set
a breakpoint on top of a br instruction will
have unpredictable results. Be warned!

Nested functions is currently not supported.

Stacktracing is not really functional yet.

==== DONATE ==========

If you like this, feel free to make a donation via Paypal:

alfkil@gmail.com

==== COMMENTS =====

If you have any problems or comments, please write to: alfkil@gmail.com




history:

v. 0.7: added globals and correct reading of variable types
