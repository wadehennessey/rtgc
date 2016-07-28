// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

#define _GNU_SOURCE

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
#include <assert.h>
#include <pthread.h>
#include "info-bits.h"
#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"

// http://www.textfiles.com/etext/AUTHORS/DOYLE/ for text files

// tmp stub to avoid linking problems
void *wcl_get_closure_env(void *ptr) {
  return(0);
}

typedef struct node {
  char bloat[1024];
  char *word;
  int count;
  struct node *lesser;
  struct node *greater;
} NODE;

/*
RT_METADATA NODE_md[] = {sizeof(NODE),
			 offsetof(NODE, word),
			 offsetof(NODE, lesser),
			 offsetof(NODE, greater),
			 (long *) -1};
*/

long NODE_md[] = {offsetof(NODE, word),
		  offsetof(NODE, lesser),
		  offsetof(NODE, greater),
		  -1};

NODE *roots[100];

NODE *new_node(char *word, NODE *lesser, NODE *greater) {
  NODE *node = (NODE *) RTallocate(RTpointers, sizeof(NODE));
  //NODE *node = (NODE *) RTallocate(NODE_md, 1);
  node->word = word;
  node->count = 1;
  setf_init(node->lesser, lesser);
  setf_init(node->greater, greater);
  return node;
}

int smallword_count = 0;
int bigword_count = 0;
char *new_word(char *buffer, int i) {
  char *word = RTallocate(RTnopointers, (i + 1));
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
  int i = 0;

  long tid = (long) arg;
  register_global_root(&(roots[tid]));
  while (i < 5000000) {
    char top;
    NODE *root = build_word_tree("redhead.txt");
    RTwrite_barrier(&(roots[tid]), root);
    assert(9317 == walk_word_tree(roots[tid], 0));
    if (0 == (i % 25)) {
      printf("[%d] %d word counts\n", tid, i);
    }
    i = i + 1;
  }
  exit(1);
}

int main(int argc, char *argv[]) {
  struct timespec res, start_time, end_time;
  if (0 == clock_getres(CLOCK_REALTIME, &res)) {
    clock_gettime(CLOCK_REALTIME, &start_time);
    clock_gettime(CLOCK_REALTIME, &end_time);
    //printf("got nano times!\n");
  }

  RTatomic_gc = 0;
  //RTinit_heap((1L << 36), 0);
  RTinit_heap((1L << 25), 1L << 20);
  for (long i = 1; i <= 3; i++) {
    new_thread(&start_word_count, (void *) i);
  }
  rtgc_loop();
}

