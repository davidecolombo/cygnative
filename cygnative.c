/*-
 * Copyright (c) 2009 Frank Behrens <frank@pinky.sax.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $HeadURL: svn+ssh://moon.behrens/var/repos/sources/cygwinNativeWrapper/cygnative.c $
 * $LastChangedDate: 2009-08-18 08:21:46 +0200 (Di, 18 Aug 2009) $
 * $LastChangedRevision: 157 $
 * $LastChangedBy: frank $
 */

#define PTHREAD 1

#include <windows.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <io.h>
#include <errno.h>
#if PTHREAD
#include <pthread.h>
#include <signal.h>
#endif

#ifndef __unused
#define __unused        __attribute__((__unused__))
#endif

#define BUFSIZE			(100*1024)
#define MAXCMDLINESIZE	(32*1024)


static HANDLE client_r;
static HANDLE client_w;
static DWORD retcode;
static volatile int doRun = TRUE;
static char cmdline[MAXCMDLINESIZE];


static void 
checkerrno (const char *message, const char *message2) {

	LPVOID lpMsgBuf;
	DWORD errnum;

	errnum = GetLastError();
	if (errnum == 0L)
		return;
	if (message2 == NULL)
		message2 = "";
	FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			errnum,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL
	);
	fprintf (stderr, "ERROR %s %s: %s (%ld)\n", message, message2, (char*) lpMsgBuf, (long)errnum);
	// Free the buffer.
	LocalFree(lpMsgBuf );
	exit (10);
}


#if PTHREAD
static void*
clientReaderThread(void* lpParameter __unused) {
#else
static DWORD WINAPI 
clientReaderThread(LPVOID lpParameter __unused) {
#endif

	static char buf[BUFSIZE];

#if DEBUG
	fprintf(stderr, "start child->parent write loop\n");
#endif
	while (doRun) {
		DWORD bytes;
		boolean fSuccess = ReadFile(client_r, buf, sizeof(buf), &bytes, NULL); 
	    if (!fSuccess || bytes <= 0 || GetLastError() == ERROR_HANDLE_EOF) 
			break;
		// with cygwin we may get unexpected EAGAIN errors on write call, thanks to Ilya Basin for research
		ssize_t wbytes;
		int maxRepeat = 100;
		int errNo = 0;
		do {
			wbytes = write(STDOUT_FILENO, buf, bytes);
			if (wbytes == -1) {
				errNo = errno;
				usleep(50);	// give parent process time to process data
			}
		} while (wbytes == -1 && errNo == EAGAIN && --maxRepeat > 0);

		if (wbytes != (ssize_t)bytes) {
			perror("could not write to parent");
			break;
		}
	}
	CloseHandle(client_r);
	close(STDOUT_FILENO);
#if DEBUG
	fprintf(stderr, "handle child->parent closed\n");
#endif
#if PTHREAD
	pthread_exit(NULL);
	return NULL;
#else
	return 0;
#endif
}

#if PTHREAD
static void*
clientWriterThread(void* lpParameter __unused) {
#else
static DWORD WINAPI 
clientWriterThread(LPVOID lpParameter __unused) {
#endif

	static char buf[BUFSIZE];
	
#if DEBUG
	fprintf(stderr, "start parent->child write loop\n");
#endif
	while (doRun) {
		ssize_t rbytes = read(STDIN_FILENO, buf, sizeof(buf));
		if (rbytes <= 0)
			break;
		
		DWORD bytes;
		boolean fSuccess = WriteFile(client_w, buf, rbytes, &bytes, NULL); 
	    if (!fSuccess || (ssize_t)bytes != rbytes) 
			break;
		
	}
	CloseHandle(client_w);
	close(STDIN_FILENO);
#if DEBUG
	fprintf(stderr, "handle parent->child closed\n");
#endif
#if PTHREAD
	pthread_exit(NULL);
	return NULL;
#else
	return 0;
#endif
}


#if PTHREAD
static void*
exitWatchDog(void* lpParameter __unused) {
#else
static DWORD WINAPI 
exitWatchDog(LPVOID lpParameter __unused) {
#endif


	HANDLE self = OpenProcess(PROCESS_TERMINATE, FALSE, GetCurrentProcessId());
	if (self == NULL)
		checkerrno("OpenProcess", "");

	Sleep(10000);
	
#if DEBUG
	fprintf(stderr, "emergency exit for wrapper with state %ld\n", (long)retcode);
#endif
	if (!TerminateProcess(self, retcode))
		checkerrno("TerminateProcess", "");


#if PTHREAD
	pthread_exit(NULL);
	return NULL;
#else
	return 0;
#endif
}


static void
usage(char *progname) {
	fprintf(stderr, "%s - a programm to use stdin/stdout redirection \non native win32 programms called from cygwin\n", progname);
	fprintf(stderr, "(C) Copyright 2009, Frank Behrens, Version %s\n", VERSION);
	fprintf(stderr, "Usage: %s <prog> [args...]\n", progname);
	exit(1);
}


int 
main(int argc, char *argv[]) {
	if (argc == 1)
		usage(*argv);
	argv++;
#if DEBUG
	fprintf(stderr, "native wrapper started for %s\n", *argv);
#endif

	setmode(STDIN_FILENO, O_BINARY);
	setmode(STDOUT_FILENO, O_BINARY);

	HANDLE newstdin;
	HANDLE newstdout;
	HANDLE newstderr;
    PROCESS_INFORMATION procinfo; 
    STARTUPINFO startinfo;

	SECURITY_ATTRIBUTES saAttr; 
	// Set the bInheritHandle flag so pipe handles are inherited. 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

	if (!CreatePipe(&client_r, &newstdout, &saAttr, 0))
		checkerrno("CreatePipe", "");
	SetHandleInformation(client_r, HANDLE_FLAG_INHERIT, 0);
	if (!CreatePipe(&newstdin, &client_w, &saAttr, 0))
		checkerrno("CreatePipe", "");
	SetHandleInformation(client_w, HANDLE_FLAG_INHERIT, 0);
	newstderr = GetStdHandle(STD_ERROR_HANDLE);		// no redirect

	ZeroMemory(&procinfo, sizeof(procinfo));
	ZeroMemory(&startinfo, sizeof(startinfo));

	cmdline[0] = '\0';
	char** c = argv;

	strlcat(cmdline, "\"", sizeof(cmdline));
	strlcat(cmdline, *(c++), sizeof(cmdline));
	strlcat(cmdline, "\" ", sizeof(cmdline));
	
	while (*c != NULL) {
		strlcat(cmdline, *c, sizeof(cmdline));
		strlcat(cmdline, " ", sizeof(cmdline));
		c++;
	}

	startinfo.cb = sizeof(startinfo);
	startinfo.hStdInput = newstdin;
    startinfo.hStdOutput = newstdout;
    startinfo.hStdError = newstderr;
	startinfo.dwFlags |= STARTF_USESTDHANDLES;

	BOOL res = CreateProcessA(NULL, cmdline, 
				NULL /*processAttrs*/, 
				NULL /*threadAttrs*/,
				TRUE /*inheritHandles*/,
				0 /*creationFlags*/,
				NULL /*environment*/,
				NULL /*currentDirectory*/,
				&startinfo,
				&procinfo);
	if (!res)
		checkerrno("CreateProcess", *argv);

	CloseHandle(procinfo.hThread);

#if PTHREAD
#if DEBUG
	fprintf(stderr, "client process started with pid %ld, now starting posix worker threads\n", (long)procinfo.dwProcessId);
#endif
	pthread_t read_id, write_id;
	pthread_create(&read_id, NULL, &clientReaderThread, NULL);
	pthread_create(&write_id, NULL, &clientWriterThread, NULL);
#else
#if DEBUG
	fprintf(stderr, "client process started with pid %ld, now starting win32 worker threads\n", (long)procinfo.dwProcessId);
#endif
	DWORD read_id, write_id;
	CreateThread(NULL, 0, &clientReaderThread, NULL, 0, &read_id);
	CreateThread(NULL, 0, &clientWriterThread, NULL, 0, &write_id);
#endif

	WaitForSingleObject(procinfo.hProcess, INFINITE);
	doRun = FALSE;
	GetExitCodeProcess(procinfo.hProcess, &retcode);
	CloseHandle(procinfo.hProcess);

#if DEBUG
	fprintf(stderr, "client returned with code %ld\n", (long)retcode);
#endif
#if 0
	pthread_kill(read_id, SIGINT);
	pthread_kill(write_id, SIGINT);
	pthread_join(read_id, NULL);
	pthread_join(write_id, NULL);
#endif

#if PTHREAD
	pthread_t exit_id;
	pthread_create(&exit_id, NULL, &exitWatchDog, NULL);
#else
	DWORD exit_id;
	CreateThread(NULL, 0, &exitWatchDog, NULL, 0, &exit_id);
#endif

#if DEBUG
	fprintf(stderr, "native wrapper returned with error code %ld\n", (long)retcode);
#endif
	exit(retcode);
}
