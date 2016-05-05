// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include "mem-config.h"
#include "infoBits.h"
#include "mem-internals.h"
#include "allocate.h"

// grows *DOWN*, not up
void *RTbig_malloc(size_t bytes) {
  BPTR p = (mmap(0,
		 bytes,
		 PROT_EXEC | PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS,
		 0,
		 0));
  printf("RTbig_malloc of %ld bytes returning pointer %p\n", bytes, p);
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

static size_t len = 8000;
static char *src;
static char *dest;

void copy_test() {
  struct timeval start_tv, end_tv;
  src = malloc(len);
  dest = malloc(len);
  
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
  // read c->count again in case bug made it change
  int retval = c->count;       
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
/*
timespec diff(timespec start, timespec end)
{
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}
*/
