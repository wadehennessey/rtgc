// (C) Copyright 2015 by Wade L. Hennessey. All rights reserved.

/* State shared by rtalloc, rtgc, and vizmem goes here */

#define _USE_GNU
#define _GNU_SOURCE


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include "compat.h"
#include "mem-config.h"
#include "infoBits.h"
#include "mem-internals.h"
#include "allocate.h"

GROUP_INFO *groups;
PAGE_INFO *pages;

SEGMENT *segments;
int total_segments;

THREAD_INFO *threads;
int total_threads;

char **global_roots;
int total_global_roots;

// get rid of this when explicit root registration works.
BPTR first_globals_ptr;
BPTR last_globals_ptr;

/* HEY! only 1 static segment while these are global! */
BPTR first_static_ptr;
BPTR last_static_ptr;

BPTR first_partition_ptr;
BPTR last_partition_ptr;

int total_partition_pages;
int unmarked_color;
int marked_color;
int enable_write_barrier;
int memory_mutex;

int visual_memory_on;
char *last_gc_state;

pthread_key_t thread_index_key;

pthread_mutex_t flip_lock;