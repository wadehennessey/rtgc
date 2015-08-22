// (C) Copyright 2015 by Wade L. Hennessey. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "compat.h"
#include "mem-config.h"
#include "infoBits.h"
#include <signal.h>
#include "mem-internals.h"
#include "allocate.h"
#include <sys/mman.h>
#include <sys/time.h>

// grows *DOWN*, not up
void *SXbig_malloc(int bytes) {
  BPTR p = (mmap(0,
		 bytes,
		 PROT_READ | PROT_WRITE, /* leave out PROT_EXEC */
		 MAP_PRIVATE | MAP_ANONYMOUS,
		 0,
		 0));
  printf("big_malloc of %d bytes returning pointer %p\n", bytes, p);
  return(p);
}

void out_of_memory(char *msg, int bytes_needed) {
  printf("out of memory %s %d\n", msg, bytes_needed);
  Debugger();
}

static int zero = 1;
static int debug;

void Debugger() {
  printf("Hey! rtgc called the debugger - fix me!\n");
  debug = 1 / zero;
}


void copy_test() {
  struct timeval start_tv, end_tv;
  int len = 1000000;
  //char src[len], dest[len];
  char *src = malloc(len);
  char *dest = malloc(len);

  gettimeofday(&start_tv, 0);
  memcpy(dest, src, len);
  gettimeofday(&end_tv, 0);
  printf("start: %d sec, %d usec\n", start_tv.tv_sec, start_tv.tv_usec);
  printf("end: %d sec, %d usec\n", end_tv.tv_sec, end_tv.tv_usec);
  printf("elapsed: %d\n", end_tv.tv_usec - start_tv.tv_usec);
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