/* -*- linux-c -*- */

/* Taken from
 * http://tlug.up.ac.za/wiki/index.php/Obtaining_a_stack_trace_in_C_upon_SIGSEGV
 * and tailored to Aqualung.
 */

/* $Id$ */

#ifndef AQUALUNG_SEGV_H
#define AQUALUNG_SEGV_H

#include <config.h>


int setup_sigsegv();


#endif /* AQUALUNG_SEGV_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

