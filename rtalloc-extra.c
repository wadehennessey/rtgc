// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

// rtalloc.c stuff we may use again some day
// moved here for now to reduce clutter since we aren't
// usinig this stuff now

int RTtotalFreeHeapSpace() {
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

int RTlargestFreeHeapBlock() {
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

static void *copy_object(LPTR src, int storage_class, int current_size,
			  int new_size, int group_size) {
  BPTR new; LPTR new_base; LPTR src_base; int i;
  int limit = current_size / sizeof(LPTR);

  switch (storage_class) {
  case SC_POINTERS:
    new = RTallocate(RTpointers,new_size); break;
  case SC_NOPOINTERS:
    new = RTallocate(RTnopointers,new_size); break;
  case SC_METADATA:
    {
      // HEY! fix this to use current_size instead of group_size
      LPTR last_ptr = src + (group_size / 4) - 1;
      void *md = (void*) *last_ptr;
      if (METADATAP(md)) {
	new = RTallocate(md, new_size);
	limit = limit - 1;
      } else {
	printf("metadata based!\n");
      }
    }
    break;
  case SC_INSTANCE:
    new = RTallocate(((GCMDPTR) src)->metadata,new_size);
    break;
  default: printf("Error! Uknown storage class in copy object\n");
  }
  new_base = (LPTR) HEAP_OBJECT_TO_GCPTR(new);

  // No need for write barrier calls since these are initializing writes
  for (i = 2; i < limit; i++) {
    *(new_base + i) = *(src + i);
  }
  return(new);
}

void *RTreallocate(void *ptr, int new_size) {
  GCPTR current;
  GPTR group;
  int storage_class;
  
  if (IN_HEAP(ptr)) {
    current = HEAP_OBJECT_TO_GCPTR(ptr);
    storage_class = GET_STORAGE_CLASS(current);
    group = pages[PTR_TO_PAGE_INDEX(current)].group;

    // HEY! subtract only 8 if we don't have metadata
    if (new_size <= (group->size - 12)) {
      // HEY! If the object shrinks a lot, we should copy to a
      // smaller group size. Then free the current object? Dangerous
      // if other pointers to it exist. Maybe just let the GC find it.
      // Also clear unused bits so we don't retain garbage!
      return(ptr);
    } else {
      return(copy_object((LPTR) current, storage_class, group->size, new_size,
			 group->size));
    }
  } else {
    if (IN_STATIC(ptr)) {
      LPTR base = ((LPTR) ptr) - 1;
      int current_size = (*base << LINK_INFO_BITS);
      current = (GCPTR) (base - 1);
      storage_class = GET_STORAGE_CLASS(current);
      if (storage_class == SC_METADATA) {
	printf("Cannot realloc static objects with metadata yet!!!\n");
      }
      // HEY! what should we be adding to current_size?
      return(copy_object((LPTR) current, storage_class,
			 current_size + sizeof(GC_HEADER),
			 new_size, group->size));
    } else {
      printf("Cannot reallocate object\n");
    }
  }
}

