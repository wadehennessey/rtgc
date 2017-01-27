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

// Memory management configuration

// HEY! get rid of max_segments?
#define MAX_HEAP_SEGMENTS 1
#define MAX_STATIC_SEGMENTS 1
#define MAX_SEGMENTS MAX_HEAP_SEGMENTS + MAX_STATIC_SEGMENTS
#define MAX_THREADS 20
#define MAX_GLOBAL_ROOTS 1000

// The heap is divided into multiple segments
#define DEFAULT_HEAP_SEGMENT_SIZE 1 << 20
#define DEFAULT_STATIC_SEGMENT_SIZE 0     /* only 1 static segment for now */
#define CHECK_BASH 0
#define CHECK_SETFINIT 1
#define GC_POINTER_ALIGNMENT (sizeof(long *))
#define PAGE_POWER 12	       /* x86_64 page size is normally 4096 */
//#define INTERIOR_PTR_RETENTION_LIMIT 32
#define INTERIOR_PTR_RETENTION_LIMIT 512

#define FLIP_SIGNAL SIGUSR1
#define DETECT_INVALID_REFS 0
#define USE_BIT_WRITE_BARRIER 1

#ifdef NDEBUG
#define DEBUG(x)
#else
#define DEBUG(x) x
#endif


