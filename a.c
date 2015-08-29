// (C) Copyright 2015 by Wade L. Hennessey. All rights reserved.

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
#include <pthread.h>
#include "compat.h"
#include "infoBits.h"

#include "mem-config.h"
#include "mem-internals.h"
#include "allocate.h"


/* http://www.textfiles.com/etext/AUTHORS/DOYLE/ for text files */

typedef struct node {
  int *md;
  char *word;
  int count;
  struct node *lesser;
  struct node *greater;
} NODE;

NODE sample_node;

int NODE_md[] = {offsetof(NODE, word),
		 offsetof(NODE, lesser),
		 offsetof(NODE, greater),
		 -1};

NODE *root;

int node_count = 0;
NODE *new_node(char *word, NODE *lesser, NODE *greater) {
  NODE *node = (NODE *) SXallocate(SXpointers, sizeof(NODE));
  //NODE *node = (NODE *) SXallocate(NODE_md, sizeof(NODE));
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
    fclose(f);
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

void *start_word_count(void *arg) {
  int i = 0;
  while (i < 500) {
    char top;
    //usleep(1000);
    build_word_tree("redhead.txt");
    printf("%d: Total words %d\n", i, walk_word_tree(root, 0));
    i = i + 1;
  }
  exit(1);
}

int main(int argc, char *argv[]) {
  SXinit_heap(1 << 19, 0);
  register_global_root(&root);
  new_thread(&start_word_count, (void *) 0);
  rtgc_loop();
}

