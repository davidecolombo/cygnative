# cygnative

This is a wrapper program written in C by Frank Behrens, which basically solves handle problems using `plink` (a command line connection tool for Windows included with [Putty](https://www.putty.org/)) along with [cygwin](https://www.cygwin.com/) and [rsync](https://linux.die.net/man/1/rsync). I found this program on the internet and I implemented a simple fix so I can compile it with cygwin’s [GCC](https://github.com/davidecolombo/cygnative).

Cygnative.exe is a wrapper for cygwin programs which call native Win32 programs with stdin and/or stdout handle redirected. Without cygnative it’s possible that the native program cannot read from the handle and receives a “invalid handle” error. See: http://diario.beerensalat.info/tags/plink/

Fixed ```cygnative.c:207:69: error: ‘VERSION’ undeclared``` on latest 1.2 sources from http://diario.beerensalat.info/2009/08/18/new_cygnative_version_1_2_for_rsync_plink.html

Compile with: `gcc cygnative.c -o cygnative.exe`

Usage: `cygnative <nativeProgram> [args...]`

Example with rsync and plink: `rsync --blocking-io -e "cygnative plink -batch" -h -a --delete / user@server:/data2/backup/`
