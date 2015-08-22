// (C) Copyright 2015 by Wade L. Hennessey. All rights reserved.

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include "compat.h"
#include "infoBits.h"
#include <signal.h>
#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"
#include <sys/time.h>
#include <pthread.h>


/* http://www.textfiles.com/etext/AUTHORS/DOYLE/ for text files */

typedef struct node {
  char *word;
  int count;
  struct node *lesser;
  struct node *greater;
} NODE;

NODE *root;

int node_count = 0;
NODE *new_node(char *word, NODE *lesser, NODE *greater) {
  NODE *node = (NODE *) SXallocate(SXpointers, sizeof(NODE));
  node->word = word;
  node->count = 1;
  setf_init(node->lesser, lesser);
  setf_init(node->greater, greater);
  node_count = node_count + 1;
  return node;
}

int smallword_count = 0;
int bigword_count = 0;
char *new_word(char *buffer, int i) {
  char *word = SXallocate(SXnopointers, (i + 1));
  buffer[i] = '\0';
  strcpy(word, buffer);
  if ((i + 1) > 8) {
    bigword_count = bigword_count + 1;
  } else {
    smallword_count = smallword_count + 1;
  }
  return(word);
}

char *read_word(FILE *f) {
  int c;
  char buffer[1024];
  int i = 0;
  do {
    c = fgetc(f);
    if (EOF == c) {
      return(NULL);
    }
  } while (!isalnum(c));
  do {
    buffer[i] = c;
    i = i + 1;
    c = fgetc(f);
    if (EOF == c) {
      return(new_word(buffer, i));
    }
  } while (isalnum(c));
  return(new_word(buffer, i));
}

void insert_node(NODE *next, char *word) {
  int result = strcmp(word, next->word);
  if (0 == result) {
    next->count = next->count + 1;
  } else {
    if (result < 0) {
      if (NULL == next->lesser) {
	setf_init(next->lesser, new_node(word, 0, 0));
      } else {
	result = strcmp(word, (next->lesser)->word);
	if (0 == result) {
	  (next->lesser)->count = (next->lesser)->count +1;
	} else {
	  if (result < 0) {
	    insert_node(next->lesser, word);
	  } else {
	    /* Insert new node between next and lesser */
	    NODE *new = new_node(word, next->lesser, 0);
	     SXwrite_barrier(&(next->lesser), new);
	  }
	}
      }
    } else {
      if (NULL == next->greater) {
	setf_init(next->greater, new_node(word, 0, 0));
      } else {
	result = strcmp(word, (next->greater)->word);
	if (0 == result) {
	  (next->greater)->count = (next->greater)->count + 1;
	} else {
	  if (result > 0) {
	    insert_node(next->greater, word);
	  } else {
	    /* Insert new node between next and greater */
	    NODE *new = new_node(word, 0, next->greater);
	    SXwrite_barrier(&(next->greater), new);
	  }
	}
      }
    }
  }
}

NODE *build_word_tree(char *filename) {
  FILE *f = fopen(filename, "r");
  if (f == NULL) {
    printf("Cannot open file %s\n", filename);
  } else {
    char *word;
    word = read_word(f);
    SXwrite_barrier(&root, new_node(word, NULL, NULL));
    while (NULL != (word = read_word(f))) {
      insert_node(root, word);
    }
  }
}

// Walk word tree, print if verbose is not 0. Return total word count.
int walk_word_tree(NODE *n, int verbose) {
  int count;
  if (NULL == n) {
    count = 0;
  } else {
    count = n->count;
    count = count + walk_word_tree(n->lesser, verbose);
    if (0 != verbose) {
      printf("%d %s\n", n->count, n->word);
    }
    count = count + walk_word_tree(n->greater, verbose);
  }
  return(count);
}

#define HEAP_SIZE (1 << 19)
#define STATIC_SIZE 0

void init_global_bounds() {
  first_globals_ptr = (BPTR) &root;
  last_globals_ptr = ((BPTR) &root) + sizeof(root);
}

/************************ atomic flip testing ********************/

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


void *start_thread(void *arg) {
  while (1) {
    //printf("*");
    //fflush(stdout);
    //usleep(200000);
  }
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
    
    printf("Resuming after signal\n");
  }
}

void create_threads() {
  int t1, t2, t3;

  struct sigaction signal_action;
  // HEY! do this signal and key create stuff in init funcs

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
  
  t1 = new_thread(&start_thread, (void *) 0);
  //t2 = new_thread(&start_thread, (void *) 1);
  //t3 = new_thread(&start_thread, (void *) 2);
  sleep(1);
  pthread_kill(threads[t1].pthread, SIGUSR1);
  //pthread_kill(threads[t2].pthread, SIGINT);
  //pthread_kill(threads[t3].pthread, SIGINT);
  sleep(5000);
}


/************************ end atomic flip testing ********************/

int main(int argc, char *argv[]) {
  char base;
  int i;

  SXinit_heap(HEAP_SIZE, STATIC_SIZE);
  create_threads();
  exit(1);
  
  printf("PID is %d\n", getpid());
  printf("LINK_INFO_BITS is 0x%x\n", LINK_INFO_BITS);

  printf("LINK_INFO_MASK is 0x%x\n", LINK_INFO_MASK);
  printf("LINK_POINTER_MASK 0xis %x\n", LINK_POINTER_MASK);
  printf("sizeof(long) is %d\n", sizeof(long));
  printf("sizeof(int) is %d\n", sizeof(int));
  printf("sizeof(char *) is %d\n\n", sizeof(char *));
  
  i = 0;
  while (i < 500) {
    char top;
    build_word_tree("redhead.txt");
    printf("Total words %d\n", walk_word_tree(root, 0));
    full_gc();
    i = i + 1;
  }

}


