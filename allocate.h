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

// Interface to rtgc allocator

#define RTnopointers ((void *) 0)
#define RTpointers   ((void *) 1)
#define RTcustom1    ((void *) 2)
#define RTmetadata   ((void *) 3)

#define RTbeerBash(lhs, rhs) ((lhs) = (rhs))
#define setf_init(lhs, rhs) ((lhs) = (rhs))

#define INVALID_ADDRESS 0xEF

typedef long RT_METADATA;

void *RTallocate(void *metadata, int number_of_bytes);

void *RTstatic_allocate(void *metadata, int number_of_bytes);

void *RTwrite_barrier(void *lhs_address, void * rhs);

void *RTsafe_bash(void *lhs_address, void * rhs);

void *RTsafe_setfInit(void *lhs_address, void * rhs);

void *ptrcpy(void *p1, void * p2, int num_bytes);

void *ptrset(void *p1, int data, int num_bytes);

void RTinit_heap(size_t first_segment_bytes, size_t static_size);

int RTpthread_create(pthread_t *thread, const pthread_attr_t *attr,
		     void *(*start_func) (void *), void *args);

int rtgc_count(void);

void RTfull_gc();

void RTregister_root_scanner(void (*root_scanner)());

int RTregister_custom_scanner(void (*custom_scanner)(void *low, void *high));

void RTregister_no_write_barrier_state(void *start, int len);

void RTtrace_pointer(void *ptr);

void RTtrace_heap_pointer(void *ptr);

struct timespec RTtime_diff(struct timespec start, struct timespec end);

int RTtime_cmp(struct timespec x, struct timespec y);

extern volatile int RTatomic_gc;
extern int RTpage_power;
extern int RTpage_size;
