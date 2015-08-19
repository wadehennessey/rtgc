// Need this define to use/see pthread_getattr_np!
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include "compat.h"
#include "infoBits.h"
#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>

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

BPTR main_stack_base;
BPTR main_stack_top;

BPTR do_nothing(BPTR p) {
  return(p);
}

void init_global_bounds() {
  first_globals_ptr = (BPTR) &root;
  last_globals_ptr = ((BPTR) &root) + sizeof(root);
}

/************************ atomic flip testing ********************/
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

pthread_key_t thread_index_key;

void *start_thread(void *arg) {
  int thread_index = (long) arg;
  printf("Thread %d started, live stack base is 0x%lx\n", 
	 thread_index, &thread_index);
  threads[thread_index].stack_bottom = (char *) &thread_index;
  if (0 != pthread_setspecific(thread_index_key, arg)) {
    printf("pthread_setspecific failed!\n"); 
  } else {
    while (1) {
      printf("*");
      fflush(stdout);
      usleep(200000);
    }
  }
}

void signal_action_func(int signum, siginfo_t *siginfo, void *context) {
  char stack_top;
  pthread_t self;
  pthread_attr_t attr;
  int thread_index;

  // we cannot be in the middle of an allocation at this point because
  // the gc holds all the group free_locks
  if (0 == (thread_index =  (long) pthread_getspecific(thread_index_key))) {
    printf("pthread_getspecific failed!\n");
  } else {
    void *stackaddr;
    size_t stacksize;
  
    printf("Pausing thread %d on signal %d\n", thread_index, signum);
    printf("stack is %p\n", &self);
    printf("siginfo is 0x%lx\n", siginfo);
    printf("context is 0x%lx\n", context);
    printf("sizeof(siginfo_t) is %x\n", sizeof(siginfo_t));
    printf("sizeof(uccontext_t) is %x\n", sizeof(ucontext_t));
    fflush(stdout);

    // HEY! only need to do this once if if .stack_base is 0
    self = pthread_self();
    pthread_getattr_np(self, &attr);
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);
    // HEY! stackaddr is the LOWEST addressable byte of the stack
    // The stackbottom starts at stackaddr + stacksize!
    printf("Stackaddr is %p\n", stackaddr);
    printf("Stacksize is 0x%x\n", stacksize);
    threads[thread_index].stack_base = stackaddr + stacksize;
    threads[thread_index].stack_size = stacksize;
    fflush(stdout);

    // HEY! correct for size of context, ucontent, and anything else
    // we know about
    printf("Addr of stack_top is %p\n", &stack_top);
    long live_stack_size = threads[thread_index].stack_bottom - &stack_top;
    printf("live stack size is 0x%lx\n", live_stack_size);

    // Be careful here, must copy from lowest to highest address
    // in both real stack and saved stack
    /*
    memcpy(threads[thread_index].saved_stack_base,
	   &stack_top,
	   live_stack_size);
    */
    printf("Resuming after signal\n");
  }
}

void create_threads() {
  int t1, t2, t3;
  struct sigaction signal_action;

  memset(&signal_action, 0, sizeof(signal_action));
  signal_action.sa_sigaction = signal_action_func;
  signal_action.sa_flags = SA_SIGINFO;

  // Why doesn't it work with SIGUSR1
  sigaction(SIGINT, &signal_action, 0);
  if (0 != pthread_key_create(&thread_index_key, NULL)) {
    printf("thread_index_key create failed!\n");
  } else {
    t1 = new_thread(&start_thread, (void *) 1);
    //t2 = new_thread(&start_thread, (void *) 2);
    //t3 = new_thread(&start_thread, (void *) 3);

    sleep(1);
    pthread_kill(threads[t1].pthread, SIGINT);
    //pthread_kill(threads[t2].pthread, SIGINT);
    //pthread_kill(threads[t3].pthread, SIGINT);
    sleep(5);
  }
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
    main_stack_top = do_nothing(&top);
    full_gc();
    i = i + 1;
  }

}


