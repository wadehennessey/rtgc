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
#include "infoBits.h"
#include "mem-internals.h"
#include "vizmem.h"
#include "allocate.h"

static
void remove_object_from_free_list(GPTR group, GCPTR object) {
  GCPTR prev = GET_LINK_POINTER(object->prev);
  GCPTR next = GET_LINK_POINTER(object->next);

  if (object == group->free) {
    // caller must hold green lock from group to save
    // us from repeatedly locking and unlocking for a page of objects
    group->free = next;	       // must be locked
  }
  if (object == group->black) {
    group->black = next;       // safe to not lock
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

  // must lock these
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

    GCPTR next = (GCPTR) PAGE_INDEX_TO_PTR(next_page_index);
    for (int i = 0; i < object_count; i++) {
      remove_object_from_free_list(group, next);
      next = (GCPTR) ((BPTR) next + group->size);
    }
    next_page_index = next_page_index + total_pages;
  }
  RTinit_empty_pages(first_page, page_count, HEAP_SEGMENT);
}

// Need to hold the group free lock so mutators do not allocate on
// a page that we've identified as free here. Maybe mutators should do this
// themselves before deciding they need a full_gc to get memory.
// They already must have the free lock to be allocating
static
void coalesce_segment_free_pages(int segment) {
  int first_page_index = -1;
  int contig_count = 0;
  int next_page_index = PTR_TO_PAGE_INDEX(segments[segment].first_segment_ptr);
  int last_page_index = PTR_TO_PAGE_INDEX(segments[segment].last_segment_ptr);
  while (next_page_index < last_page_index) { 
    GPTR group = pages[next_page_index].group;
    int total_pages = (group > 0) ?
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

//************************************************
// new coalescer now that we don't have a giant implicit lock
// and we don't want to maintain page_bytes

// identify free pages and turn back into SYSTEM_PAGEs

static
int all_green_page(int page, GPTR group) {
  // handle groups bigger than a page separately
  if ((group > EXTERNAL_PAGE) && (group->size <= BYTES_PER_PAGE)) {
    int all_green = 1;
    BPTR page_base = PAGE_INDEX_TO_PTR(page);
    BPTR next_object = page_base;
    while (all_green && (next_object < (page_base + BYTES_PER_PAGE))) {
      GCPTR gcptr = (GCPTR) next_object;
      if (all_green && GREENP(gcptr)) {
	next_object = next_object + group->size;
      } else {
	all_green = 0;
      }
    }
    return(all_green);
  } else {
    return(0);
  }
}

int syscnt = 0;
static
void identify_free_pages() {
  for (int page = 0; page < total_partition_pages; page++) {
    GPTR group = pages[page].group;
    if (all_green_page(page, group)) {
      pthread_mutex_lock(&(group->free_lock));
      if (all_green_page(page, group)) {
	// remove all objects on page from free list
	int object_count = BYTES_PER_PAGE / group->size;
	GCPTR next = (GCPTR) PAGE_INDEX_TO_PTR(page);
	for (int i = 0; i < object_count; i++) {
	  remove_object_from_free_list(group, next);
	  next = (GCPTR) ((BPTR) next + group->size);
	}
	pages[page].group = SYSTEM_PAGE;
	syscnt = syscnt + 1;
	//printf("gc_count %d, syscnt %d\n", gc_count, syscnt);
	printf("syscnt %d\n", syscnt);
      }
      pthread_mutex_unlock(&(group->free_lock));
    }
  }
}

void coalesce_all_free_pages() {
  identify_free_pages();
}


	
	
	
	
      
    
