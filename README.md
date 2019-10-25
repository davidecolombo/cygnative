# cygnative

Copyright (c) 2009 Frank Behrens <frank@pinky.sax.de>

All rights reserved.

$Id: README 124 2009-03-12 20:57:46Z frank $


Cygnative.exe is a wrapper for cygwin programs which call native Win32
programs with stdin and/or stdout handle redirected. Without cygnative.exe it 
is possible that the native program can not read from the handle and receives a
"invalid handle" error.


See also http://diario.beerensalat.info/tags/plink/

Fixed ```cygnative.c:207:69: error: ‘VERSION’ undeclared``` on latest 1.2 sources from http://diario.beerensalat.info/2009/08/18/new_cygnative_version_1_2_for_rsync_plink.html

Compile with: ```gcc cygnative.c -o cygnative.exe```


Usage: ```cygnative <nativeProgram> [args...]```

Example with rsync and plink: ```rsync --blocking-io -e "cygnative plink -batch" -h -a --delete / user@server:/data2/backup/```
