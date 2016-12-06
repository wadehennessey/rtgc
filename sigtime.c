// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <error.h>
#include "info-bits.h"
#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"

// sig_atomic_t is a long?
static volatile long flag = 0;
static volatile long counter = 0;

void handler_func(int signum, siginfo_t *siginfo, void *context) {
  counter = counter + 1;
}

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

void *busy_loop(void *arg) {
  const int count = 6005;
  long buffer[count];
  long overflow = 0;
  struct timespec res, prev, next;
  if (0 == clock_getres(CLOCK_REALTIME, &res)) {
    //printf("clock resolution is %ld ns\n", res.tv_nsec);
  }
  // warm up buffer cache if needed
  memset(buffer, 0, count * sizeof(long));

  while(0 == flag);		// spin waiting for sender to start
  clock_gettime(CLOCK_REALTIME, &prev);
  for (int i = 0; i < count; i++) {
    clock_gettime(CLOCK_REALTIME, &next);
    struct timespec diff = RTtime_diff(prev, next);
    prev = next;
    if (i < count) {
      buffer[i] = diff.tv_nsec;
    } else {
      overflow = overflow + 1;
    }
  }
  for (int i = 5; i < count; i++) {
    printf("%ld,\n", buffer[i]);
  }
  //printf("overflow of %ld\n", overflow);
}

void send_loop(pthread_t thread) {
  int err;
  flag = 1;
  for (int i = 0; i < 50; i++) {
    usleep(1);
    if (0 != (err = pthread_kill(thread, FLIP_SIGNAL))) {
      error(0, err, "pthread_kill failed");
    }
  }
  fprintf(stderr, "exiting send loop, counter is %ld\n", counter);
}

void init_signals() {
  struct sigaction signal_action;
  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, FLIP_SIGNAL);
  // sigprocmask seems to do the same thing as this
  if (0 != pthread_sigmask(SIG_UNBLOCK, 0, &set)) {
    printf("mask failed!");
  }
  
  memset(&signal_action, 0, sizeof(signal_action));
  signal_action.sa_sigaction = handler_func;
  signal_action.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(FLIP_SIGNAL, &signal_action, 0);
}

int main(int argc, char *argv[]) {
  //busy_loop(0);
  //exit(1);
  
  pthread_t thread;
  init_signals();
  pthread_create(&thread, NULL, &busy_loop, 0);
  send_loop(thread);
}
