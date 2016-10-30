// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/time.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include "info-bits.h"
#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"

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

New method uses sig_atomic_t flags and locked increment instructions.
*/

// Integers safe to read and set in signal handler
// static volatile sig_atomic_t volatile is ESSENTIAL, 
// or -O2 optimizaions break things
static volatile long entered_handler_count = 0;
static volatile long copied_stack_count = 0;

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
  // Actually short cs, gs, fs, __pad0.
  printf("REG_CSGSFS %llx\n", (*gregs)[REG_CSGSFS]);
  printf("REG_ERR %llx\n", (*gregs)[REG_ERR]);
  printf("REG_TRAPNO %llx\n", (*gregs)[REG_TRAPNO]);
  printf("REG_OLDMASK %llx\n", (*gregs)[REG_OLDMASK]);
  printf("REG_CR2 %llx\n", (*gregs)[REG_CR2]);
}

void gc_flip_action_func(int signum, siginfo_t *siginfo, void *context) {
  THREAD_INFO *thread;
  struct timeval start_tv, end_tv, pause_tv;
  long current_gc_count = gc_count;

  // We cannot be in the middle of an allocation at this point because
  // the gc holds all the group free_locks
  gettimeofday(&start_tv, 0);
  locked_long_inc(&entered_handler_count);
  if (0 == (thread = pthread_getspecific(thread_key))) {
    printf("pthread_getspecific failed!\n");
  } else {

    ucontext_t *ucontext = (ucontext_t *) context;
    mcontext_t *mcontext = &(ucontext->uc_mcontext);
    gregset_t *gregs = &(mcontext->gregs);

    //memcpy(&(thread->registers), gregs, sizeof(gregset_t));


    // switch to this
    memcpy(&(saved_threads[thread->saved_thread_index].registers),
	     gregs,
	     sizeof(gregset_t));

    
    // real interrupted stack pointer is saved in the RSP register
    char *stack_top = (char *) (*gregs)[REG_RSP];
    long live_stack_size = thread->stack_bottom - stack_top;

    /*
    memcpy(thread->saved_stack_base,
	   stack_top,
	   live_stack_size);
    thread->saved_stack_size = live_stack_size;
    */
    
    // switch to this

    // Be careful here, must copy from lowest to highest address
    // in both real stack and saved stack
    memcpy(saved_threads[thread->saved_thread_index].saved_stack_base,
	   stack_top,
	   live_stack_size);
    saved_threads[thread->saved_thread_index].saved_stack_size = live_stack_size;

    
    locked_long_inc(&copied_stack_count);

    gettimeofday(&end_tv, 0);
    timersub(&end_tv, &start_tv, &pause_tv);
    timeradd(&(thread->total_pause_tv),
	     &pause_tv,
	     &(thread->total_pause_tv));
    if timercmp(&pause_tv, &(thread->max_pause_tv), >) {
    	thread->max_pause_tv = pause_tv;
      }
  }

  if (1 == RTatomic_gc) {
    while (gc_count == (current_gc_count + 2)) {
      sched_yield();
    }
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
  signal_action.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(FLIP_SIGNAL, &signal_action, 0);
}

void lock_all_free_locks() {
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    GPTR group = &groups[i];
    pthread_mutex_lock(&(group->free_lock));
  }
}

void unlock_all_free_locks() {
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    GPTR group = &groups[i];
    pthread_mutex_unlock(&(group->free_lock));
  }
}

// Return total number of mutators stopped
int stop_all_mutators_and_save_state() {  
  // stop the world and copy all stack and register state in each live thread
  entered_handler_count = 0;
  copied_stack_count = 0;
  pthread_mutex_lock(&total_threads_lock);
  int total_threads_to_halt = 0;
  THREAD_INFO *thread = live_threads;
  while (thread != NULL) {
    thread->saved_thread_index = total_threads_to_halt;
    saved_threads[total_threads_to_halt].saved_stack_size = 0;
      
    total_threads_to_halt = total_threads_to_halt + 1;
    // delete this
    //thread->saved_stack_size = 0;
    int err = pthread_kill(thread->pthread, FLIP_SIGNAL);
    if (0 != err) {
      if (ESRCH == err) {
	// Try to call free_thread here? Probably not.
	// This should have been done correctly on pthread exit, no
	// matter how it occurred.
	Debugger("Not a valid thread handle\n");
      }	else {
	Debugger("pthread_kill failed!");
      }
    }
    thread = thread->next;
  }

  if (0 != RTno_write_barrier_state_ptr) {
    saved_no_write_barrier_state = *RTno_write_barrier_state_ptr;
  }
  
  while (entered_handler_count != total_threads_to_halt) {
    sched_yield();
  }
  
  enable_write_barrier = 1;
  SWAP(marked_color,unmarked_color);
  unlock_all_free_locks();

  // Busy wait to start gc cycle until all thread stacks are copied
  while (copied_stack_count != total_threads_to_halt) {
    sched_yield();
  }
  // all stacks and registers should be copied at this point
  assert(total_threads_to_halt == copied_stack_count);
  // Allow creation of new threads now
  pthread_mutex_unlock(&total_threads_lock);

  // We could return this and pass it around, but what's the point.
  // Its unique global info used once per gc cycle
  total_saved_threads = total_threads_to_halt;
}

