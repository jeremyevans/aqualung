/* -*- linux-c -*- */

/* Taken from
 * http://tlug.up.ac.za/wiki/index.php/Obtaining_a_stack_trace_in_C_upon_SIGSEGV
 * and tailored to Aqualung.
 */

/* $Id$ */

#include <config.h>

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#ifdef DEBUG_BUILD

#include <ucontext.h>
#include <dlfcn.h>
#include <execinfo.h>

#endif /* DEBUG_BUILD */

#include "segv.h"

#ifdef DEBUG_BUILD

#if defined(REG_RIP)
# define SIGSEGV_STACK_IA64
# define REGFORMAT "%016lx"
#elif defined(REG_EIP)
# define SIGSEGV_STACK_X86
# define REGFORMAT "%08x"
#else
# define SIGSEGV_STACK_GENERIC
# define REGFORMAT "%x"
#endif


static void
signal_segv(int signum, siginfo_t * info, void * ptr) {
  
	static const char *si_codes[3] = {"", "SEGV_MAPERR", "SEGV_ACCERR"};
	
	size_t i;
	ucontext_t *ucontext = (ucontext_t*)ptr;
	
#if defined(SIGSEGV_STACK_X86) || defined(SIGSEGV_STACK_IA64)
	int f = 0;
	Dl_info dlinfo;
	void **bp = 0;
	void *ip = 0;
#else
	void *bt[20];
	char **strings;
	size_t sz;
#endif
	
	fprintf(stderr, "===[ CRASH REPORT ]======\n");
	fprintf(stderr, "Please mail this to <aqualung-friends@lists.sourceforge.net>\n");
	fprintf(stderr, "along with a short description of what you were doing when\n");
	fprintf(stderr, "the program crashed. Thank you in advance!\n\n");
	fprintf(stderr, "  si_signo = %d (%s)\n", signum, strsignal(signum));
	fprintf(stderr, "  si_errno = %d\n", info->si_errno);
	fprintf(stderr, "  si_code  = %d (%s)\n", info->si_code, si_codes[info->si_code]);
	fprintf(stderr, "  si_addr  = %p\n\n", info->si_addr);

	for (i = 0; i < NGREG; i++) {
		fprintf(stderr, "  R[%02d] = 0x" REGFORMAT,
			i, ucontext->uc_mcontext.gregs[i]);
		if ((i % 4) == 3) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n\n");
	
#if defined(SIGSEGV_STACK_X86) || defined(SIGSEGV_STACK_IA64)
# if defined(SIGSEGV_STACK_IA64)
	ip = (void*)ucontext->uc_mcontext.gregs[REG_RIP];
	bp = (void**)ucontext->uc_mcontext.gregs[REG_RBP];
# elif defined(SIGSEGV_STACK_X86)
	ip = (void*)ucontext->uc_mcontext.gregs[REG_EIP];
	bp = (void**)ucontext->uc_mcontext.gregs[REG_EBP];
# endif
	
	while (bp && ip) {
		if (!dladdr(ip, &dlinfo))
			break;
		
		const char *symname = dlinfo.dli_sname;
		fprintf(stderr, "%3d: %12p <%s + %u> (%s)\n",
			++f,
			ip,
			symname,
			(unsigned)(ip - dlinfo.dli_saddr),
			dlinfo.dli_fname);
		
		if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main"))
			break;
		
		ip = bp[1];
		bp = (void**)bp[0];
	}
#else
	fprintf(stderr, "  backtrace():\n");
	sz = backtrace(bt, 20);
	strings = backtrace_symbols(bt, sz);
	
	for(i = 0; i < sz; ++i)
		fprintf(stderr, "%s\n", strings[i]);
#endif
	fprintf(stderr, "===[ END OF CRASH REPORT ]======\n");
	exit (-1);
}

#endif /* DEBUG_BUILD */

#ifdef RELEASE_BUILD
static void
signal_segv(int signum, siginfo_t * info, void * ptr) {

	fprintf(stderr, "Aqualung received signal %d (%s).\n\n",
		signum, strsignal(signum));
#ifdef _WIN32
	fprintf(stderr, "If you were running on UNIX instead of Windows,\n");
	fprintf(stderr, "you could help the developers by sending them the\n");
	fprintf(stderr, "crash report (stacktrace) you'd get instead of\n");
	fprintf(stderr, "this message...\n");
#else
	fprintf(stderr, "To help the developers fix the bug causing this crash,\n");
	fprintf(stderr, "please do the following:\n\n");
	fprintf(stderr, "1) configure & make Aqualung with --debug-enabled\n");
	fprintf(stderr, "2) reproduce the crash\n");
	fprintf(stderr, "3) send the crash report to the developers\n\n");
	fprintf(stderr, "Thank you for supporting Aqualung!\n");
#endif /* _WIN32 */
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

