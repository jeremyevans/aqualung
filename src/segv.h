/* -*- linux-c -*- */

/* Taken from
 * http://tlug.up.ac.za/wiki/index.php/Obtaining_a_stack_trace_in_C_upon_SIGSEGV
 * and tailored to Aqualung.
 */

/* $Id$ */

#ifndef _SEGV_H
#define _SEGV_H

#include <config.h>

#ifdef __cplusplus
extern "C" {
#endif


int setup_sigsegv();


#ifdef __cplusplus
}
#endif

#endif /* _SEGV_H */
