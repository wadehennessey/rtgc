// (C) Copyright 2015 by Wade L. Hennessey. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include "compat.h"
#include "mem-config.h"
#include "infoBits.h"
#include "mem-internals.h"
#include "allocate.h"


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
  Debugger(0);
}

static int zero = 0;
static int debug;

void Debugger(char *msg) {
  if (0 != msg) {
    printf(msg);
  } else {
    printf("Hey! rtgc called the debugger - fix me!\n");
  }
  fflush(stdout);
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

void counter_init(COUNTER *c) {
  c->count = 0;
  pthread_mutex_init(&(c->lock), NULL);
  pthread_cond_init(&(c->cond), NULL);
}

int counter_zero(COUNTER *c) {
  pthread_mutex_lock(&(c->lock));
  c->count = 0;
  int err = pthread_cond_broadcast(&(c->cond));
  if (0 != err)  {
    printf("pthread_cond_broadcast failed with %d!\n", err);
  }
  int val = c->count;
  //printf("counter zero\n");
  pthread_mutex_unlock(&(c->lock));
  return(val);
}
 
int counter_increment(COUNTER *c) {
  pthread_mutex_lock(&(c->lock));
  // why can't we just say c->count = c->count + 1 ?
  int val = c->count;
  val = val + 1;
  c->count = val;
  int err = pthread_cond_broadcast(&(c->cond));
  if (0 != err)  {
    printf("pthread_cond_broadcast failed with %d!\n", err);
  }
  //printf("counter increment\n");
  fflush(stdout);
  pthread_mutex_unlock(&(c->lock));
  return(val);
}

void counter_wait_threshold(COUNTER *c, int threshold) {
  pthread_mutex_lock(&(c->lock));
  while (c->count < threshold) {
    pthread_cond_wait(&(c->cond), &(c->lock));
  }
  // now c->count >= threshold
  pthread_mutex_unlock(&(c->lock));
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
