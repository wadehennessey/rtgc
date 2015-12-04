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
#include "infoBits.h"
#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"
#include "vizmem.h"

/*
Tried using condition variables to synch handlers and gc. Always deadlocked after
running a long time. Then discovered this from the pthread_cond_broadcast
man page:

       int pthread_cond_broadcast(pthread_cond_t *cond);
       int pthread_cond_signal(pthread_cond_t *cond);

       It  is  not  safe  to use the pthread_cond_signal() function in a signal
       handler that is invoked asynchronously. Even  if  it  were  safe,  there
       would   still   be   a   race   between   the   test   of   the  Boolean
       pthread_cond_wait() that could not be efficiently eliminated.

       Mutexes and condition variables are thus not suitable  for  releasing  a
       waiting thread by signaling from code running in a signal handler.

New method using sig_atomic_t flags. 
3 step "handshake" interaction:

1. handler_done = 0. mutators_may_proceed = 0. 
   Stop every mutator, mutators copy stack, set handler_done - 1;

2. then busy wait until gc thread sets mutators_may_proceed to 1.

3. Then each mutator acknowledges mutators_may proceed == 1 by exiting busy wait
   and setting handler_done = -1 (meaning handlers is exiting). Gc thread uses 
   handler_done == -1 loop to wait for all acknowledgements before starting 
   a new gc cycle. This avoids being deadlocked in step 2, and each new gc cycle
   starts only after all mutators acknowledge signal handler is going to exit.

*/

// integers safe to read and set in signal handler
// could probaly just haved use a normal int.
static volatile sig_atomic_t mutators_may_proceed = 0;
static volatile sig_atomic_t handler_done[100]; //  = 0;

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
  int thread_index;

  // we cannot be in the middle of an allocation at this point because
  // the gc holds all the group free_locks
  if (0 == (thread_index =  (long) pthread_getspecific(thread_index_key))) {
    printf("pthread_getspecific failed!\n");
  } else {
    //printf("Pausing thread %d on signal %d\n", thread_index, signum);

    // Copy registers - NREGS is 23 on x86_64
    ucontext_t *ucontext = (ucontext_t *) context;
    mcontext_t *mcontext = &(ucontext->uc_mcontext);
    gregset_t *gregs = &(mcontext->gregs);
    memcpy(&(threads[thread_index].registers), gregs, sizeof(gregset_t));

    // real interrupted stack pointer is saved in the RSP register
    char *stack_top = (char *) (*gregs)[REG_RSP];
    //printf("Saved stack bottom is %p\n", threads[thread_index].stack_bottom);
    //printf("Interrupted stack_top is %p\n", stack_top);
    long live_stack_size = threads[thread_index].stack_bottom - 
                           stack_top;
    //printf("live stack size is 0x%lx\n", live_stack_size);

    // Be careful here, must copy from lowest to highest address
    // in both real stack and saved stack
    memcpy(threads[thread_index].saved_stack_base,
	   stack_top,
	   live_stack_size);
    threads[thread_index].saved_stack_size = live_stack_size;

    handler_done[thread_index] = 1;
    //if (1 != counter_increment(&stacks_copied_counter)) {
    //  Debugger("error, counter_increment failed!\n");
    //} 

    //printf("about to wait on may_proceed\n");
    // Wait until all threads have stopped, and the write barrier
    // has been turned back on
    while (0 == mutators_may_proceed) {
      sched_yield();
    }
    // indicate mutator is proceeding
    handler_done[thread_index] = -1;
    //printf("Resuming after signal\n");
  }
}

void init_signals_for_rtgc() {
  struct sigaction signal_action;
  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, FLIP_SIGNAL);
  // sigprocmask seems to do the same thing as this
  if (0 != pthread_sigmask(SIG_UNBLOCK, 0, &set)) {
    printf("mask failed!");
  }
  
  memset(&signal_action, 0, sizeof(signal_action));
  signal_action.sa_sigaction = gc_flip_action_func;
  signal_action.sa_flags = SA_SIGINFO;
  sigaction(FLIP_SIGNAL, &signal_action, 0);
}

// Return total number of mutators stopped
int stop_all_mutators_and_save_state() {  
  // stop the world and copy all stack and register state in each live thread
  pthread_mutex_lock(&total_threads_lock);
  int total_threads_to_halt = total_threads - 1; /* omit gc thread */
  counter_zero(&stacks_copied_counter);
  mutators_may_proceed = 0;
  for (int i = 0; i < total_threads_to_halt; i++) {
    int thread = i + 1;		// skip 0 - gc thread
    handler_done[thread] = 0;
    threads[thread].saved_stack_size = 0;
    int err = pthread_kill(threads[thread].pthread, FLIP_SIGNAL);
    if (0 != err) {
      printf("pthread_kill failed with err %d!\n", err);
      Debugger("pthread_kill failed!");
    }
  }
  //counter_wait_threshold(&stacks_copied_counter, total_threads_to_halt);
    
  // all stacks and registers should be copied at this point
  for (int i = 0; i < total_threads_to_halt; i++) {
    int thread = i + 1;
    while (0 == handler_done[thread]);
    if (0 == threads[thread].saved_stack_size) {
      Debugger("Stack copy problem!");
    }
  }
  //printf("***All stacks copied!*****\n");
  enable_write_barrier = 1;
  mutators_may_proceed = 1;
  //printf("mutators_may_proceed is now %d\n", mutators_may_proceed);
  for (int i = 0; i < total_threads_to_halt; i++) {
    int thread = i + 1;
    while (-1 != handler_done[thread]);
  }
  pthread_mutex_unlock(&total_threads_lock);
  //usleep(100000);
}

