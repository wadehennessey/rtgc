/* Real time storage garbage collector running on one or more threads/cores */

/* omitted .h files that don't exist */

#define THREAD_CALLBACK
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>
#include "compat.h"
#include "mem-config.h"
#include "infoBits.h"
#include "mem-internals.h"
#include "vizmem.h"
#include "allocate.h"

/* Global GC variables follow. We do NOT want GC info containing
   heap pointers in the global data section, or the GC will
   mistake them for mutator pointers and save them! Hence we
   malloc some structures */

int gc_count;
int next_thread; 		/* HEY! get rid of this...  */

double total_gc_time_in_cycle;
double max_increment_in_cycle;
double total_write_barrier_time_in_cycle;
tock_type start_gc_cycle_tocks;

static
void remove_object_from_free_list(GPTR group, GCPTR object) {
  GCPTR prev, next;
  
  prev = GET_LINK_POINTER(object->prev);
  next = GET_LINK_POINTER(object->next);

  if (object == group->free) {
    group->free = next;
  }
  if (object == group->black) {
    group->black = next;
  }

  if (object == group->free_last) {
    group->free_last = ((next == NULL) ? prev : next);
  }

  if (prev != NULL) {
    SET_LINK_POINTER(prev->next, next);
  }
  if (next != NULL) {
    SET_LINK_POINTER(next->prev, prev);
  }

  group->green_count = group->green_count - 1;
  group->total_object_count = group->total_object_count - 1;
}

static
void convert_free_to_empty_pages(int first_page, int page_count) {
  int next_page_index = first_page;
  int end_page = first_page + page_count;
  HOLE_PTR new;

  /* Remove objects on pages from their respective free lists */
  while (next_page_index < end_page) {
    GPTR group = pages[next_page_index].group;
    int total_pages = MAX(1,group->size / BYTES_PER_PAGE);
    int object_count = (total_pages * BYTES_PER_PAGE) / group->size;
    int i;
    GCPTR next;

    next = (GCPTR) PAGE_INDEX_TO_PTR(next_page_index);
    for (i = 0; i < object_count; i++) {
      remove_object_from_free_list(group, next);
      next = (GCPTR) ((BPTR) next + group->size);
    }
    next_page_index = next_page_index + total_pages;
  }
  SXinit_empty_pages(first_page, page_count, HEAP_SEGMENT);
}

static
void coalesce_segment_free_pages(int segment) {
  int next_page_index, first_page_index, last_page_index, contig_count;

  first_page_index = -1;
  contig_count = 0;
  next_page_index = PTR_TO_PAGE_INDEX(segments[segment].first_segment_ptr);
  last_page_index = PTR_TO_PAGE_INDEX(segments[segment].last_segment_ptr);
  while (next_page_index < last_page_index) {
    GPTR group = pages[next_page_index].group;
    int total_pages = (group > (GPTR) 0) ?
                      MAX(1, group->size / BYTES_PER_PAGE) :
                      1;
    int count_free_page = (group != EMPTY_PAGE) &&
      (pages[next_page_index].bytes_used == 0);
    if (count_free_page) {
      if (first_page_index == -1) {
	first_page_index = next_page_index;
      }
      contig_count = contig_count + total_pages;
    }
    next_page_index = next_page_index + total_pages;
    if ((!count_free_page || next_page_index == last_page_index) &&
	(first_page_index != -1)) {
      convert_free_to_empty_pages(first_page_index, contig_count);
      first_page_index = -1;
      contig_count = 0;
    }
  }
}

static
void coalesce_all_free_pages() {
  int segment;

  for (segment = 0; segment < total_segments; segment++) {
    /* HEY! Put a MAYBE_PAUSE here */
    if (segments[segment].type == HEAP_SEGMENT) {
      coalesce_segment_free_pages(segment);
    }
  }
}

/* HEY! pass in group info! */
//static
int SXmake_object_gray(GCPTR current, BPTR raw) {
  GCPTR prev;
  GCPTR next;
  GCPTR black;
  GCPTR gray;
  GPTR group = PTR_TO_GROUP(current);
  BPTR header = (BPTR) current + sizeof(GC_HEADER);
  
  /* Only allow interior pointers to retain objects <= 1 page in size */
  if ((group->size <= INTERIOR_PTR_RETENTION_LIMIT) ||
      (((long) raw) == -1) || (raw == header)) {
    prev = GET_LINK_POINTER(current->prev);
    next = GET_LINK_POINTER(current->next);

    /* Remove current from WHITE space */
    if (current == group->white) {
      group->white = next;
    }
    if (prev != NULL) {
      SET_LINK_POINTER(prev->next, next);
    }
    if (next != NULL) {
      SET_LINK_POINTER(next->prev, prev);
    }

    /* Link current onto the end of the gray set. This give us a breadth
       first search when scanning the gray set (not that it matters) */
    SET_LINK_POINTER(current->prev, NULL);
    gray = group->gray;
    if (gray == NULL) {
      SET_LINK_POINTER(current->next, group->black);
      if (group->black == NULL) {
	group->black = current;
	group->free_last = current;
      } else {
	SET_LINK_POINTER((group->black)->prev, current);
      }
    } else {
      SET_LINK_POINTER(current->next, gray);
      SET_LINK_POINTER(gray->prev, current);
    }
    SET_COLOR(current, GRAY);
    group->gray = current;
    group->white_count = group->white_count - 1;
  }
  return(group->size);
}

/* Scan memory looking for *possible* pointers */
void scan_memory_segment(BPTR low, BPTR high) {
  BPTR next;
  BPTR ptr;
  GCPTR gcptr;
  int page_index;
  GPTR group;
  int len;

  len = high - low;
  /* if GC_POINTER_ALIGNMENT is < 4, avoid scanning potential pointers that
     extend past the end of this object */
  high = high - sizeof(LPTR) + 1;
  /* Why is next initialized twice??? delete first one */
  next = low;
  for (next = low; next < high; next = next + GC_POINTER_ALIGNMENT) {
    MAYBE_PAUSE_GC;
    ptr = *((BPTR *) next);
    if (IN_PARTITION(ptr)) {
      page_index = PTR_TO_PAGE_INDEX(ptr);
      group = pages[page_index].group;
      if (group > EXTERNAL_PAGE) {
	gcptr = interior_to_gcptr(ptr); /* Map it ourselves here! */
	if WHITEP(gcptr) {
	  SXmake_object_gray(gcptr, ptr); /* Pass in group info! */
	}
      } else {
	if (VISUAL_MEMORY_ON && (group == EMPTY_PAGE)) {
	  SXupdate_visual_fake_ptr_page(page_index);
	}
      }
    }
  }
}

/* Omitted inclusion of ObjStoreProtocol */
/* Omitted all metadata walking and related code */

static
void scan_memory_segment_with_metadata(BPTR low, BPTR high, MetaData *md) {
  scan_memory_segment(low, high);
}

/* Snapshot-at-gc-start write barrier.
   This is really just a specialized version of scan_memory_segment. */
void * SXwrite_barrier(void *lhs_address, void *rhs) {
  if (memory_mutex == 1) {
    printf("HEY! write_barrier called from withing GC!\n");
    Debugger();
  }
  if (enable_write_barrier) {
    BPTR object;
    GCPTR gcptr;

    if (ENABLE_VISUAL_MEMORY) START_CODE_TIMING;
    object = *((BPTR *) lhs_address);
    if (IN_HEAP(object)) {
      gcptr = interior_to_gcptr(object); 
      if WHITEP(gcptr) {
	SXmake_object_gray(gcptr, (BPTR) -1);
      }
    }
    if (ENABLE_VISUAL_MEMORY)
      END_CODE_TIMING(total_write_barrier_time_in_cycle);
  }
  return((void *) (*(LPTR)lhs_address = (long) rhs));
}

void * SXsafe_bash(void * lhs_address, void * rhs) {
  BPTR object;
  GCPTR gcptr;

  if (CHECK_BASH) {
    object = *((BPTR *) lhs_address);
    if (IN_HEAP(object)) {
      gcptr = interior_to_gcptr(object); 
      if WHITEP(gcptr) {
	if (memory_mutex == 1) {
	  printf("HEY! write_barrier called from within GC!\n");
	  /* Call Debugger, then
	     return((void *) (*lhs_address = rhs)); */
	}
	/* Debugger */
      }
    }
  }
  /* Why is the calling the write barrier instead of simply doing the
     assignment?? */
  return(SXwrite_barrier(lhs_address, rhs));
}

void *  SXsafe_setfInit(void * lhs_address, void * rhs) {
  BPTR object;

  if (CHECK_SETFINIT) {
    object = *((BPTR *) lhs_address);
    if (object != NULL) {
      /* if ((int) object != rhs) */
      Debugger();
    }
  }
  return((void *) (* (LPTR) lhs_address = (long) rhs));
}

void *ptrcpy(void *p1, void *p2, int num_bytes) {
  if (enable_write_barrier) {
    if (ENABLE_GC_TIMING) START_CODE_TIMING;
    //pause_ok_flag = 0;
    scan_memory_segment(p1, (BPTR) p1 + num_bytes);
    //pause_ok_flag = 1;
    if (ENABLE_GC_TIMING) END_CODE_TIMING(total_write_barrier_time_in_cycle);
  }
  memcpy(p1, p2, num_bytes);
  return(p1);
}

void *ptrset(void *p1, int data, int num_bytes) {
  if (enable_write_barrier) {
    //pause_ok_flag = 0;
    scan_memory_segment(p1, (BPTR) p1 + num_bytes);
    //pause_ok_flag = 1;
  }
  memset(p1, data, num_bytes);
  return(p1);
}

static
void copyThreadInfo(SXobject thread) {
  if (next_thread < THREAD_LIMIT) {
    threads[next_thread].thread = thread;
    //setNeedSniff(thread);
    next_thread = next_thread + 1;
  } else {
    /* HEY! Should allocate a bigger buffer */
    printf("Too many threads!\n");
    exit(1);
  }
}

static
void save_thread_state() {
  /* SXforEach(SXallThreads, (SXfunction) copyThreadInfo, NULL) */
}

void SXscan_thread(SXobject thread) {
  BPTR bottom = SXgetStackBase(thread);
  /* Only scan threads with a real stack and skip the gc thread */
  if (bottom != 0) {
    int num_registers;
    BPTR top = SXgetStackTop(thread);
    BPTR ptr_aligned_top = (BPTR) ((long) top & ~(GC_POINTER_ALIGNMENT - 1));
    /* HEY! fix this - regs aren't special, just part of non run stack state */
    BPTR regptr = SXthread_registers(thread, &num_registers);
    
    /* Scan thread state atomically!!! */
    //pause_ok_flag = 0;
    /* HEY! reg ptrs are aligned on 4 byte boundries... */
    if (num_registers > 0) {
      scan_memory_segment(regptr, regptr + (num_registers * 4));
    }
    scan_memory_segment(ptr_aligned_top, bottom);
    //pause_ok_flag =1;
  }
}

static
void scan_threads() {
  while (next_thread > 0) {
    next_thread = next_thread - 1;
    SXscan_thread(threads[next_thread].thread);
    MAYBE_PAUSE_GC;
  }
}

static
void scan_globals() {
  scan_memory_segment(first_globals_ptr, last_globals_ptr);
}

static
void scan_static_space() {
  BPTR next, low, end;
  GCPTR gcptr;
  int size;

  next = first_static_ptr;
  end = last_static_ptr;
  while (next < end) {
    size = *((int *) next);
    size = size >> LINK_INFO_BITS;
    low = next + sizeof(GCPTR);
    next = low + size;
    gcptr = (GCPTR) (low - sizeof(GC_HEADER));
    scan_object(gcptr, size + sizeof(GC_HEADER));
    /* Delete ME! HEY! Convert to common scanner with scan_object 
       if (GET_STORAGE_CLASS(gcptr) != SC_NOPOINTERS) {
       scan_memory_segment(low,next);
       } */
  }
}

static
void scan_root_set() {
  last_gc_state = "Scan Threads";
  UPDATE_VISUAL_STATE();
  scan_threads();
  last_gc_state = "Scan Globals";
  UPDATE_VISUAL_STATE();
  scan_globals();
  last_gc_state = "Scan Statics";
  UPDATE_VISUAL_STATE();
  scan_static_space();
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
  case SC_INSTANCE:
    /* instance_metadata((SXobject) low); */
    scan_memory_segment_with_metadata(low,high,0);
    break;
  default: Debugger();
  }
}

static
void scan_object_with_group(GCPTR ptr, GPTR group) {
  scan_object(ptr, group->size);
  SET_COLOR(ptr,marked_color);
  group->black = ptr;
  group->black_count = group->black_count + 1;
}

/* HEY! Fix this up now that it's not continuation based... */
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
      /* current could be gray, black, or green */
      if ((current != NULL ) && (!(GRAYP(current)))) {
	current = GET_LINK_POINTER(current->prev);
      }
      while (current != NULL) {
	MAYBE_PAUSE_GC;
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
  MAYBE_PAUSE_GC;
}

static
void flip() {
  GPTR group;
  GCPTR free, prev, free_last, black;

  MAYBE_PAUSE_GC;
  // Originally, at this point all mutator threads are stopped, and none of
  // them is in the middle of an SXallocate. We got this for free by being
  // single threaded and implicity locking by yielding only when we chose to.
  //
  // Now, we have to acquire all group allocation locks to be sure no mutator
  // is allocating. Then we have to interrupt all mutators to stop them.
  // Then we can proceed to flip and then resume when the flip is done.
  
  last_gc_state = "Flip";
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    group = &groups[i];
    // No allocation allowed during a flip
    pthread_mutex_lock(&(group->free_lock));
		       
    group->gray = NULL;
    free = group->free;
    if (free != NULL) {
      prev = GET_LINK_POINTER(free->prev);
      if (prev != NULL) {
	SET_LINK_POINTER(prev->next,NULL); /* end black set */
      }
      SET_LINK_POINTER(free->prev,NULL);
    } else {
      free_last = group->free_last;
      if (free_last != NULL) {
	SET_LINK_POINTER(free_last->next,NULL); /* end black set */
      }
      group->free_last = NULL;
    }
    
    black = group->black;
    if (black == NULL) {
      // HEY! Why can't black be null?
      //printf("YOW! black is NULL\n");
    } else {
      // HEY! this was unconditional before - why?
      group->white = (GREENP(black) ? NULL : black);
    }

    group->black = group->free;
    group->white_count = group->black_count;
    group->black_count = 0;
  }

  pthread_mutex_lock(&flip_lock);
  SWAP(marked_color,unmarked_color);
  pthread_mutex_unlock(&flip_lock);

  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    group = &groups[i];
    pthread_mutex_unlock(&(group->free_lock));
  }
  save_thread_state();
}

/* WE need to change garbage color now so that conservative
   scanning doesn't start making free objects that look white turn gray! */
static
GCPTR recycle_group_garbage(GPTR group) {
  GCPTR next;
  GCPTR last;
  int count = 0;

  last = NULL;
  next = group->white;
  while (next != NULL) {
    int page_index = PTR_TO_PAGE_INDEX(next);
    PPTR page = &pages[page_index];
    int old_bytes_used = page->bytes_used;
    page->bytes_used = page->bytes_used - group->size;
    if (VISUAL_MEMORY_ON) {
      SXmaybe_update_visual_page(page_index,old_bytes_used,page->bytes_used);
    }
    
    if (GET_STORAGE_CLASS(next) == SC_INSTANCE) {
      SXobject obj = (SXobject) ((BPTR) next + 8);
      //void (*finalize)(SXobject) = (*obj)->finalize;
      // HEY! fix how we get finalize method
      int *finalize;
      if (finalize != NULL) {
	/* printf("class = %s\n", (*(SXclass_o *) obj)->name);  */
	/* UG! We're code that does SXgeneric dispatches, which may
	   int turn try to allocate storage */
	memory_mutex = 0;
	if ((long) finalize != 1) {
	  //finalize(obj);	/* Single-inheritance speed optimization */
	} else {
	  //SXfinalize(obj);	/* Mulitple-inheritance */
	}
	/* SXfinalize(obj); */
	memory_mutex = 1;
      }
    }
    SET_COLOR(next,GREEN);
    if (DETECT_INVALID_REFS) {
      memset((BPTR) next + 8, INVALID_ADDRESS, group->size);
    }
    last = next;
    next = GET_LINK_POINTER(next->next);
    count = count + 1;
    MAYBE_PAUSE_GC;
  }

  /* HEY! could unlink free obj on pages whoe count is 0. Then hook remaining
     frag free onto free list and coalesce 0 pages */
  if (count != group->white_count) {
    verify_all_groups();
    Debugger();
  }
  /* Append garbage to free list. Not great for a VM system, but it's easier */
  if (last != NULL) {
    SET_LINK_POINTER(last->next, NULL);
    
    if (group->free == NULL) {
      group->free = group->white;
    }
    if (group->black == NULL) {
      group->black = group->white;
    }
    
    if (group->free_last != NULL) {
      SET_LINK_POINTER((group->free_last)->next, group->white);
    }
    SET_LINK_POINTER((group->white)->prev, group->free_last);
    group->free_last = last;
    group->green_count = group->green_count + count;
  }
  group->white = NULL;
  group->white_count = 0;
  return(last);
}

static 
void recycle_all_garbage() {
  int i;
  last_gc_state = "Recycle Garbage";
  UPDATE_VISUAL_STATE();
  for (i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i++) {
    recycle_group_garbage(&groups[i]);
  }
  coalesce_all_free_pages();
}


static 
void reset_gc_cycle_stats() {
  total_allocation_this_cycle = 0;
  total_gc_time_in_cycle = 0.0;
  total_write_barrier_time_in_cycle = 0.0;
  max_increment_in_cycle = 0.0;
  if (ENABLE_GC_TIMING) CPU_TOCKS(start_gc_cycle_tocks);
}

static 
void summarize_gc_cycle_stats() {
  double total_cycle_time;
  
  if (ENABLE_GC_TIMING) {
    ELAPSED_MILLISECONDS(start_gc_cycle_tocks, total_cycle_time);
    last_cycle_ms = total_cycle_time;
    last_gc_ms = total_gc_time_in_cycle;
    last_write_barrier_ms = total_write_barrier_time_in_cycle;
  }
  if (VISUAL_MEMORY_ON) SXdraw_visual_gc_stats();
}

void full_gc() {
  reset_gc_cycle_stats();
  flip();
  scan_root_set();
  scan_gray_set();
  
  printf("*****HEY! doing full_gc********\n");
  enable_write_barrier = 0;
  recycle_all_garbage(0);
  enable_write_barrier = 1;

  gc_count = gc_count + 1;
  summarize_gc_cycle_stats();
  last_gc_state = "Cycle Complete";
  UPDATE_VISUAL_STATE();
}

static 
void gc_loop() {
  while (1) {
    full_gc();
  }
}


int SXgc(void) {
  //run_gc_to_completion();
  //run_gc_to_completion();
  return(gc_count);
}

void init_realtime_gc() {
  int thread_bytes;

  gc_count = 0;
  visual_memory_on = 0;
  last_gc_state = "<initial state>";
  pthread_mutex_init(&flip_lock, NULL);
}
  

	    
		      
	
  
    


    
      
		       
  
	      	       
	       

	    
      
    
  
      
      
