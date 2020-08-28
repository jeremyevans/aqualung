/* -*- linux-c -*- */

/* Taken from
 * http://tlug.up.ac.za/wiki/index.php/Obtaining_a_stack_trace_in_C_upon_SIGSEGV
 * and tailored to Aqualung.
 */

/* $Id$ */

#include <config.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef DEBUG_BUILD

#include <execinfo.h>

#include "version.h"

#endif /* DEBUG_BUILD */

#include "segv.h"


#ifdef DEBUG_BUILD
static void
signal_segv(int signum, siginfo_t * info, void * ptr) {

	unsigned i;
	void *bt[20];
	char **strings;
	size_t sz;

	fprintf(stderr, "===[ Aqualung %s CRASH REPORT ]===\n", AQUALUNG_VERSION);
#ifdef HAVE_PSIGINFO
	psiginfo(info, "Caught");
#else
	psignal(signum, "Caught");
#endif
	fprintf(stderr, "If you would like to report this as a bug, please file an issue\n");
	fprintf(stderr, "at https://github.com/jeremyevans/aqualung/issues/new\n");
	fprintf(stderr, "with a short description of what you were doing when\n");
	fprintf(stderr, "the program crashed. Please also send the output of `aqualung -v'.\n");
	fprintf(stderr, "Thank you in advance!\n");

	sz = backtrace(bt, 20);
	strings = backtrace_symbols(bt, sz);

	fprintf(stderr, "\nbacktrace (%zd stack frames)\n", sz);
	for(i = 0; i < sz; ++i)
		fprintf(stderr, "%s\n", strings[i]);
	fprintf(stderr, "===[ END OF CRASH REPORT ]===\n");

	free(strings);
	exit (-1);
}
#endif /* DEBUG_BUILD */

#ifdef RELEASE_BUILD
static void
signal_segv(int signum, siginfo_t * info, void * ptr) {

	fprintf(stderr, "Aqualung received signal %d (%s).\n\n",
		signum, strsignal(signum));
#ifndef _WIN32
	fprintf(stderr, "To help the developers fix the bug causing this crash,\n");
	fprintf(stderr, "please do the following:\n\n");
	fprintf(stderr, "1) configure & make Aqualung with --enable-debug\n");
	fprintf(stderr, "2) reproduce the crash\n");
	fprintf(stderr, "3) send the crash report to the developers\n\n");
	fprintf(stderr, "Thank you for supporting Aqualung!\n");
#endif /* !_WIN32 */
	exit(-1);
}
#endif /* RELEASE_BUILD */

int
setup_sigsegv() {
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = signal_segv;
	action.sa_flags = SA_SIGINFO;

	if (sigaction(SIGFPE, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}
	if (sigaction(SIGILL, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}
	if (sigaction(SIGSEGV, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}
	if (sigaction(SIGBUS, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}
	if (sigaction(SIGABRT, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}
	return 1;
}

#ifndef SIGSEGV_NO_AUTO_INIT
static void
__attribute((constructor))
init(void) {
	setup_sigsegv();
}
#endif

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

