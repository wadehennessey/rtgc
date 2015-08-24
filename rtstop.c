// (C) Copyright 2015 by Wade L. Hennessey. All rights reserved.

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include "compat.h"
#include "infoBits.h"
#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"

// see /usr/include/sys/ucontext.h for more details
void print_registers(gregset_t *gregs) {
  printf("REG_R8 %llx\n", (*gregs)[REG_R8]);
  printf("REG_R9 %llx\n", (*gregs)[REG_R9]);
  printf("REG_R10 %llx\n", (*gregs)[REG_R10]);
  printf("REG_R11 %llx\n", (*gregs)[REG_R11]);
  printf("REG_R12 %llx\n", (*gregs)[REG_R12]);
  printf("REG_R13 %llx\n", (*gregs)[REG_R13]);
  printf("REG_R14 %llx\n", (*gregs)[REG_R14]);
  printf("REG_R15 %llx\n", (*gregs)[REG_R15]);

  printf("REG_RDI %llx\n", (*gregs)[REG_RDI]);
  printf("REG_RSI %llx\n", (*gregs)[REG_RSI]);
  printf("REG_RBP %llx\n", (*gregs)[REG_RBP]);
  printf("REG_RBX %llx\n", (*gregs)[REG_RBX]);
  printf("REG_RDX %llx\n", (*gregs)[REG_RDX]);
  printf("REG_RAX %llx\n", (*gregs)[REG_RAX]);
  printf("REG_RCX %llx\n", (*gregs)[REG_RCX]);
  printf("REG_RSP %llx\n", (*gregs)[REG_RSP]);
  printf("REG_RIP %llx\n", (*gregs)[REG_RIP]);

  printf("REG_EFL %llx\n", (*gregs)[REG_EFL]);
  /* Actually short cs, gs, fs, __pad0.  */
  printf("REG_CSGSFS %llx\n", (*gregs)[REG_CSGSFS]);
  printf("REG_ERR %llx\n", (*gregs)[REG_ERR]);
  printf("REG_TRAPNO %llx\n", (*gregs)[REG_TRAPNO]);
  printf("REG_OLDMASK %llx\n", (*gregs)[REG_OLDMASK]);
  printf("REG_CR2 %llx\n", (*gregs)[REG_CR2]);
}

/*
  
  end live    ->
  start frame -> 
                
  end siginfo -> ...ebf0

  siginfo     -> ...eb70

  end context -> ...ede8

  context     -> ...ea40
 */
void gc_flip_action_func(int signum, siginfo_t *siginfo, void *context) {
  long stack_top;
  int thread_index;

  // we cannot be in the middle of an allocation at this point because
  // the gc holds all the group free_locks
  if (0 == (thread_index =  (long) pthread_getspecific(thread_index_key))) {
    printf("pthread_getspecific failed!\n");
  } else {
    printf("Pausing thread %d on signal %d\n", thread_index, signum);
    printf("stack is %p\n", &stack_top);
    printf("siginfo is 0x%lx\n", siginfo);
    printf("context is 0x%lx\n", context);
    printf("sizeof(siginfo_t) is %x\n", sizeof(siginfo_t));
    printf("sizeof(ucontext_t) is %x\n", sizeof(ucontext_t));
    fflush(stdout);

    // HEY! correct for size of context, ucontent, and anything else
    // we know about
    printf("Addr of stack_top is %p\n", &stack_top);
    long live_stack_size = threads[thread_index].stack_bottom - 
                           (char *) &stack_top;
    printf("live stack size is 0x%lx\n", live_stack_size);

    // Be careful here, must copy from lowest to highest address
    // in both real stack and saved stack
    // This ends up inverting the stack so we can start scanning from
    // lowest to highest address
    memcpy(threads[thread_index].saved_stack_base,
	   &stack_top,
	   live_stack_size);
    threads[thread_index].saved_stack_size = live_stack_size;

    
    // Copy registers - NREGS is 23 on x86_64
    ucontext_t *ucontext = (ucontext_t *) context;
    mcontext_t *mcontext = &(ucontext->uc_mcontext);
    gregset_t *gregs = &(mcontext->gregs);
    memcpy(&(threads[thread_index].registers), gregs, sizeof(gregset_t));
    //print_registers(&(threads[thread_index].registers));

    counter_increment(&stacks_copied_counter);
    printf("Resuming after signal\n");
  }
}

void init_signals_for_rtgc() {
  struct sigaction signal_action;

  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  // sigprocmask seems to do the same thing as this
  if (0 != pthread_sigmask(SIG_UNBLOCK, 0, &set)) {
    printf("mask failed!");
  }
  
  memset(&signal_action, 0, sizeof(signal_action));
  signal_action.sa_sigaction = gc_flip_action_func;
  signal_action.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR1, &signal_action, 0);
}

// Return total number of mutators stopped
int stop_all_mutators_and_save_state() {  
  // stop the world and copy all stack and register state in each live thread
  counter_init(&stacks_copied_counter);
  pthread_mutex_lock(&total_threads_lock);
  int total_threads_to_halt = total_threads - 1; /* omit gc thread */
  pthread_mutex_unlock(&total_threads_lock);
  for (int i = 0; i < total_threads_to_halt; i++) {
    int thread = i + 1;		// skip 0 - gc thread
    threads[thread].saved_stack_size = 0;
    pthread_kill(threads[thread].pthread,SIGUSR1);
  }
  counter_wait_threshold(&stacks_copied_counter, total_threads_to_halt);
  
  // all stacks should be copied at this point
  for (int i = 0; i < total_threads_to_halt; i++) {
    int thread = i + 1;
    if (0 == threads[thread].saved_stack_size) {
      printf("Stack copy problem!");
      Debugger();
    }
  }
  printf("***All stacks copied!*****\n");
}


