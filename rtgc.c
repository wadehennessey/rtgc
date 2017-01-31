/*
 * Copyright 2017 Wade Lawrence Hennessey
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

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
#include "info-bits.h"
#include "mem-internals.h"
#include "allocate.h"

struct timeval max_flip_tv, total_flip_tv;

static
void RTmake_object_gray(GCPTR current) {
  GPTR group = PTR_TO_GROUP(current);
  GCPTR prev = GET_LINK_POINTER(current->prev);
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
      group->last = current;
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
  DEBUG(group->white_count = group->white_count - 1);
}

static inline int valid_interior_ptr(GCPTR gcptr, BPTR interior_ptr) {
  long delta = interior_ptr - (BPTR) gcptr;
  return(delta < (INTERIOR_PTR_RETENTION_LIMIT + sizeof(GC_HEADER)));
}

static inline GCPTR interior_to_gcptr_3(BPTR ptr, PPTR page, GPTR group) {
  GCPTR gcptr;
  if (group->size >= BYTES_PER_PAGE) {
    gcptr = page->base;
  } else {
    // This only works because first_partition_ptr is BYTES_PER_PAGE aligned 
    gcptr = (GCPTR) ((long) ptr & (-1 << group->index));
  }
  return(gcptr);
}

static inline GCPTR interior_to_gcptr(BPTR ptr) {
  PPTR page = pages + PTR_TO_PAGE_INDEX(ptr);
  GPTR group = page->group;
  GCPTR gcptr;

  if (group > EXTERNAL_PAGE) {
    if (group->size >= BYTES_PER_PAGE) {
      gcptr = page->base;
    } else {
      // This only works because first_partition_ptr is BYTES_PER_PAGE aligned 
      gcptr = (GCPTR) ((long) ptr & (-1 << group->index));
    }
  } else {
    Debugger("ERROR! Found IN_HEAP pointer with NULL group!\n");
  }
  // FIX me - gcptr can be returned uninitailzed.
  return(gcptr);
}

void RTtrace_pointer(void *ptr) {
  if (IN_PARTITION(ptr)) {
    PPTR page = pages + PTR_TO_PAGE_INDEX(ptr);
    GPTR group = page->group;
    if (group > EXTERNAL_PAGE) {
      GCPTR gcptr = interior_to_gcptr_3(ptr, page, group);
      if (WHITEP(gcptr) && valid_interior_ptr(gcptr, ptr)) {
	RTmake_object_gray(gcptr);
      }
    }
  }
}

// Slightly shorter trace that skips partition check for ptrs known
// to point into the heap
void RTtrace_heap_pointer(void *ptr) {
  GCPTR gcptr = interior_to_gcptr(ptr);
  if (WHITEP(gcptr)) {
    RTmake_object_gray(gcptr);
  }
}

// Scan memory to trace *possible* pointers
static
void scan_memory_segment(BPTR low, BPTR high) {
  for (BPTR next = low; next < high; next = next + GC_POINTER_ALIGNMENT) {
    BPTR ptr = *((BPTR *) next);
    if (IN_PARTITION(ptr)) {
      PPTR page = pages + PTR_TO_PAGE_INDEX(ptr);
      GPTR group = page->group;
      if (group > EXTERNAL_PAGE) {
	GCPTR gcptr = interior_to_gcptr_3(ptr, page, group);
	if (WHITEP(gcptr) && valid_interior_ptr(gcptr, ptr)) {
	  RTmake_object_gray(gcptr);
	}
      }
    }
  }
}

static
void scan_memory_segment_with_metadata(BPTR low, BPTR high) {
  LPTR last_ptr = (LPTR) high - 1;
  RT_METADATA *md = (RT_METADATA *) *last_ptr;
  long size = *md;
  long length = high - low - sizeof(RT_METADATA *);
  long count = length / size;

  for (int i = 0; i < count; i++) {
    BPTR offset = low + (i * size);
    for (int j = 1; md[j] != -1; j++) {
      BPTR ptr = *((BPTR *) (offset + md[j]));
      if (IN_PARTITION(ptr)) {
	PPTR page = pages + PTR_TO_PAGE_INDEX(ptr);
	GPTR group = page->group;
	if (group > EXTERNAL_PAGE) {
	  GCPTR gcptr = interior_to_gcptr_3(ptr, page, group);
	  if (WHITEP(gcptr) && valid_interior_ptr(gcptr, ptr)) {
	    RTmake_object_gray(gcptr);
	  }
	}
      }
    }
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
	    RTmake_object_gray(gcptr);
	  }
	  mask = ~mask;
	  // Must clear only the bit we just found set.
	  // Clearing entire long at end of bit scan
	  // creates a race condition with mark_write_vector.
	  locked_long_and(RTwrite_vector + index, mask);
	}
      }
    }
  }
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
	RTmake_object_gray(gcptr);
      }
    }
  }
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
      if (WHITEP(gcptr) && valid_interior_ptr(gcptr, object)) {
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
    for (BPTR next = low; next < high; next = next + GC_POINTER_ALIGNMENT) {
      BPTR object = *((BPTR *) next);
      if (IN_HEAP(object)) {
	GCPTR gcptr = interior_to_gcptr(object); 
	if (WHITEP(gcptr) && valid_interior_ptr(gcptr, object)) {
	  mark_write_vector(gcptr);
	}
      }
    }
  }
}

void *RTmemcpy(void *p1, void *p2, int num_bytes) {
  memory_segment_write_barrier(p1, (BPTR) p1 + num_bytes);
  memcpy(p1, p2, num_bytes);
  return(p1);
}

void *RTrecordcpy(void *p1, void *p2, int num_bytes) {
  RTmemcpy(p1, p2, num_bytes);
}

void *RTmemset(void *p1, int data, int num_bytes) {
  memory_segment_write_barrier(p1, (BPTR) p1 + num_bytes);
  memset(p1, data, num_bytes);
  return(p1);
}

static
void scan_saved_registers(int i) {
  // HEY! just scan saved regs that need it, not all 23 of them
  BPTR registers = (BPTR) saved_threads[i].registers;
  scan_memory_segment(registers, registers + (23 * sizeof(long)));
}

static
void scan_saved_stack(int i) {
  BPTR top = (BPTR) saved_threads[i].saved_stack_base;
  BPTR bottom = top + saved_threads[i].saved_stack_size;
  BPTR ptr_aligned_top = (BPTR) ((long) top & ~(GC_POINTER_ALIGNMENT - 1));
  scan_memory_segment(ptr_aligned_top, bottom);
}

static
void scan_saved_thread_state(int i) {
  scan_saved_registers(i);
  scan_saved_stack(i);
}

static
void scan_threads() {
  for (int i = 0; i < total_saved_threads; i++) {
    scan_saved_thread_state(i);
  }  

  // move this to it's own function
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
      PPTR page = pages + PTR_TO_PAGE_INDEX(ptr);
      GPTR group = page->group;
      if (group > EXTERNAL_PAGE) {
	GCPTR gcptr = interior_to_gcptr_3(ptr, page, group); 
	if (WHITEP(gcptr) && valid_interior_ptr(gcptr, ptr)) {
	  RTmake_object_gray(gcptr);
	}
      }
    }
  }
}

static
void scan_static_space() {
  BPTR next = first_static_ptr;
  // Ok to not acquire lock or copy at flip time???
  // might scan uninitalized object or more than needed this way
  // problaby safer to set static_frontier_at_flip in rtstop.c
  BPTR end = static_frontier_ptr;
  while (next < end) {
    BPTR low = next + sizeof(long);
    int size = *((long *) next);
    size = size >> LINK_INFO_BITS;
    next = low + size;
    GCPTR gcptr = (GCPTR) (low - sizeof(GC_HEADER));
    scan_object(gcptr, size + sizeof(GC_HEADER));
  }
}

static int total_root_scanners = 0;
static void (*root_scanners[10])();

static int total_custom_scanners = 0;
static void (*custom_scanners[5])(void *low, void *high);

void RTregister_root_scanner(void (*root_scanner)()) {
  root_scanners[0] = root_scanner;
  total_root_scanners = total_root_scanners + 1;
}

int RTregister_custom_scanner(void (*custom_scanner)(void *low, void *high)) {
  custom_scanners[0] = custom_scanner;
  total_custom_scanners = total_custom_scanners + 1;
  return(SC_CUSTOM1);
}

// HEY! Generalize this to allow more than 1 no_write_barrier state
// to be registered.
void RTregister_no_write_barrier_state(void *start, int len) {
  RTno_write_barrier_state_ptr = start;
}

static
void scan_root_set() {
  scan_threads();
  scan_global_roots();
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
  case SC_CUSTOM1:
    (*custom_scanners[0])(low, high);
    break;
  case SC_METADATA:
    scan_memory_segment_with_metadata(low, high);
    break;
  default: Debugger(0);
  }
}

static
void scan_object_with_group(GCPTR ptr, GPTR group) {
  scan_object(ptr, group->size);
  SET_COLOR(ptr,marked_color);
  group->black = ptr;
  DEBUG(group->black_scanned_count = group->black_scanned_count + 1);
}

// HEY! Fix this up now that it's not continuation based.
static
void scan_gray_set() {
  int i, scan_count, rescan_all_groups;

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
}

static
void flip() {
  assert(0 == enable_write_barrier);
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
      GCPTR last = group->last;
      if (last != NULL) {
	SET_LINK_POINTER(last->next,NULL); // end black set
      }
      group->last = NULL;
    }

    // used to handle this with:
    // group->white = (GREENP(black) ? NULL : black)
    if (group->black == group->free) {
      // Must have no retained objects, we have nothing
      // available to retain this cycle
      group->white = NULL;
    } else {
      group->white = group->black;
    }
    group->black = group->free;
    group->gray = NULL;

    group->white_count = group->black_scanned_count + group->black_alloc_count;
    assert(group->white_count >= 0);

    group->black_scanned_count = 0;
    group->black_alloc_count = 0;
  }

  stop_all_mutators_and_save_state();
}

// The alloc counterpart to this function is init_pages_for_group.
// We need to change garbage color to green now so conservative
// scanning in a later gc cycle doesn't start making free objects 
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
  }
  if (count != group->white_count) { 
    DEBUG(printf("group->white_count is %d, actual count is %d\n", 
		 group->white_count, count));
    DEBUG(Debugger("group->white_count doesn't equal actual count\n"));
  }

  if (last != NULL) {
    SET_LINK_POINTER(last->next, NULL);

    if (group->free == NULL) {
      group->free = group->white;
    }

    if (group->black == NULL) {
      group->black = group->white;
    }
    
    
    if (group->last != NULL) {
      SET_LINK_POINTER((group->last)->next, group->white);
    }
    SET_LINK_POINTER((group->white)->prev, group->last);
    group->last = last;
  }
  group->white = NULL;
  group->white_count = 0; // no lock needed, white_count is gc only
  pthread_mutex_unlock(&(group->free_lock));
}

static 
void recycle_all_garbage() {
  assert(0 == enable_write_barrier);
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    recycle_group_garbage(&groups[i]);
  }
  coalesce_all_free_pages();
}

static
void full_gc() {
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

  gc_count = gc_count + 1;
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
  // The gc_flip signal_handler uses this to find the thread corresponding to
  // the mutator pthread it is running on
  if (0 != pthread_key_create(&thread_key, NULL)) {
    Debugger("thread_key create failed!\n");
  }

  printf("Running last commit before t1/t2 branch creation\n");
  printf("Page size is %d\n", BYTES_PER_PAGE);
  printf((RTatomic_gc ? "***ATOMIC GC***\n" : "***REAL-TIME GC***\n"));
#ifdef NDEBUG
  printf("NDEBUG is defined\n");
#endif
  total_global_roots = 0;
  gc_count = 0;
  pthread_mutex_init(&threads_lock, NULL);
  pthread_mutex_init(&global_roots_lock, NULL);
  pthread_mutex_init(&empty_pages_lock, NULL);
  pthread_mutex_init(&static_frontier_ptr_lock, NULL);
  sem_init(&gc_semaphore, 0, 0);
  init_signals_for_rtgc();
  timerclear(&max_flip_tv);
  timerclear(&total_flip_tv);
}
