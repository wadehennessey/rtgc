// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

// Real time garbage collector running on one or more threads/cores

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include "mem-config.h"
#include "infoBits.h"
#include "mem-internals.h"
#include "vizmem.h"
#include "allocate.h"

volatile long gc_count;
double total_gc_time_in_cycle;
double max_increment_in_cycle;
double total_write_barrier_time_in_cycle;
struct timeval start_gc_cycle_time;
double last_cycle_ms;
double last_gc_ms;
double last_write_barrier_ms;
struct timeval max_flip_tv, total_flip_tv;


static int verify_count = 0;

static
void verify_white_count(GPTR group) {
  GCPTR ptr = group->white;
  int count = 0;
  while (ptr != NULL) {
    if (group->size < BYTES_PER_PAGE) {
      if ((((long) (GET_LINK_POINTER(ptr->prev)) % group->size) != 0) ||
	  (((long) (GET_LINK_POINTER(ptr->next)) % group->size) != 0)) {
	Debugger("Bad gchdr\n");
      }
    }
    ptr = GET_LINK_POINTER(ptr->next);
    count = count + 1;
  }
  if (group->white_count != count) {
    Debugger("incorrect white_count\n");
  } else {
    verify_count = verify_count + 1;
    //printf("Verify_white_count passed! %d\n", verify_count);
  }
}

static
void verify_white_counts() {
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    GPTR group = &groups[i];
    verify_white_count(group);
  }
}

static
void RTmake_object_gray(GCPTR current, BPTR raw) {
  GPTR group = PTR_TO_GROUP(current);
  BPTR header = (BPTR) current + sizeof(GC_HEADER);
  long delta = raw - header;
  
  if ((delta < INTERIOR_PTR_RETENTION_LIMIT) || 
      (((long) raw) == -1) || (raw == header)) {
    GCPTR prev = GET_LINK_POINTER(current->prev);
    assert(prev != (GCPTR) 0x80);
    GCPTR next = GET_LINK_POINTER(current->next);

    // Remove current from WHITE space
    if (current == group->white) {
      group->white = next;
    }
    if (prev != NULL) {
      SET_LINK_POINTER(prev->next, next);
    }
    if (next != NULL) {
      SET_LINK_POINTER(next->prev, prev);
    }

    // Link current onto the end of the gray set. This give us a breadth
    // first search when scanning the gray set (not that it matters)
    SET_LINK_POINTER(current->prev, NULL);
    GCPTR gray = group->gray;
    if (gray == NULL) {
      pthread_mutex_lock(&(group->black_and_last_lock));
      SET_LINK_POINTER(current->next, group->black);
      if (group->black == NULL) {
	assert(NULL == group->free);
	group->black = current;
	group->free_last = current;
	pthread_mutex_unlock(&(group->black_and_last_lock));
      } else {
	pthread_mutex_unlock(&(group->black_and_last_lock));
	// looks like a race with alloc setting color on 
	// black->prev. Should use a more specific lock than free.
	WITH_LOCK(group->free_lock,
		  SET_LINK_POINTER((group->black)->prev, current););
      }
    } else {
      SET_LINK_POINTER(current->next, gray);
      SET_LINK_POINTER(gray->prev, current);
    }
    assert(WHITEP(current));
    SET_COLOR(current, GRAY);
    group->gray = current;
    assert(group->white_count > 0); // no lock needed, white_count is gc only
    group->white_count = group->white_count - 1;
  }
}


// make this inline too
static GCPTR interior_to_gcptr_with_group(BPTR ptr, GPTR group) {
  Debugger("write me!");
}

// GCC can't handle inline functions which call inline functions I think
// HEY! Make this an inline function for speed
// and at least pass in the group ptr!
static GCPTR interior_to_gcptr(BPTR ptr) {
  PPTR page = &pages[PTR_TO_PAGE_INDEX(ptr)];
  GPTR group = page->group;
  GCPTR gcptr;

  if (group > EXTERNAL_PAGE) {
    if (group->size >= BYTES_PER_PAGE) {
      gcptr = page->base;
    } else {
      // This only works because first_partition_ptr is BYTES_PER_Page aligned 
      gcptr = (GCPTR) ((long) ptr & (-1 << group->index));
    }
  } else {
    printf("ERROR! Found IN_HEAP pointer with NULL group!\n");
  }
  return(gcptr);
}

// Scan memory looking for *possible* pointers
static
void scan_memory_segment(BPTR low, BPTR high) {
  // if GC_POINTER_ALIGNMENT is < 4, avoid scanning potential pointers that
  // extend past the end of this object
  high = high - sizeof(LPTR) + 1;
  for (BPTR next = low; next < high; next = next + GC_POINTER_ALIGNMENT) {
    MAYBE_YIELD;
    BPTR ptr = *((BPTR *) next);
    if (IN_PARTITION(ptr)) {
      int page_index = PTR_TO_PAGE_INDEX(ptr);
      GPTR group = pages[page_index].group;
      if (group > EXTERNAL_PAGE) {
	GCPTR gcptr = interior_to_gcptr(ptr);
	if WHITEP(gcptr) {
	    RTmake_object_gray(gcptr, ptr);
	}
      } else {
	if (VISUAL_MEMORY_ON && (group == EMPTY_PAGE)) {
	  RTupdate_visual_fake_ptr_page(page_index);
	}
      }
    }
  }
}

extern void *wcl_get_closure_env(void *ptr);

static
void scan_memory_segment_with_metadata(BPTR low, BPTR high, RT_METADATA *md) {
  BPTR env = wcl_get_closure_env(low + sizeof(long));
  //printf("closure env is %p\n", env);
  GCPTR gcptr = interior_to_gcptr(env); 
  if WHITEP(gcptr) {
      RTmake_object_gray(gcptr, env);
    }
}

// Public version
void RTscan_memory_segment(BPTR low, BPTR high) {
  scan_memory_segment(low, high);
}

#if USE_BIT_WRITE_BARRIER
static
int scan_write_vector() {
  int mark_count = 0;
  for (long index = 0; index < RTwrite_vector_length; index++) {
    if (0 != RTwrite_vector[index]) {
      BPTR base_ptr = first_partition_ptr + 
	(index * MIN_GROUP_SIZE * BITS_PER_LONG);
      for (long bit = 0; bit < BITS_PER_LONG; bit = bit + 1) {
	unsigned long mask = 1L << bit;
	if (0 != (RTwrite_vector[index] & mask)) {
	  GCPTR gcptr = (GCPTR) (base_ptr + (bit * MIN_GROUP_SIZE));
	  mark_count = mark_count + 1;
	  if (WHITEP(gcptr)) {
	    RTmake_object_gray(gcptr, (BPTR) -1);
	  }
	  mask = ~mask;
	  // Must clear only the bit we just found set.
	  // Clearing entire long at end of bit scan
	  // creates a race condition with the mark_RTwrite_vector.
	  locked_long_and(RTwrite_vector + index, mask);
	}
      }
    }
  }
  //printf("mark_count is %d\n", mark_count);
  return(mark_count);
}

static
void mark_write_vector(GCPTR gcptr) {
  long ptr_offset = ((BPTR) gcptr - first_partition_ptr);
  long long_index = ptr_offset / (MIN_GROUP_SIZE * BITS_PER_LONG);
  int bit = (ptr_offset % (MIN_GROUP_SIZE * BITS_PER_LONG)) / MIN_GROUP_SIZE;
  unsigned long bit_mask = 1L << bit;
  assert(0 != bit_mask);
  locked_long_or(RTwrite_vector + long_index, bit_mask);
}
#else
static
int scan_write_vector() {
  int mark_count = 0;
  for (long index = 0; index < RTwrite_vector_length; index++) {
    if (1 == RTwrite_vector[index]) {
      GCPTR gcptr = (GCPTR) (first_partition_ptr + (index * MIN_GROUP_SIZE));
      RTwrite_vector[index] = 0;
      mark_count = mark_count + 1;
      if (WHITEP(gcptr)) {
	RTmake_object_gray(gcptr, (BPTR) -1);
      }
    }
  }
  //printf("mark_count is %d\n", mark_count);
  return(mark_count);
}

static
void mark_write_vector(GCPTR gcptr) {
  long index = ((BPTR) gcptr - first_partition_ptr) / MIN_GROUP_SIZE;
  RTwrite_vector[index] = 1;
}
#endif

// Snapshot-at-gc-start write barrier.
// This is really just a version of scan_memory_segment on a single pointer.
// It marks the RTwrite_vector instead of immediately making white 
// objects become gray.
void *RTwrite_barrier(void *lhs_address, void *rhs) {
  if (enable_write_barrier) {
    BPTR object = *((BPTR *) lhs_address);
    if (IN_HEAP(object)) {
      GCPTR gcptr = interior_to_gcptr(object); 
      if WHITEP(gcptr) {
	  mark_write_vector(gcptr);
	}
    }
  }
  return((void *) (*(LPTR)lhs_address = (long) rhs));
}

void *RTsafe_bash(void * lhs_address, void * rhs) {
  BPTR object;
  GCPTR gcptr;

  if (CHECK_BASH) {
    object = *((BPTR *) lhs_address);
    if (IN_HEAP(object)) {
      gcptr = interior_to_gcptr(object); 
      if WHITEP(gcptr) {
	  Debugger("White object is escaping write_barrier!\n");
      }
    }
  }
  return((void *) (*(LPTR)lhs_address = (long) rhs));
}

void *RTsafe_setfInit(void * lhs_address, void * rhs) {
  if (CHECK_SETFINIT) {
    BPTR object = *((BPTR *) lhs_address);
    if (object != NULL) {
      // if ((int) object != rhs)
      Debugger("RTsafe_setfInit problem\n");
    }
  }
  return((void *) (* (LPTR) lhs_address = (long) rhs));
}

// This is just a version of scan_memory_segment that marks the 
// write_vector instead of immediately making white objects become gray.
void memory_segment_write_barrier(BPTR low, BPTR high) {
  Debugger("HEY! I haven't been tested!\n");
  if (enable_write_barrier) {
    // if GC_POINTER_ALIGNMENT is < 4, avoid scanning potential pointers that
    // extend past the end of this object
    high = high - sizeof(LPTR) + 1;
    for (BPTR next = low; next < high; next = next + GC_POINTER_ALIGNMENT) {
      BPTR object = *((BPTR *) next);
      if (IN_HEAP(object)) {
	GCPTR gcptr = interior_to_gcptr(object); 
	if WHITEP(gcptr) {
	    mark_write_vector(gcptr);
	  }
      }
    }
  }
}

void *ptrcpy(void *p1, void *p2, int num_bytes) {
  memory_segment_write_barrier(p1, (BPTR) p1 + num_bytes);
  memcpy(p1, p2, num_bytes);
  return(p1);
}

void *ptrset(void *p1, int data, int num_bytes) {
  memory_segment_write_barrier(p1, (BPTR) p1 + num_bytes);
  memset(p1, data, num_bytes);
  return(p1);
}

static
void scan_thread_registers(int thread) {
  // HEY! just scan saved regs that need it, not all 23 of them
  BPTR registers = (BPTR) threads[thread].registers;
  scan_memory_segment(registers, registers + (23 * sizeof(long)));
}

static
void scan_thread_saved_stack(int thread) {
  BPTR top = (BPTR) threads[thread].saved_stack_base;
  BPTR bottom = top + threads[thread].saved_stack_size;
  BPTR ptr_aligned_top = (BPTR) ((long) top & ~(GC_POINTER_ALIGNMENT - 1));
  scan_memory_segment(ptr_aligned_top, bottom);
}

static
void scan_thread(int thread) {
  scan_thread_registers(thread);
  scan_thread_saved_stack(thread);
}

static
void scan_threads() {
  // HEY! need to pass along number of threads scanned
  // Acquire total_threads lock here? Maybe earlier in flip.
  for (int next_thread = 1; next_thread < total_threads; next_thread++) {
    scan_thread(next_thread);
  }
  if (0 != saved_no_write_barrier_state) {
    BPTR low =  (BPTR) &saved_no_write_barrier_state;;
    BPTR high = ((BPTR) low + sizeof(long));
    scan_memory_segment(low, high);
  }
}
  
static
void scan_global_roots() {
  for (int i = 0; i < total_global_roots; i++) {
    BPTR ptr =  *((BPTR *) *(global_roots + i));
    if (IN_PARTITION(ptr)) {
      int page_index = PTR_TO_PAGE_INDEX(ptr);
      GPTR group = pages[page_index].group;
      if (group > EXTERNAL_PAGE) {
	GCPTR gcptr = interior_to_gcptr(ptr); 
	if WHITEP(gcptr) {
	    RTmake_object_gray(gcptr, ptr);
	  }
      } else {
	if (VISUAL_MEMORY_ON && (group == EMPTY_PAGE)) {
	  RTupdate_visual_fake_ptr_page(page_index);
	}
      }
    }
  }
}

static
void scan_static_space() {
  BPTR next = first_static_ptr;
  BPTR end = last_static_ptr;
  while (next < end) {
    int size = *((int *) next);
    BPTR low = next + sizeof(GCPTR);
    size = size >> LINK_INFO_BITS;
    next = low + size;
    GCPTR gcptr = (GCPTR) (low - sizeof(GC_HEADER));
    scan_object(gcptr, size + sizeof(GC_HEADER));
    /* Delete ME! HEY! Convert to common scanner with scan_object 
       if (GET_STORAGE_CLASS(gcptr) != SC_NOPOINTERS) {
       scan_memory_segment(low,next);
       } */
  }
}

static int total_root_scanners = 0;
static void (*root_scanners[10])();

void RTregister_root_scanner(void (*root_scanner)()) {
  root_scanners[0] = root_scanner;
  total_root_scanners = total_root_scanners + 1;
}

void RTregister_no_write_barrier_state(void *start, int len) {
  RTno_write_barrier_state_ptr = start;
}

static
void scan_root_set() {
  last_gc_state = "Scan Threads";
  UPDATE_VISUAL_STATE();
  scan_threads();
  last_gc_state = "Scan Globals";
  UPDATE_VISUAL_STATE();
  scan_global_roots();
  last_gc_state = "Scan Statics";
  UPDATE_VISUAL_STATE();
  scan_static_space();
  for (int i = 0; i < total_root_scanners; i++) {
    (*root_scanners[i])();
  }
}

void scan_object(GCPTR ptr, int total_size) {
  BPTR bptr, low, high;

  bptr = (BPTR) ptr;
  low = bptr + sizeof(GC_HEADER);
  high = bptr + total_size;
  switch (GET_STORAGE_CLASS(ptr)) {
  case SC_NOPOINTERS: break;
  case SC_POINTERS:
    scan_memory_segment(low, high);
    break;
  case SC_METADATA:
    scan_memory_segment_with_metadata(low, high, 0);
    break;
  default: Debugger(0);
  }
}

static
void scan_object_with_group(GCPTR ptr, GPTR group) {
  scan_object(ptr, group->size);
  WITH_LOCK(group->black_count_lock,
	    SET_COLOR(ptr,marked_color);
	    group->black_count = group->black_count + 1;
	    group->black = ptr;);
}

// HEY! Fix this up now that it's not continuation based.
static
void scan_gray_set() {
  int i, scan_count, rescan_all_groups;

  last_gc_state = "Scan Gray Set";
  UPDATE_VISUAL_STATE();
  i = MIN_GROUP_INDEX;
  scan_count = 0;
  do {
    while (i <= MAX_GROUP_INDEX) {
      GPTR group = &groups[i];
      GCPTR current = group->black;
      // current could be gray, black, or green
      if ((current != NULL ) && (!(GRAYP(current)))) {
	current = GET_LINK_POINTER(current->prev);
      }
      while (current != NULL) {
	MAYBE_YIELD;
	scan_object_with_group(current,group);
	scan_count = scan_count + 1;
	current = GET_LINK_POINTER(current->prev);
      }
      i = i + 1;
    }
    if (scan_count > 0) {
      rescan_all_groups = 1;
      i = MIN_GROUP_INDEX;
      scan_count = 0;
    } else {
      rescan_all_groups = 0;
    }
  } while (rescan_all_groups == 1);
  MAYBE_YIELD;
}

static
void lock_all_free_locks() {
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    GPTR group = &groups[i];
    pthread_mutex_lock(&(group->free_lock));
  }
}

static
void unlock_all_free_locks() {
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    GPTR group = &groups[i];
    pthread_mutex_unlock(&(group->free_lock));
  }
}
  
static
void flip() {
  MAYBE_YIELD;
  // Originally at this point all mutator threads are stopped, and none of
  // them is in the middle of an RTallocate. We got this for free by being
  // single threaded and implicity locking by yielding only when we chose to.
  assert(0 == enable_write_barrier);
  last_gc_state = "Flip";
  // No allocation allowed during a flip
  lock_all_free_locks();
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    GPTR group = &groups[i];
    GCPTR free = group->free;
    if (free != NULL) {
      GCPTR prev = GET_LINK_POINTER(free->prev);
      if (prev != NULL) {
	SET_LINK_POINTER(prev->next,NULL); // end black set
      }
      SET_LINK_POINTER(free->prev,NULL);
    } else {
      GCPTR free_last = group->free_last;
      if (free_last != NULL) {
	SET_LINK_POINTER(free_last->next,NULL); // end black set
      }
      group->free_last = NULL;
    }

    group->white = group->black;
    group->black = group->free;
    group->gray = NULL;

    assert(group->black_count >= 0); 
    group->white_count = group->black_count;
    group->black_count = 0;
  }

  assert(0 == enable_write_barrier);
  // We do this in rtstop.c now. Should we move it back here? It shouldn't
  // matter because we hold all free locks in each place
  //enable_write_barrier = 1;
  //SWAP(marked_color,unmarked_color);

  struct timeval start_tv, end_tv, flip_tv;
  gettimeofday(&start_tv, 0);
  stop_all_mutators_and_save_state();
  gettimeofday(&end_tv, 0);

  timersub(&end_tv, &start_tv, &flip_tv);
  timeradd(&total_flip_tv, &flip_tv, &total_flip_tv);
  if timercmp(&flip_tv, &max_flip_tv, >) {
      max_flip_tv = flip_tv;
      /*      printf("max_flip_tv is %d.%06d, avg is %f, saved stack is %d bytes\n", 
	     max_flip_tv.tv_sec, 
	     max_flip_tv.tv_usec,
	     ((total_flip_tv.tv_sec * 1.0) + 
	      (total_flip_tv.tv_usec / 1000000.0))
	     / (gc_count + 1), 
	     threads[1].saved_stack_size);
      */
  }
}

// The alloc counterpart to this function is init_pages_for_group.
// We need to change garbage color to green now so conservative
// scanning in the next gc cycle doesn't start making free objects 
// that look white turn gray!
static
void recycle_group_garbage(GPTR group) {
  int count = 0;
  GCPTR last = NULL;
  GCPTR next = group->white;

  pthread_mutex_lock(&(group->free_lock));
  while (next != NULL) {
    // Finalize code was here. Need to add it back

    SET_COLOR(next,GREEN);
    if (DETECT_INVALID_REFS) {
      memset((BPTR) next + sizeof(GC_HEADER), 
	     INVALID_ADDRESS,
	     (group->size - sizeof(GC_HEADER)));
    }
    last = next;
    next = GET_LINK_POINTER(next->next);
    count = count + 1;
    MAYBE_YIELD;
  }

  if (count != group->white_count) { // no lock needed, white_count is gc only
    //verify_all_groups();
    printf("group->white_count is %d, actual count is %d\n", 
	   group->white_count, count);
    Debugger("group->white_count doesn't equal actual count\n");
  }

  if (last != NULL) {
    SET_LINK_POINTER(last->next, NULL);

    if (group->free == NULL) {
      group->free = group->white;
    }

    /*
    if (group->black == NULL) {
      group->black = group->white;
    }
    */
    
    if (group->free_last != NULL) {
      SET_LINK_POINTER((group->free_last)->next, group->white);
    }
    SET_LINK_POINTER((group->white)->prev, group->free_last);
    group->free_last = last;
    group->green_count = group->green_count + count;
  }
  group->white = NULL;
  group->white_count = 0; // no lock needed, white_count is gc only
  pthread_mutex_unlock(&(group->free_lock));
}

static 
void recycle_all_garbage() {
  last_gc_state = "Recycle Garbage";
  UPDATE_VISUAL_STATE();
  assert(0 == enable_write_barrier);
  //verify_all_groups();
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    recycle_group_garbage(&groups[i]);
  }
  coalesce_all_free_pages();
}

static 
void reset_gc_cycle_stats() {
  //total_allocation_this_cycle = 0;
  total_gc_time_in_cycle = 0.0;
  total_write_barrier_time_in_cycle = 0.0;
  max_increment_in_cycle = 0.0;
  if (ENABLE_GC_TIMING) {
    gettimeofday(&start_gc_cycle_time, 0);
  }
}

static 
void summarize_gc_cycle_stats() {
  double total_cycle_time;
  
  if (ENABLE_GC_TIMING) {
    ELAPSED_MILLISECONDS(start_gc_cycle_time, total_cycle_time);
    last_cycle_ms = total_cycle_time;
    last_gc_ms = total_gc_time_in_cycle;
    last_write_barrier_ms = total_write_barrier_time_in_cycle;
  }
  if (VISUAL_MEMORY_ON) RTdraw_visual_gc_stats();
}

static
void full_gc() {
  //reset_gc_cycle_stats();
  flip();
  assert(1 == enable_write_barrier);
  scan_root_set();

  int mark_count = 0;
  do {
    scan_gray_set();
    mark_count = scan_write_vector();
  } while (mark_count > 0);

  enable_write_barrier = 0;
  recycle_all_garbage();
  // moved this into stop_all_mutators_and_save_state.
  // enable_write_barrier = 1;

  gc_count = gc_count + 1;
  //summarize_gc_cycle_stats();
  //last_gc_state = "Cycle Complete";
  //UPDATE_VISUAL_STATE();
}

void RTfull_gc() {
  full_gc();
}

void rtgc_loop() {
  while (1) {
    if (1 == RTatomic_gc) while (0 == run_gc);
    full_gc();
    full_gc();
    if (0 == (gc_count % 5000)) {
      //printf("gc end - gc_count %d\n", gc_count);
      //fflush(stdout);
    }
    if (1 == RTatomic_gc) run_gc = 0;
  }
}

int rtgc_count(void) {
  return(gc_count);
}

void init_realtime_gc() {
  // the gc_flip signal handler uses this to find the thread_index of 
  // the mutator thread it is running on
  if (0 != pthread_key_create(&thread_index_key, NULL)) {
    printf("thread_index_key create failed!\n");
  }

  printf("Page size is %d\n", BYTES_PER_PAGE);
  printf((RTatomic_gc ? "***ATOMIC GC***\n" : "***REAL-TIME GC***\n"));
  total_global_roots = 0;
  gc_count = 0;
  visual_memory_on = 0;
  last_gc_state = "<initial state>";
  pthread_mutex_init(&total_threads_lock, NULL);
  pthread_mutex_init(&empty_pages_lock, NULL);
  sem_init(&gc_semaphore, 0, 0);
  init_signals_for_rtgc();
  timerclear(&max_flip_tv);
  timerclear(&total_flip_tv);
}
