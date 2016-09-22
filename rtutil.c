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
#include "info-bits.h"
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
  // printf("RTbig_malloc of %ld bytes returning pointer %p\n", bytes, p);
  return(p);
}

void out_of_memory(char *msg, int bytes_needed) {
  printf("out of memory %s %d\n", msg, bytes_needed);
  Debugger(0);
}

void Debugger(char *msg) {
  if (0 != msg) {
    printf(msg);
  } else {
    printf("Hey! rtgc called the debugger - fix me!\n");
  }
  fflush(stdout);
  raise(SIGSTOP);
}

void copy_test(size_t len) {
  struct timeval start_tv, end_tv;
  char *src = malloc(len);
  char *dest = malloc(len);
  
  gettimeofday(&start_tv, 0);
  memcpy(dest, src, len);
  gettimeofday(&end_tv, 0);
  printf("start: %d sec, %d usec\n", start_tv.tv_sec, start_tv.tv_usec);
  printf("end: %d sec, %d usec\n", end_tv.tv_sec, end_tv.tv_usec);
  printf("elapsed: %d\n", end_tv.tv_usec - start_tv.tv_usec);
  free(src);
  free(dest);
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

// useful with clock_gettime
struct timespec RTtime_diff(struct timespec start, struct timespec end) {
  struct timespec diff;
  if ((end.tv_nsec - start.tv_nsec) < 0) {
    diff.tv_sec = end.tv_sec - start.tv_sec - 1;
    diff.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
  } else {
    diff.tv_sec = end.tv_sec - start.tv_sec;
    diff.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return(diff);
}

int RTtime_cmp(struct timespec a, struct timespec b) {
  if (a.tv_sec == b.tv_sec) {
    return(a.tv_nsec > b.tv_nsec);
  } else {
    return(a.tv_sec > b.tv_sec);
  }
}
    

static
int verify_white_count(GPTR group) {
  GCPTR ptr = group->white;
  int count = 0;
  while (ptr != NULL) {
    if (group->size < BYTES_PER_PAGE) {
      if ((((long) (GET_LINK_POINTER(ptr->prev)) % group->size) != 0) ||
	  (((long) (GET_LINK_POINTER(ptr->next)) % group->size) != 0)) {
	Debugger("Bad gchdr\n");
      }
    }
    ptr = GET_LINK_POINTER(ptr->next);
    count = count + 1;
  }
  if (group->white_count != count) {
    Debugger("incorrect white_count\n");
  }
}

static
void verify_white_counts() {
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    GPTR group = &groups[i];
    verify_white_count(group);
  }
}

#define FINALIZE_RING_SIZE (1 << 16)
#define FINALIZE_RING_MASK (FINALIZE_RING_SIZE - 1)

unsigned long finalize_head;
unsigned long finalize_tail;
void *finalize_ring[FINALIZE_RING_SIZE];
pthread_mutex_t finalize_lock;
pthread_cond_t finalize_cond_empty;
pthread_cond_t finalize_cond_full;

void finalize_add(void *obj) {
  pthread_mutex_lock(&finalize_lock);
  while ((finalize_tail + FINALIZE_RING_SIZE) > finalize_head) {
    pthread_cond_wait(&finalize_cond_full, &finalize_lock);
  }
  finalize_ring[finalize_head & FINALIZE_RING_MASK] = obj;
  finalize_head = finalize_head + 1;
  pthread_cond_signal(&finalize_cond_empty);
  pthread_mutex_unlock(&finalize_lock);
}

void *finalize_remove() {
  pthread_mutex_lock(&finalize_lock);
  while (finalize_tail == finalize_head) {
    pthread_cond_wait(&finalize_cond_empty, &finalize_lock);
  }
  void *object = finalize_ring[finalize_tail & FINALIZE_RING_MASK];
  finalize_tail = finalize_tail + 1;
  pthread_cond_signal(&finalize_cond_full);
  pthread_mutex_unlock(&finalize_lock);
}

void finalize_init() {
  pthread_mutex_init(&finalize_lock, NULL);
  pthread_cond_init(&finalize_cond_empty, NULL);
  pthread_cond_init(&finalize_cond_full, NULL);
  finalize_head = 0;
  finalize_tail = 0;
}

void timespec_test () {
  struct timespec res, start_time, end_time, elapsed;
  if (0 == clock_getres(CLOCK_REALTIME, &res)) {
    clock_gettime(CLOCK_REALTIME, &start_time);
    clock_gettime(CLOCK_REALTIME, &end_time);
    elapsed = RTtime_diff(start_time, end_time);
    printf("Elapsed: %lld.%.9ld\n", elapsed.tv_sec, elapsed.tv_nsec);
  }
}

 

