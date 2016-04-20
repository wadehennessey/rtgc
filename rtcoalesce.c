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

// new coalescer now that we don't have a giant implicit lock
// and we don't want to maintain page_bytes
void coalesce_free_pages() {
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


static
void remove_object_from_free_list(GPTR group, GCPTR object) {
  GCPTR prev = GET_LINK_POINTER(object->prev);
  GCPTR next = GET_LINK_POINTER(object->next);

  if (object == group->free) {
    // we end up here a lot
    // caller must hold green lock from group to save
    // us from repeatedly locking and unlocking for a page of objects
    group->free = next;	       // must be locked
  }

  assert(object != group->black);
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

static
int all_green_page(int page, GPTR group) {
  if (group > EXTERNAL_PAGE) {
    BPTR page_base = PAGE_INDEX_TO_PTR(page);
    BPTR next_object = page_base;
    if (group->size < BYTES_PER_PAGE) {
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
    } else {
      // Use this code to enable large object recycle
      // currently crashes after a bit of run time
      GCPTR gcptr = (GCPTR) next_object;
      assert(pages[page].base == gcptr);
      return(GREENP(gcptr));
      //return(0);
    }
  } else {
    return(0);
  }
}

static
void identify_free_pages() {
  int page = 0;
  while (page < total_partition_pages) {
    GPTR group = pages[page].group;
    if (group > EXTERNAL_PAGE) {
      if (all_green_page(page, group)) {
	pthread_mutex_lock(&(group->free_lock));
	if (all_green_page(page, group)) {
	  GCPTR next = (GCPTR) PAGE_INDEX_TO_PTR(page);
	  if (group->size < BYTES_PER_PAGE) {
	    // remove all objects on page from free list
	    int object_count = BYTES_PER_PAGE / group->size;
	    for (int i = 0; i < object_count; i++) {
	      remove_object_from_free_list(group, next);
	      next = (GCPTR) ((BPTR) next + group->size);
	    }
	    // HEY! Remove this clear page - just here to catch bugs
	    memset(PAGE_INDEX_TO_PTR(page), 0, BYTES_PER_PAGE);
	    pages[page].group = FREE_PAGE;
	  } else {
	    // set all freed pages to FREE_PAGE
	    int num_pages = group->size / BYTES_PER_PAGE;
	    remove_object_from_free_list(group, next);
	    for (int i = 0; i < num_pages; i++) {
	      pages[page + i].base = 0;
	      pages[page + i].group = FREE_PAGE;
	      // HEY! Remove this clear page - just here to catch bugs
	      memset(PAGE_INDEX_TO_PTR(page + i), 0, BYTES_PER_PAGE);
	    }
	  }
	}
	pthread_mutex_unlock(&(group->free_lock));
      }
      if (group->size < BYTES_PER_PAGE) {
	page = page + 1;
      } else {
	page = page + (group->size / BYTES_PER_PAGE);
      }
    } else {
      page = page + 1;
    }
  }
}

void coalesce_all_free_pages() {
  identify_free_pages();
  coalesce_free_pages();
}


	
	
	
	
      
    
