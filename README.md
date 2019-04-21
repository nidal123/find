# find
Unix-like operating system command

nfind is a utility for macOS (nfind_macOS.c) or Linux (nfind_linux.c). nfind is intended to fully replicate the much of the functionality of the find(1) linux (read about it here tool http://man7.org/linux/man-pages/man1/find.1.html).

nfind implements the -P, -L, -o, -a, -depth, -exec, -maxdepth, -name, -newer, and -print flags described in the documentation of find (link at the end of the paragraph directly above this one).

to compile on macOS open "terminal" and cd to directory with Makefile and nfind_macOS.c and type "make"

to compile on linux do the same thing but change "nfind_macOS.c" in the "Makefile" to "nfind_linux.c" before typing "make" and pressing enter

To see nfind traverse the directories starting in your present working directory type "./nfind" then press enter. Otherwise ./nfind's functionality should mirror that of find(1). 
