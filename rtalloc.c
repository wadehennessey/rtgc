// (C) Copyright 2015 by Wade L. Hennessey. All rights reserved.

/* Real time storage allocater */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include "mem-config.h"
#include "infoBits.h"
#include "mem-internals.h"
#include "vizmem.h"
#include "allocate.h"

HOLE_PTR empty_pages;

int total_allocation;
int total_requested_allocation;
int total_requested_objects;
int total_allocation_this_cycle;

static
int size_to_group_index(int size) {
  int s = size;
  int index = 0;
  
  s = s - 1;
  while (s != 0) {
    s = s / 2;
    index = index + 1;
  }
  return(MAX(MIN_GROUP_INDEX, index));
}

static
void init_group_info() {
  int index;
  for (index = MIN_GROUP_INDEX; index <= MAX_GROUP_INDEX; index = index + 1) {
    int size = 1 << index;
    groups[index].size = size;
    groups[index].index = index;
    groups[index].free = NULL;
    groups[index].free_last = NULL;
    groups[index].white = NULL;
    groups[index].black = NULL;
    groups[index].gray = NULL;
    groups[index].total_object_count = 0;
    groups[index].white_count = 0;
    groups[index].black_count = 0;
    groups[index].green_count = 0;
    pthread_mutex_init(&(groups[index].black_count_lock), NULL);
    pthread_mutex_init(&(groups[index].free_lock), NULL);
    pthread_mutex_init(&(groups[index].black_and_last_lock), NULL);
  }
}

static 
void init_page_info() {
  for (int i = 0; i < total_partition_pages; i++) {
    /* Could put the next two in a per segment table to save memory */
    pages[i].base = NULL;
    pages[i].bytes_used = 0;
    pages[i].group = SYSTEM_PAGE;
  }
}

void SXinit_empty_pages(int first_page, int page_count, int type) {
  int last_page = first_page + page_count;
  for (int i = first_page; i < last_page; i++) {
    /* Could put the next two in a per segment table to save memory */
    pages[i].base = NULL;
    pages[i].bytes_used = 0;
    pages[i].group = EMPTY_PAGE;
    if (VISUAL_MEMORY_ON) SXupdate_visual_page(i);
  }

  if (type == HEAP_SEGMENT) {
    /* Add the pages to the front of the empty page list */
    HOLE_PTR new_hole = (HOLE_PTR) PAGE_INDEX_TO_PTR(first_page);
    new_hole->page_count = page_count;
    new_hole->next = empty_pages;
    empty_pages = new_hole;
  } else {
    /* HEY! fix this to allow more than 1 static segment */
    last_static_ptr = segments[0].last_segment_ptr;
    first_static_ptr = last_static_ptr;
  }
}

static
int allocate_segment(int desired_bytes, int type) {
  int actual_bytes = 0;
  BPTR first_segment_ptr, last_segment_ptr;
  int segment_page_count, first_segment_page;
  int segment = total_segments;

  if ((desired_bytes > 0) &&
      (desired_bytes == (desired_bytes & ~PAGE_ALIGNMENT_MASK)) &&
      (total_segments < MAX_SEGMENTS)) {
    first_segment_ptr = SXbig_malloc(desired_bytes);

    if (NULL != first_segment_ptr) {
      total_segments = total_segments + 1;
      actual_bytes = desired_bytes;
      segment_page_count = actual_bytes / BYTES_PER_PAGE;
      segments[segment].first_segment_ptr = first_segment_ptr;
      last_segment_ptr = first_segment_ptr +
	                 (segment_page_count * BYTES_PER_PAGE);
      segments[segment].last_segment_ptr = last_segment_ptr;
      segments[segment].segment_page_count = segment_page_count;
      segments[segment].type = type;

      // for now we only support a single segment
      if (0 == segment) {
	first_partition_ptr = first_segment_ptr;
	last_partition_ptr = last_segment_ptr;
      } else {
	total_partition_pages = ((last_partition_ptr - first_partition_ptr) /
				 BYTES_PER_PAGE);
	printf("Need code to adjust partition ptrs and size!\n");
	Debugger(0);
      }
      
      first_segment_page = PTR_TO_PAGE_INDEX(first_segment_ptr);
      SXinit_empty_pages(first_segment_page, segment_page_count, type);
    }
  }
  return(actual_bytes);
}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Everything above here should only be run at SXinit_heap time,
// which completes before the gc ever gets a chance to start running.
// Ignore locking issues until we decide to support more than 1 heap segment.

static
GCPTR allocate_empty_pages(int required_page_count,
			   int min_page_count,
			   GPTR group) {
  int remaining_page_count, best_remaining_page_count, next_page_index;
  GCPTR base = NULL;
  HOLE_PTR prev = NULL;
  HOLE_PTR best = NULL;
  HOLE_PTR best_prev = NULL;

  pthread_mutex_lock(&empty_pages_lock);
  HOLE_PTR next = empty_pages;
  /* Search for a best fit hole */
  best_remaining_page_count = total_partition_pages + 1;
  while ((best_remaining_page_count > 0) && (next != NULL)) {
    if (next->page_count >= required_page_count) {
      remaining_page_count = next->page_count - required_page_count;
      if (remaining_page_count < best_remaining_page_count) {
	best_remaining_page_count = remaining_page_count;
	best = next;
	best_prev = prev;
      }
    }
    prev = next;
    next = next->next;
  }

  if (best != NULL) {
    HOLE_PTR rest;
    if (best_remaining_page_count == 0) {
      rest = best->next;
    } else {
      rest = (HOLE_PTR) ((BPTR) best + (required_page_count * BYTES_PER_PAGE));
      rest->page_count = best_remaining_page_count;
      rest->next = best->next;
    }
    if (best_prev == NULL) {
      empty_pages = rest;
    } else {
      best_prev->next = rest;
    }
    base = (GCPTR) best;
  }
  
  /* Initialize page table entries */
  if (base != NULL) {
    int i;
    next_page_index = PTR_TO_PAGE_INDEX(((BPTR) base));
    for (i = 1; i <= required_page_count; i++) {
      pages[next_page_index].base = base;
      pages[next_page_index].group = group;
      pages[next_page_index].bytes_used = 0;
      next_page_index = next_page_index + 1;
      /* commented out code omitted */
    }
  }
  // HEY! should be able to move this to before page table init above
  // just being safe/simple right now
  pthread_mutex_unlock(&empty_pages_lock);
  return(base);
}

// The gc counter part to this function is recycle_group_garbage.
// We could try to unify the small common part.
// Whoever calls this function has to be holding the group->free_lock.
// So far only SXallocate calls this.
static
void init_pages_for_group(GPTR group, int min_pages) {
  int pages_per_object = group->size / BYTES_PER_PAGE;
  int byte_count = MAX(pages_per_object,min_pages) * BYTES_PER_PAGE;
  int num_objects = byte_count >> group->index;
  int page_count = (num_objects * group->size) / BYTES_PER_PAGE;
  int min_page_count = MAX(1, pages_per_object);
  GCPTR base = allocate_empty_pages(page_count, min_page_count, group);

  /* HEY! do this somewhere else? */
  if (base == NULL) {
    int actual_bytes = allocate_segment(MAX(DEFAULT_HEAP_SEGMENT_SIZE,
					    page_count * BYTES_PER_PAGE),
					HEAP_SEGMENT);
    assert(0 == actual_bytes);	// while we only have 1 segement
    if (actual_bytes < byte_count) {
      // atomic and concurrent gc can't flip without
      // unlocking all group free locks
      memory_mutex = 0;
      pthread_mutex_unlock(&(group->free_lock));
      if (atomic_gc) {
	// atomic gc
	run_gc = 1;
	while (1 == run_gc);
      } else {
	// concurrent gc
	// need to wait until gc count increases by 2
	// need to make gc_count a COUNTER counter instead of a lone int

	printf("alloc out ran gc, sync collect\n");
	int current_gc_count = gc_count;
	while (gc_count < (current_gc_count + 2)) {
	  sched_yield();
	}
	assert(NULL != group->free);
      }
      pthread_mutex_lock(&(group->free_lock));
      memory_mutex = 1;
    }
    base = allocate_empty_pages(page_count, min_page_count, group);
  }

  if (base != NULL) {
    GCPTR next = base;
    assert(NULL == group->free);
    group->free = next;
    WITH_LOCK((group->black_and_last_lock),
	      // HEY! lock less here by moving free_last assignment higher up
	      GCPTR current = group->free_last;
	      if (current == NULL) { 	/* No gray, black, or green objects? */
		group->black = next;
	      } else {
		SET_LINK_POINTER(current->next,next);
	      }
	      for (int i = 0; i < num_objects; i++) {
		GCPTR prev = current;
		current = next;
		next = (GCPTR) ((BPTR) current + group->size);
		current->prev = prev;
		current->next = next;
		SET_COLOR(current,GREEN);
	      }
	      SET_LINK_POINTER(current->next,NULL);
	      group->free_last = current;);
    group->green_count = group->green_count + num_objects;
    group->total_object_count = group->total_object_count + num_objects;
  }
}

static
GPTR allocationGroup(int *metadata, int size,
		     int *return_data_size, int *return_real_size, void *return_metadata) {
  int data_size, real_size;

  if (size >= 0) {
    switch ((long) metadata) {
    case (long) SXnopointers:
    case (long) SXpointers:
      data_size = size;
      real_size = size + sizeof(GC_HEADER); break;
    default:
      if (METADATAP(metadata)) {
	/* We count the metadata ptr in data_size for compat with instance */
	data_size = (size * (((RT_METADATA *) metadata)->size)) + sizeof(void *);
	real_size = data_size + sizeof(GC_HEADER);
      } else {
	Debugger("Error - you may not allocate instances\n");
      }
      break;
    }
    
    GPTR group;
    int group_index = size_to_group_index(real_size);
    if (group_index > MAX_GROUP_INDEX) {
      printf("%d exceeds the maximum object size\n", real_size);
    } else {
      group = &(groups[group_index]);
    }
    *return_data_size = data_size;
    *return_real_size = real_size;
    //*((SXobject *) return_metadata) = (SXobject) metadata;
    *((int **) return_metadata) = (int *) metadata;
    return(group);
  } else {
    printf("Negative object size\n");
  }
}

LPTR SXInitializeObject(void *metadata, void *void_base,
			int total_size, int real_size) {
  LPTR base = void_base;
  GCPTR gcptr = (GCPTR) base;
  int limit = ((DETECT_INVALID_REFS) ?
	       ((real_size / sizeof(LPTR)) + ((real_size % sizeof(LPTR)) != 0)) :
	       total_size / sizeof(LPTR));

  for (int i = 2; i < limit; i++) {
    *(base + i) = 0;
  }

  switch ((long) metadata) {
  case (long) SXnopointers:
    SET_STORAGE_CLASS(gcptr,SC_NOPOINTERS);
    base = base + 2;
    break;
  case (long) SXpointers:
    SET_STORAGE_CLASS(gcptr,SC_POINTERS);
    base = base + 2;
    break;    
  default:
    if (METADATAP(metadata)) {
      LPTR last_ptr = base + (total_size / sizeof(LPTR)) - 1;
      SET_STORAGE_CLASS(gcptr,SC_METADATA);
      *last_ptr = (long) metadata;
      base = base + 2;
    } else {
      /* optionally, print out the name of the class being allocated */
      /* code omitted */
      SET_STORAGE_CLASS(gcptr,SC_INSTANCE);
      ((GCMDPTR) gcptr)->metadata = metadata;
      base = base + 2;
    }
    break;
  }
  return(base);
}

void * SXallocate(void *metadata, int size) {
  int i, data_size, real_size, limit;
  GCPTR new;
  LPTR base;
  GPTR group;

  group = allocationGroup(metadata,size,&data_size,&real_size,&metadata);
  pthread_mutex_lock(&(group->free_lock));
  if (group->free == NULL) {
    init_pages_for_group(group,1);
    if (group->free == NULL) {
      out_of_memory("Heap", group->size);
    }
  }
  new = group->free;
  group->free = GET_LINK_POINTER(new->next);
  group->green_count = group->green_count - 1;
  WITH_LOCK(group->black_count_lock,
	    group->black_count = group->black_count + 1;)

  // No need for an explicit flip lock here. During a flip the gc will
  // hold the green lock for each group, so no allocator can get here
  // when the marked_color is being changed.
  SET_COLOR(new,marked_color);	/* Must allocate black! */

  //verify_group(group);
  {
    int page_index = PTR_TO_PAGE_INDEX(new);
    PPTR page = &pages[page_index];
    int old_bytes_used = page->bytes_used;
    page->bytes_used = page->bytes_used + group->size;
    if (VISUAL_MEMORY_ON) {
      SXmaybe_update_visual_page(page_index,old_bytes_used,page->bytes_used);
    }
  }

  base = SXInitializeObject(metadata, new, group->size, real_size);
  // Unlock after Initialization beause storage class is set without a lock
  // and gc recyling garbage may be affect by locked next ptr read, set, and
  // store.
  pthread_mutex_unlock(&(group->free_lock));
  
  /* optional stats 
  total_requested_allocation = total_requested_allocation + data_size;
  total_allocation = total_allocation + group->size;
  total_requested_objects = total_requested_objects + 1;
  total_allocation_this_cycle = total_allocation_this_cycle + group->size;
  */
  return(base);
}
 
/* HEY! make this a macro for speed eventually?
   at least pass in the group ptr! */
GCPTR interior_to_gcptr(BPTR ptr) {
  PPTR page = &pages[PTR_TO_PAGE_INDEX(ptr)];
  GPTR group = page->group;
  GCPTR gcptr;

  if (group > EXTERNAL_PAGE) {
    if (group->size >= BYTES_PER_PAGE) {
      gcptr = page->base;
    } else {
      /* This only works because first_partition_ptr is
	 BYTES_PER_Page aligned */
      gcptr = (GCPTR) ((long) ptr & (-1 << group->index));
    }
  } else {
    printf("ERROR! Found IN_HEAP pointer with NULL group!\n");
  }
  return(gcptr);
}

void verify_group(GPTR group) {
  int black_count = 0;
  GCPTR next = group->black;

  while ((next != group->free) && (next != group->free_last)) {
    black_count = black_count + 1;
    next = GET_LINK_POINTER(next->next);
  }
  if ((group->free != group->free_last) && (group->free_last == next)) {
    /* free is the next object after the last black object
       free is the last black object, so we count it here */
    black_count = black_count + 1;
  }
  if (black_count == group->black_count) {
    //printf("G %d, black count matches: %d\n", group->size, black_count);
  } else {
    Debugger("Group black counts do not match!!!\n");
  }
}

// could try using SIGSTOP and SIGCONT to stop black changes instead
// of locks.

// GC thread caller of this should own all the free locks
void verify_all_groups(void) {
  // iterate over all groups and verify each one
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    verify_group(&groups[i]);
  }
}

int SXtotalFreeHeapSpace() {
  int free = 0;
  int index;

  pthread_mutex_lock(&empty_pages_lock);
  HOLE_PTR next = empty_pages;
  while (next != NULL) {
    free = free + (next->page_count * BYTES_PER_PAGE);
    next = next->next;
  }
  for (index = MIN_GROUP_INDEX; index <= MAX_GROUP_INDEX; index = index + 1) {
    int group_free = groups[index].green_count * groups[index].size;
    free = free + group_free;
  }
  pthread_mutex_unlock(&empty_pages_lock);
  return(free);
}

int SXlargestFreeHeapBlock() {
  int largest = 0;

  pthread_mutex_lock(&empty_pages_lock);
  HOLE_PTR next = empty_pages;
  while (next != NULL) {
    largest = MAX(largest, next->page_count * BYTES_PER_PAGE);
    next = next->next;
  }

  int index = MAX_GROUP_INDEX;
  while (index >= MIN_GROUP_INDEX) {
    if (groups[index].free != NULL) {
      largest = MAX(largest, groups[index].size);
      index = 0;
    } else {
      index = index - 1;
    }
  }
  pthread_mutex_unlock(&empty_pages_lock);
  return(largest);
}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Everything below here should run before the gc starts running
// AS LONG AS WE HAVE ONLY 1 MUTATOR THREAD!


// Thread 0 is considered the main stack that started this process.
// The gc itself runs on Thread 0.
void init_gc_thread() {
  pthread_attr_t attr;
  void *stackaddr;
  size_t stacksize;
  pthread_t self = pthread_self();
  pthread_getattr_np(self, &attr);
  pthread_attr_getstack(&attr, &stackaddr, &stacksize);
  printf("Main Stackaddr is %p\n", stackaddr);
  printf("Main Stacksize is 0x%x\n", stacksize);
  threads[0].stack_base = stackaddr + stacksize;
  threads[0].stack_size = stacksize;
  threads[0].stack_bottom = (char *) &stacksize;
  threads[0].saved_stack_base = 0;
  threads[0].saved_stack_size = 0;
  total_threads = 1;
}


void register_global_root(void *root) {
  if (total_global_roots == MAX_GLOBAL_ROOTS) {
    Debugger("global roots full!\n");
  } else {
    global_roots[total_global_roots] = (char *) root;
    total_global_roots = total_global_roots + 1;
  }
}

void SXinit_heap(int first_segment_bytes, int static_size) {

  enable_write_barrier = 0;
  total_allocation = 0;
  total_requested_allocation = 0;
  total_requested_objects = 0;
  BPTR p;

  /* We malloc vectors here so that they are NOT part of the global
     data segment, and thus will not be scanned if we decide to 
     scan it. We don't want the GC looking at itself! */
  total_partition_pages = first_segment_bytes / BYTES_PER_PAGE;
  groups = malloc(sizeof(GROUP_INFO) * (MAX_GROUP_INDEX + 1));
  pages = malloc(sizeof(PAGE_INFO) * total_partition_pages);
  segments = malloc(sizeof(SEGMENT) * MAX_SEGMENTS);
  threads = malloc(sizeof(THREAD_INFO) * MAX_THREADS);
  global_roots = malloc(sizeof(char **) * MAX_GLOBAL_ROOTS);
  write_vector_size = (total_partition_pages * 
		       (BYTES_PER_PAGE / MIN_GROUP_SIZE));
  write_vector = malloc(write_vector_size);
  if ((pages == 0) || (groups == 0) || (segments == 0) || 
      (threads == 0) || (global_roots == 0) || (write_vector == 0)) {
    out_of_memory("Heap Memory tables", 0);
  }

  init_page_info();
  empty_pages = NULL;
  init_gc_thread();
  total_segments = 0;
  
  if ((static_size > 0) &&
      (allocate_segment(static_size, STATIC_SEGMENT) == 0)) {
    out_of_memory("Heap Memory Initialization", static_size/1024);
  }
  last_static_ptr = segments[0].last_segment_ptr;
  first_static_ptr = last_static_ptr;

  if (allocate_segment(first_segment_bytes, HEAP_SEGMENT) == 0) {
    out_of_memory("Heap Memory allocation", first_segment_bytes/1024);
  }
  
  marked_color = GENERATION0;
  unmarked_color = GENERATION1;
  init_group_info();
  memset(write_vector, 0, write_vector_size); // init write_vector
  init_realtime_gc();
}

void *rtalloc_start_thread(void *arg) {
  START_THREAD_ARGS *start_args = (START_THREAD_ARGS *) arg;
  void *(*real_start_func) (void *);

  // unpack start args and then free arg
  long thread_index = start_args->thread_index;
  real_start_func = start_args->real_start_func;
  char *real_args = start_args->real_args;
  free(arg);

  // setup code runs first
  pthread_t self;
  pthread_attr_t attr;
  void *stackaddr;
  size_t stacksize;
  
  printf("Thread %d started, live stack top is 0x%lx\n", 
	 thread_index, &thread_index);
  self = pthread_self();
  threads[thread_index].pthread = self;
  pthread_getattr_np(self, &attr);
  pthread_attr_getstack(&attr, &stackaddr, &stacksize);
  // We don't really need this info, but it might be nice for debugging
  // stackaddr is the LOWEST addressable byte of the stack
  // The stack pointer starts at stackaddr + stacksize!
  printf("Stackaddr is %p\n", stackaddr);
  printf("Stacksize is %x\n", stacksize);
  threads[thread_index].stack_base = stackaddr;
  threads[thread_index].stack_size = stacksize;
  threads[thread_index].stack_bottom = (char *)  &stacksize;
  fflush(stdout);

  if (0 != pthread_setspecific(thread_index_key, (void *) thread_index)) {
    printf("pthread_setspecific failed!\n"); 
  } else {
    // initializing saved_stack_base tells new_thread
    // that stack setup is done and it can return
    threads[thread_index].saved_stack_base = SXbig_malloc(stacksize);
    // after setup, invoke the real start func with the real args
    (real_start_func)(real_args);
  }
}

int new_thread(void *(*start_func) (void *), void *args) {
  pthread_t thread;

  pthread_mutex_lock(&total_threads_lock);
  if (total_threads < MAX_THREADS) {
    // HEY! this isn't thread safe! need a mutex for total_threads
    int index = total_threads;
    total_threads = total_threads + 1;
    pthread_mutex_unlock(&total_threads_lock);

    START_THREAD_ARGS *start_args = malloc(sizeof(START_THREAD_ARGS));
    start_args->thread_index = index;
    start_args->real_start_func = start_func;
    start_args->real_args = args;

    // this indicates that thread setup isn't complete
    threads[index].saved_stack_base = 0;
    if (0 != pthread_create(&thread, 
			    NULL, 
			    rtalloc_start_thread,
			    (void *) start_args)) {
      Debugger("pthread_create failed!\n");
    } else {
      // HEY! should do something smarter than busy wait
      // rtalloc_start_thread to complete thread init
      while (0 == threads[index].saved_stack_base) {
	// YOW! without this explicit sched_yield(), we hang in this
	// loop when compiled with -O1 and-O2
	sched_yield();
      }
      return(index);
    }
  } else {
    pthread_mutex_lock(&total_threads_lock);
    out_of_memory("Too many threads", MAX_THREADS);
  }
}

    

	
      
      

  
       
       



