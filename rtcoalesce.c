// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

// rtgc page coalescing code. 

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

static void verify_heap() {
  lock_all_free_locks();
  int page = 0;
  while (page < total_partition_pages) {
    GPTR group = pages[page].group;
    if (group > EXTERNAL_PAGE) {
      if (group->size <= BYTES_PER_PAGE) {
	//identify_single_free_page(page, group);
	page = page + 1;
      } else {
	//page = identify_multiple_free_pages(page, group);
	//page = page + (group->size / BYTES_PER_PAGE);
	BPTR page_base = PAGE_INDEX_TO_PTR(page);
	GCPTR gcptr = (GCPTR) page_base;
	if (gcptr->prev > (GCPTR) 16) {
	  assert(gcptr->prev >= (GCPTR) first_partition_ptr);
	}
	if (gcptr->next > (GCPTR) 16) {
	  assert(gcptr->next >= (GCPTR) first_partition_ptr);
	}
	page = page + (group->size / BYTES_PER_PAGE);
      }
    } else {
      page = page + 1;
    }
  }
  unlock_all_free_locks();
}

// new coalescer now that we don't have a giant implicit lock
// and we don't want to maintain page_bytes
static void coalesce_free_pages() {
  long next_page = 0;
  long hole = -1;
  long page_count;
  while (next_page < total_partition_pages) {
    if (pages[next_page].group == FREE_PAGE) {
      if (-1 == hole) {
	hole = next_page;
	page_count = 1;
      } else {
	page_count = page_count + 1;
      }
    } else {
      if (-1 != hole) {
	//printf("Adding hole start = %d page_count = %d\n", hole, page_count);
	RTinit_empty_pages(hole, page_count, HEAP_SEGMENT);
	hole = -1;
	page_count = 0;
      }
    }
    next_page = next_page + 1;
  }
  if (-1 != hole) {
    //printf("Adding hole start = %d page_count = %d\n", hole, contig_count);
    RTinit_empty_pages(hole, page_count, HEAP_SEGMENT);
  }
}


static void remove_object_from_free_list(GPTR group, GCPTR object) {
  GCPTR prev = GET_LINK_POINTER(object->prev);
  GCPTR next = GET_LINK_POINTER(object->next);

  if (object == group->free) {
    // we end up here a lot
    // caller must hold green lock from group to save
    // us from repeatedly locking and unlocking for a page of objects
    group->free = next;	       // must be locked
  }

  if (object == group->black) {
    group->black = next;       // safe to not lock
  }

  if (object == group->free_last) {
    // we end up here a lot
    group->free_last = ((next == NULL) ? prev : next);
  }

  if (prev != NULL) {
    SET_LINK_POINTER(prev->next, next);
  }
  if (next != NULL) {
    SET_LINK_POINTER(next->prev, prev);
  }

  // must lock these
  group->green_count = group->green_count - 1;
  group->total_object_count = group->total_object_count - 1;
}

static int all_green_page(int page, GPTR group) {
  BPTR page_base = PAGE_INDEX_TO_PTR(page);
  BPTR next_object = page_base;
  int all_green = 1;
  while (all_green && (next_object < (page_base + BYTES_PER_PAGE))) {
    GCPTR gcptr = (GCPTR) next_object;
    if (all_green && GREENP(gcptr)) {
      next_object = next_object + group->size;
    } else {
      all_green = 0;
    }
  }
  return(all_green);
}

static void identify_single_free_page(int page, GPTR group) {
  if (all_green_page(page, group)) {
    pthread_mutex_lock(&(group->free_lock));
    if (all_green_page(page, group)) {
      GCPTR next = (GCPTR) PAGE_INDEX_TO_PTR(page);
      // remove all objects on page from free list
      int object_count = BYTES_PER_PAGE / group->size;
      for (int i = 0; i < object_count; i++) {
	remove_object_from_free_list(group, next);
	next = (GCPTR) ((BPTR) next + group->size);
      }
      pages[page].group = 0;
      pages[page].group = FREE_PAGE;
      // HEY! conditionalize this clear page - just here to catch bugs
      //memset(PAGE_INDEX_TO_PTR(page), 0xEF, BYTES_PER_PAGE);
    }
    pthread_mutex_unlock(&(group->free_lock));
  }
}

// Return base page index, even if page is somewhere in the middle of object.
static int identify_multiple_free_pages(int page, GPTR group) {
  BPTR page_base = PAGE_INDEX_TO_PTR(page);
  GCPTR gcptr = (GCPTR) page_base;
  if (pages[page].base == gcptr) {
    // Getting here means we've found the start of a multi-page object
    if (GREENP(gcptr)) {
      pthread_mutex_lock(&(group->free_lock));
      if (GREENP(gcptr)) {
	int num_pages = group->size / BYTES_PER_PAGE;
	remove_object_from_free_list(group, gcptr);
	for (int i = 0; i < num_pages; i++) {
	  pages[page + i].base = 0;
	  pages[page + i].group = FREE_PAGE;
	  // HEY! conditionalize this clear page - just here to catch bugs
	  //memset(PAGE_INDEX_TO_PTR(page + i), 0, BYTES_PER_PAGE);
	}
      }
      pthread_mutex_unlock(&(group->free_lock));
    }
  } else {
    // Getting here means we've run into a race condition with a multi-page
    // object allocation. We passed the base page when the page was still
    // empty, but the object got allocated and now we need to jump past it.
    assert(pages[page].base < gcptr);
    printf("mapping race page %d to base ptr %p\n", page, pages[page].base);
    page = PTR_TO_PAGE_INDEX(pages[page].base);
  }
  return(page);
}

void identify_free_pages() {
  int page = 0;
  while (page < total_partition_pages) {
    GPTR group = pages[page].group;
    if (group > EXTERNAL_PAGE) {
      if (group->size <= BYTES_PER_PAGE) {
	identify_single_free_page(page, group);
	page = page + 1;
      } else {
	page = identify_multiple_free_pages(page, group);
	page = page + (group->size / BYTES_PER_PAGE);
	//page = page + 1;
      }
    } else {
      page = page + 1;
    }
  }
}

void RTroom_print(long *green_count, long *alloc_count, long *hole_counts) {
  long total_empty_pages = 0;
  printf("----------------------------------------------------------------\n");
  for (long i = 0; i < (total_partition_pages + 1); i++) {
    if (hole_counts[i] > 0) {
      printf("Hole size = %d: %d\n", i, hole_counts[i]);
    }
    total_empty_pages = total_empty_pages + (hole_counts[i] * i);
  }
  printf("Total hole bytes = %d\n", total_empty_pages * BYTES_PER_PAGE);
  long total_committed_bytes = 0;
  for (int i = MIN_GROUP_INDEX; i <= MAX_GROUP_INDEX; i = i + 1) {
    if ((green_count[i] > 0) || (alloc_count[i] > 0)) {
      long total_group_bytes = 	((alloc_count[i] +  green_count[i]) * 
				 groups[i].size);
      printf("Group size = %d: allocated: %d, free: %d, total_bytes = %d\n",
	     groups[i].size, 
	     alloc_count[i], 
	     green_count[i],
	     total_group_bytes);
      total_committed_bytes = total_committed_bytes + total_group_bytes;
    }
  }
  printf("Total committed bytes = %d\n", total_committed_bytes);
  printf("Total hole + committed bytes = %d\n", 
	 (total_empty_pages * BYTES_PER_PAGE) + total_committed_bytes);
  printf("----------------------------------------------------------------\n");
}

void RTroom() {
  long green_count[MAX_GROUP_INDEX + 1];
  long alloc_count[MAX_GROUP_INDEX + 1];
  long hole_counts_len = (total_partition_pages + 1) * sizeof(long);
  // HEY! should malloc this
  long hole_counts[hole_counts_len];
  memset(green_count, 0, sizeof(green_count));
  memset(alloc_count, 0, sizeof(alloc_count));
  memset(hole_counts, 0, hole_counts_len);
  int page = 0;
  int hole_len = 0;
  lock_all_free_locks();
  while (page < total_partition_pages) {
    GPTR group = pages[page].group;
    if (group > EXTERNAL_PAGE) {
      if (hole_len > 0) {
	hole_counts[hole_len] = hole_counts[hole_len] + 1;
	hole_len = 0;
      }
      GCPTR next = (GCPTR) PAGE_INDEX_TO_PTR(page);
      if (group->size <= BYTES_PER_PAGE) {
	int object_count = BYTES_PER_PAGE / group->size;
	int green = 0;
	int alloc = 0;
	for (int i = 0; i < object_count; i++) {
	  if (GREENP(next)) {
	    green = green + 1;
	  } else {
	    alloc = alloc + 1;
	  }
	  next = (GCPTR) ((BPTR) next + group->size);
	}
	green_count[group->index] = green_count[group->index] + green;
	alloc_count[group->index] = alloc_count[group->index] + alloc;
	page = page + 1;
      } else {
	if (GREENP(next)) {
	  printf("HEY! shouldn't see green multi page objects after coalesce!\n");
	  green_count[group->index] = green_count[group->index] + 1;
	} else {
	  alloc_count[group->index] = green_count[group->index] + 1;
	}
	page = page + (group->size / BYTES_PER_PAGE);
      }
    } else {
      if (group != EMPTY_PAGE) {
	Debugger("Should have found an EMPTY_PAGE!\n");
      }
      hole_len = hole_len + 1;
      page = page + 1;
    }
  }
  if (hole_len > 0) {
    hole_counts[hole_len] = hole_counts[hole_len] + 1;
  }
  unlock_all_free_locks();
  RTroom_print(green_count, alloc_count, hole_counts);
}


void coalesce_all_free_pages() {
  identify_free_pages();
  coalesce_free_pages();
  //  verify_heap();
  if ((gc_count % 1000) == 0) {
    RTroom();
  }
}


	
	
	
	
      
    
