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

// Links are divided into a pointer and some low order info bits.
typedef struct gc_header {
  struct gc_header *prev;
  struct gc_header *next;
} GC_HEADER;

typedef GC_HEADER *GCPTR;

#define LINK_INFO_BITS 4
#define LINK_INFO_MASK ((1 << LINK_INFO_BITS) - 1)
#define LINK_POINTER_MASK (~LINK_INFO_MASK)

#define GET_LINK_POINTER(l) ((GCPTR) (((long) (l)) & LINK_POINTER_MASK))
#define SET_LINK_POINTER(l, value) (RTbeerBash (l, \
                                    (GCPTR) (((long) (l) & LINK_INFO_MASK) \
					   | (long) value)))

#define GET_LINK_INFO(l,mask) ((long) l & (mask))
#define SET_LINK_INFO(l,mask,bits) (RTbeerBash (l, \
                                    (GCPTR) (((long) l & ~(mask)) | bits)))

// GC info bits
#define GC_STORAGE_INFO_MASK (0x3)
#define GC_COLOR_INFO_MASK (0x3)

#define GET_STORAGE_CLASS(p) (GET_LINK_INFO(p->next, GC_STORAGE_INFO_MASK))
#define SET_STORAGE_CLASS(p,RTclass) (SET_LINK_INFO(p->next,GC_STORAGE_INFO_MASK, RTclass))

#define GET_COLOR(p) (GET_LINK_INFO(p->prev,GC_COLOR_INFO_MASK))
#define SET_COLOR(p,color) (SET_LINK_INFO(p->prev,GC_COLOR_INFO_MASK,color))

// use enums for these instead?
#define SC_NOPOINTERS     0
#define SC_POINTERS       1
#define SC_CUSTOM1        2
#define SC_METADATA       3

#define GENERATION0       0
#define GENERATION1       1
#define GRAY              2
#define GREEN             3    // Could use GREEN=GRAY, but we've got room

#define WHITEP(p) (GET_COLOR(p) == unmarked_color)
#define BLACKP(p) (GET_COLOR(p) == marked_color)
#define GRAYP(p)  (GET_COLOR(p) == GRAY)
#define GREENP(p) (GET_COLOR(p) == GREEN)
