/*
 * Copyright 2017 Wade Lawrence Hennessey
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// As root:
// cpupower frequency-set -g performance
// cpupower frequency-info

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <error.h>

static volatile long flag = 0;
static volatile long counter = 0;

void handler_func(int signum, siginfo_t *siginfo, void *context) {
  counter = counter + 1;
}

struct timespec RTtime_diff(struct timespec end, struct timespec start) {
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

void *us_busy_loop(void *arg) {
  const int count = 6005;
  struct timeval buffer[count];
  memset(buffer, 0, count * sizeof(struct timeval));

  int err;
  while(0 == flag);		// spin waiting for sender to start
  for (int i = 0; i < count; i++) {
    if (0 != (err = gettimeofday(buffer + i, 0))) {
      error(0, err, "gettimeofday failed");
    }
  }
  
  for (int i = 5; i < (count - 1); i++) {
    struct timeval diff;
    timersub(buffer + i + 1, buffer + i, &diff);
    printf("%ld,\n", diff.tv_usec);
  }
  flag = 0;
}

void *ns_busy_loop(void *arg) {
  const int count = 6005;
  struct timespec buffer[count];
  struct timespec res;
  if (0 == clock_getres(CLOCK_THREAD_CPUTIME_ID, &res)) {
    fprintf(stderr, "clock resolution is %ld ns\n", res.tv_nsec);
  }
  memset(buffer, 0, count * sizeof(struct timespec));

  int err;
  while(0 == flag);		// spin waiting for sender to start
  for (int i = 0; i < count; i++) {
    // Tried CLOCK_THREAD_CPUTIME_ID and CLOCK_REALTIME
    if (0 != (err = clock_gettime(CLOCK_REALTIME, buffer + i))) {
      error(0, err, "clock_gettime failed");
    }
  }
  
  for (int i = 0; i < (count - 1); i++) {
    struct timespec diff = RTtime_diff(buffer[i + 1], buffer[i]);
    printf("%ld,\n", diff.tv_nsec);
  }
  flag = 0;
}

void send_loop(pthread_t thread) {
  int err;
  counter = 0;
  flag = 1;
  for (int i = 0; i < 50; i++) {
    //for (int t = 0; t < 5000; t++);

    long next_count = counter + 1;
    if (0 != (err = pthread_kill(thread, SIGUSR1))) {
      error(0, err, "pthread_kill failed");
    }
    while (counter < next_count);

  }
  void **retval;
  //pthread_join(thread, retval);
  fprintf(stderr, "exiting send loop, counter is %ld\n", counter);
}

void init_signals() {
  struct sigaction signal_action;
  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  // sigprocmask seems to do the same thing as this
  if (0 != pthread_sigmask(SIG_UNBLOCK, 0, &set)) {
    printf("mask failed!");
  }
  
  memset(&signal_action, 0, sizeof(signal_action));
  signal_action.sa_sigaction = handler_func;
  signal_action.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGUSR1, &signal_action, 0);
}

int main(int argc, char *argv[]) {
  pthread_t thread;
  init_signals();
  pthread_create(&thread, NULL, &ns_busy_loop, 0);
  send_loop(thread);
}
