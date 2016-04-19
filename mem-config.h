// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

/* Memory management configuration */

// HEY! get rid of max_segments?
#define MAX_HEAP_SEGMENTS 1
#define MAX_STATIC_SEGMENTS 1
#define MAX_SEGMENTS MAX_HEAP_SEGMENTS + MAX_STATIC_SEGMENTS
#define MAX_THREADS 100
#define MAX_GLOBAL_ROOTS 1000

/* The heap is divided into multiple segments */
#define DEFAULT_HEAP_SEGMENT_SIZE 1 << 20
#define DEFAULT_STATIC_SEGMENT_SIZE 0     /* only 1 static segment for now */
#define CHECK_BASH 0
#define CHECK_SETFINIT 1
#define GC_POINTER_ALIGNMENT (sizeof(long *))
#define PAGE_POWER 11	       /* x86_64 page size is normally 4096 */
#define INTERIOR_PTR_RETENTION_LIMIT 512

#define FLIP_SIGNAL SIGUSR1

// HEY! use timersub instead and write timeval_to_double 
#define ELAPSED_MILLISECONDS(start, delta) {struct timeval end; \
    gettimeofday(&end, 0); \
    delta = (end.tv_usec - start.tv_usec) / 1000.0; }
#define START_CODE_TIMING { struct timeval start_time; double time; \
    gettimeofday(&start_time, 0);
#define END_CODE_TIMING(total) ELAPSED_MILLISECONDS(start_time, time); \
    total = total + time; }

#define ENABLE_VISUAL_MEMORY 0
#define VISUAL_MEMORY_ON (ENABLE_VISUAL_MEMORY && visual_memory_on)
#define VISUAL_MEMORY_DEFAULT_ON 0
#define UPDATE_VISUAL_STATE() {if (VISUAL_MEMORY_ON) { \
                               RTdraw_visual_gc_state(); }}
#define ENABLE_GC_TIMING 1
#define DETECT_INVALID_REFS 0

#define USE_BIT_WRITE_BARRIER 1


