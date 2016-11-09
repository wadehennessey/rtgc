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
#include "info-bits.h"
#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"

// http://www.textfiles.com/etext/AUTHORS/DOYLE/ for text files

typedef struct node {
  char *word;
  int count;
  struct node *lesser;
  struct node *greater;
} NODE;

RT_METADATA NODE_md[] = {sizeof(NODE),
			 offsetof(NODE, word),
			 offsetof(NODE, lesser),
			 offsetof(NODE, greater),
			 -1};

NODE *new_node(char *word, NODE *lesser, NODE *greater) {
  //NODE *node = (NODE *) RTallocate(RTpointers, sizeof(NODE));
  NODE *node = (NODE *) RTallocate(NODE_md, 1);
  node->word = word;
  node->count = 1;
  setf_init(node->lesser, lesser);
  setf_init(node->greater, greater);
  return node;
}

char *new_word(char *buffer, int i) {
  char *word = RTallocate(RTnopointers, (i + 1));
  buffer[i] = '\0';
  strcpy(word, buffer);
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

void insert_node(NODE *next, char *word, int level) {
  int result = strcmp(word, next->word);
  if (level < 1844) {
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
	      insert_node(next->lesser, word, level + 1);
	    } else {
	      // Insert new node between next and lesser
	      NODE *new = new_node(word, next->lesser, 0);
	      RTwrite_barrier(&(next->lesser), new);
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
	      insert_node(next->greater, word, level + 1);
	    } else {
	      // Insert new node between next and greater
	      NODE *new = new_node(word, 0, next->greater);
	      RTwrite_barrier(&(next->greater), new);
	    }
	  }
	}
      }
    }
  } else {
    Debugger("Infinite recursion!\n");
  }
}

NODE *build_word_tree(char *filename) {
  FILE *f = fopen(filename, "r");
  if (f == NULL) {
    printf("Cannot open file %s\n", filename);
    return(NULL);
  } else {
    char *word;
    NODE *root;
    word = read_word(f);
    // No write_barrier needed - intialzing write to the stack
    root = new_node(word, NULL, NULL);
    while (NULL != (word = read_word(f))) {
      insert_node(root, word, 0);
    }
    fclose(f);
    return(root);
  }
}

// Walk word tree, print if verbose is not 0. Return total word count.
int walk_word_tree(NODE *n, int verbose) {
  int r, count;
  if (NULL == n) {
    count = 0;
  } else {
    if (0 != n->lesser) {
      r = strcmp(n->word, (n->lesser)->word);
      assert(r > 0);
    }
    if (0 != n->greater) {
      r = strcmp(n->word, (n->greater)->word);
      assert(r < 0);
    }
    count = n->count;
    count = count + walk_word_tree(n->lesser, verbose);
    if (0 != verbose) {
      printf("%d %s\n", n->count, n->word);
    }
    count = count + walk_word_tree(n->greater, verbose);
  }
  return(count);
}

void *start_word_count(void *arg) {
  long count = (long) arg;
  long i = 0;

  pthread_t thread = pthread_self();
  while (i < count) {
    char top;
    NODE *root = build_word_tree("redhead.txt");
    assert(9317 == walk_word_tree(root, 0));
    if (0 == (i % 25)) {
      printf("[%p] %d word counts\n", thread, i);
    }
    i = i + 1;
  }
}

/*
void *make_threads(void *arg) {
   pthread_t thread;
   for (long i = 0; i < 3; i++) {
     pthread_t thread;
     RTpthread_create(&thread, NULL, &start_word_count, (void *) i);
   }
}
*/

void *make_threads(void *arg) {
  long counts_per_thread = 100;
  long total_threads_created = 0;
  while (1) {
    // should make total_threads a condition variable
    while (total_threads < 4) {
      pthread_t thread;
      int err;
      if (0!= (err = RTpthread_create(&thread, 
				      NULL, 
				      &start_word_count,
				      (void *) counts_per_thread))) {
	printf("Thread create failure, err = %d\n", err);
	Debugger("Thread create failure");
      } else {
	total_threads_created = total_threads_created + 1;
	if (0 == (total_threads_created % 25)) {
	  printf("**************** %d threads created (%d total word counts)\n",
		 total_threads_created, 
		 total_threads_created * counts_per_thread);
	}
      }
    }
    sched_yield();
  }
}

int main(int argc, char *argv[]) {
  RTatomic_gc = 0;
  // When using RTatomic_gc = 1 we need about 4x them minimum heap size we need
  // when using RTatomic_gc = 0, otherwise we'll get "out of memory Heap"
  // errors.
  RTinit_heap((1L << 23), 1L << 18);
  pthread_t thread;
  RTpthread_create(&thread, NULL, &make_threads, 0);
  rtgc_loop();
}
