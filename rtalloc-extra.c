// rtalloc.c stuff we may use again some day
// moved here for now to reduce clutter since we aren't
// usinig this stuff now


void * SXstaticAllocate(void * metadata, int size) {
  int real_size, data_size;
  LPTR base;
  GPTR group;
  BPTR new_static_ptr;

  /* We don't care about the group, just the GCHDR compatible real size */
  group = allocationGroup(metadata, size, &data_size, &real_size, &metadata);
  data_size = ROUND_UPTO_LONG_ALIGNMENT(data_size);
  real_size = ROUND_UPTO_LONG_ALIGNMENT(real_size);

  /* Static object headers are only 1 word long instead of 2 */
  /* HEY! add 8 byte alignment??? */
  new_static_ptr = first_static_ptr - (real_size - sizeof(GCPTR));
  
  if (new_static_ptr >= segments[0].first_segment_ptr) {
    int first_static_page_index = PTR_TO_PAGE_INDEX(new_static_ptr);
    int last_static_page_index = PTR_TO_PAGE_INDEX(first_static_ptr);
    int index;

    for (index = first_static_page_index; 
	 index < last_static_page_index; 
	 index++) {
      pages[index].group = STATIC_PAGE;
      if (VISUAL_MEMORY_ON) SXupdate_visual_page(index);
    }
    first_static_ptr = new_static_ptr;
  } else {
    /* HEY! allow more than 1 static segment? for now */
    /* just dynamically allocate after booting */
    return(SXallocate(metadata, size));
  }

  base = (LPTR) first_static_ptr;
  *base = (data_size << LINK_INFO_BITS);
  base = (LPTR) (((BPTR) base) - sizeof(GCPTR)); /* make base GCHDR compat */
  base = SXInitializeObject(metadata, base, real_size, real_size);
  return(base);
}

static void * copy_object(LPTR src, int storage_class, int current_size,
			  int new_size, int group_size) {
  BPTR new; LPTR new_base; LPTR src_base; int i;
  int limit = current_size / sizeof(LPTR);

  switch (storage_class) {
  case SC_POINTERS:
    new = SXallocate(SXpointers,new_size); break;
  case SC_NOPOINTERS:
    new = SXallocate(SXnopointers,new_size); break;
  case SC_METADATA:
    {
      /* HEY! fix this to use current_size instead of group_size */
      LPTR last_ptr = src + (group_size / 4) - 1;
      void *md = (void*) *last_ptr;
      if (METADATAP(md)) {
	new = SXallocate(md, new_size);
	limit = limit - 1;
      } else {
	printf("metadata based!\n");
      }
    }
    break;
  case SC_INSTANCE:
    new = SXallocate(((GCMDPTR) src)->metadata,new_size);
    break;
  default: printf("Error! Uknown storage class in copy object\n");
  }
  new_base = (LPTR) HEAP_OBJECT_TO_GCPTR(new);

  /* No need for write barrier calls since these are initializing writes */
  for (i = 2; i < limit; i++) {
    *(new_base + i) = *(src + i);
  }
  return(new);
}

void * SXreallocate(void *ptr, int new_size) {
  GCPTR current;
  GPTR group;
  int storage_class;
  
  /* omitted debug message */
  
  if (IN_HEAP(ptr)) {
    current = HEAP_OBJECT_TO_GCPTR(ptr);
    storage_class = GET_STORAGE_CLASS(current);
    group = pages[PTR_TO_PAGE_INDEX(current)].group;

    /* HEY! subtrace only 8 if we don't have metadata */
    if (new_size <= (group->size - 12)) {
      /* HEY! If the object shrinks a lot, we should copy to a
	 smaller group size. Then free the current object? Dangerous
	 if other pointers to it exist. Maybe just let the GC find it.

	 Also clear unused bits so we don't retain garbage! */
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


// seems to never be used
int SXstackAllocationSize(void * metadata, int size) {
  int data_size, real_size;
  GPTR group = allocationGroup(metadata,size,&data_size,&real_size,&metadata);
  return(real_size);
}

// seems to never be used
int SXallocationTrueSize(void * metadata, int size) {
  int data_size, real_size;

  GPTR group = allocationGroup(metadata,size,&data_size,&real_size,&metadata);
  int md_size = ((metadata > SXpointers) ? 4 : 0);
  return(group->size - sizeof(GC_HEADER) - md_size);
}

// seems to never be used
int SXtrueSize(void *ptr) {
  GPTR group = PTR_TO_GROUP(ptr);
  GCPTR gcptr = interior_to_gcptr(ptr);
  int md_size = ((GET_STORAGE_CLASS(gcptr) > SC_POINTERS) ? 4 : 0);
  return(group->size - sizeof(GC_HEADER) - md_size);
}
