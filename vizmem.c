
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "compat.h"
#include "mem-config.h"
#include "infoBits.h"
#include "mem-internals.h"
#include "allocate.h"
#include <sys/mman.h>

extern BPTR main_stack_base;
extern BPTR main_stack_top;

// We don't want to scan main/gc stack at all
// want to scan all thread stacks, signal handler will determine stack base on top at flip time
BPTR SXgetStackBase(SXobject thread) {
  return(main_stack_base);
}

BPTR SXgetStackTop(SXobject thread) {
  return(main_stack_top);
}

// Use getcontext() to get access to the registers?
// /home/wade/glibc-2.22/sysdeps/unix/sysv/linux/x86_64/getcontext.S
BPTR SXthread_registers(SXobject thread, int *num_registers) {
  *num_registers = 0;
  return(0);
}

// grows *DOWN*, not up
void *SXbig_malloc(int bytes) {
  BPTR p = (mmap(0,
		 bytes,
		 PROT_READ | PROT_WRITE, /* leave out PROT_EXEC */
		 MAP_PRIVATE | MAP_ANONYMOUS,
		 0,
		 0));
  printf("big_malloc returning pointer %p\n", p);
  return(p);
}

void out_of_memory(char *msg, int bytes_needed) {
  printf("out of memory %s %d\n", msg, bytes_needed);
  Debugger();
}

void SXmaybe_update_visual_page(int page_number, int old_bytes_used,
				int new_bytes_used) {
}

int SXupdate_visual_page(int page_index) {
}

void SXupdate_visual_static_page(int page_number) {
}
void SXupdate_visual_fake_ptr_page(int page_index) {
}
void SXdraw_visual_gc_state(void) {
}
void SXdraw_visual_gc_stats(void) {
}
void SXvisual_runbar_on(void) {
}
void SXvisual_runbar_off(void) {
}

static int zero = 1;
static int debug;

void Debugger() {
  printf("Hey! rtgc called the debugger - fix me!\n");
  debug = 1 / zero;
}
