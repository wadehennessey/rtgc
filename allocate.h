// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

/* Interface to the memory manager */

// Requires more than just this header to work
//#define RTnopointers ((void *) SC_NOPOINTERS)
//#define RTpointers   ((void *) SC_POINTERS)

#define RTnopointers ((void *) 0)
#define RTpointers   ((void *) 1)

#define RTbeerBash(lhs, rhs) ((lhs) = (rhs))
#define setf_init(lhs, rhs) ((lhs) = (rhs))

#define INVALID_ADDRESS 0xEF

typedef struct rt_metadata {
  long size;
  long *offsets;
} RT_METADATA;

void * RTwrite_barrier(void *lhs_address, void * rhs);

void * RTsafe_bash(void *lhs_address, void * rhs);

void * RTsafe_setfInit(void *lhs_address, void * rhs);

void * RTallocate(void *metadata, int number_of_bytes);

void * RTstaticAllocate(void *metadata, int number_of_bytes);

void * RTreallocate(void *pointer, int new_size);

void * ptrcpy(void *p1, void * p2, int num_bytes);

void * ptrset(void *p1, int data, int num_bytes);

int RTlargestFreeHeapBlock(void);

int RTstackAllocationSize(void *metadata, int size);

//int RTallocationTrueSize(void *metadata, int size);

int RTtrueSize(void *ptr);

//LPTR RTinitializeObject(void *metadata, void *base, int total_size, int real);

void RTinit_heap(size_t first_segment_bytes, int static_size);

int new_thread(void *(*start_func) (void *), void *args);

int rtgc_count(void);

void RTregister_root_scanner(void (*root_scanner)());

void RTregister_no_write_barrier_state(void *start, int len);

void RTfull_gc();

extern volatile int RTatomic_gc;
