# cygnative
Cygnative.exe is a wrapper for cygwin programs which call native Win32 programs with stdin and/or stdout handle redirected. Without cygnative.exe it  is possible that the native program can not read from the handle and receives a "invalid handle" error.
