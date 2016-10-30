// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

// State shared by rtalloc, rtgc, and vizmem goes here

#define _USE_GNU
#define _GNU_SOURCE


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include "mem-config.h"
#include "info-bits.h"
#include "mem-internals.h"
#include "allocate.h"

GROUP_INFO *groups;
PAGE_INFO *pages;
HOLE_PTR empty_pages;

int RTpage_power = PAGE_POWER;
int RTpage_size = BYTES_PER_PAGE;  
SEGMENT *segments;
int total_segments;

THREAD_INFO *threads;
THREAD_INFO *live_threads;
THREAD_INFO *free_threads;
int total_threads = 0;

char **global_roots;
int total_global_roots;

// HEY! only 1 static segment while these are global!
BPTR first_static_ptr;
BPTR last_static_ptr;
BPTR static_frontier_ptr;

BPTR first_partition_ptr;
BPTR last_partition_ptr;

#if USE_BIT_WRITE_BARRIER
LPTR RTwrite_vector;
#else
BPTR RTwrite_vector;
#endif
size_t RTwrite_vector_length;

long total_partition_pages;
int unmarked_color;
int marked_color;
int enable_write_barrier;
volatile long gc_count;

pthread_key_t thread_key;

pthread_mutex_t total_threads_lock;
pthread_mutex_t global_roots_lock;
pthread_mutex_t empty_pages_lock;
// Use locked add instruction instead of a mutex?
pthread_mutex_t static_frontier_ptr_lock;

sem_t gc_semaphore;
volatile int run_gc = 0;
volatile int RTatomic_gc = 0;

long *RTno_write_barrier_state_ptr = 0;
long saved_no_write_barrier_state = 0;

