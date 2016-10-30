// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

// Memory management configuration

// HEY! get rid of max_segments?
#define MAX_HEAP_SEGMENTS 1
#define MAX_STATIC_SEGMENTS 1
#define MAX_SEGMENTS MAX_HEAP_SEGMENTS + MAX_STATIC_SEGMENTS
#define MAX_THREADS 20
#define MAX_GLOBAL_ROOTS 1000

// The heap is divided into multiple segments
#define DEFAULT_HEAP_SEGMENT_SIZE 1 << 20
#define DEFAULT_STATIC_SEGMENT_SIZE 0     /* only 1 static segment for now */
#define CHECK_BASH 0
#define CHECK_SETFINIT 1
#define GC_POINTER_ALIGNMENT (sizeof(long *))
#define PAGE_POWER 12	       /* x86_64 page size is normally 4096 */
//#define INTERIOR_PTR_RETENTION_LIMIT 32
#define INTERIOR_PTR_RETENTION_LIMIT 512

#define FLIP_SIGNAL SIGUSR1
#define DETECT_INVALID_REFS 0
#define USE_BIT_WRITE_BARRIER 1

#ifdef NDEBUG
#define DEBUG(x)
#else
#define DEBUG(x) x
#endif


