// rtgc.c page coalescing code. 
// moved here for now to reduce clutter since we aren't
// we've stopped using until a concurrent collector is working well


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

// Need to hold the group free lock so mutator doesn't allocate on
// a page that we've identified as free here. Maybe mutators should do this
// them selves before deciding they need a full_gc to get memory.
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

static
void coalesce_all_free_pages() {
  for (int segment = 0; segment < total_segments; segment++) {
    if (segments[segment].type == HEAP_SEGMENT) {
      coalesce_segment_free_pages(segment);
    }
  }
}
