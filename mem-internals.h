// (C) Copyright 2015 - 2017 by Wade L. Hennessey. All rights reserved.

#define EMPTY_PAGE     ((GPTR) 0)
#define FREE_PAGE      ((GPTR) 1)
#define SYSTEM_PAGE    ((GPTR) 2)
#define EXTERNAL_PAGE  ((GPTR) 3)

#define HEAP_SEGMENT 0
#define STATIC_SEGMENT 1

typedef unsigned long * LPTR;
typedef unsigned char * BPTR;

#define BYTES_PER_PAGE (1 << PAGE_POWER)
#define PAGE_ALIGNMENT_MASK (BYTES_PER_PAGE - 1)
#define PTR_TO_PAGE_INDEX(ptr) ((long) (((BPTR) ptr - first_partition_ptr) >> PAGE_POWER))
#define PAGE_INDEX_TO_PTR(page_index) (first_partition_ptr + ((page_index) << PAGE_POWER))
#define PTR_TO_GROUP(ptr) pages[PTR_TO_PAGE_INDEX(ptr)].group
#define IN_PARTITION(ptr) (((BPTR) ptr >= first_partition_ptr) && ((BPTR) ptr < last_partition_ptr))
#define PAGE_GROUP(ptr) (IN_PARTITION(ptr) ? PTR_TO_GROUP(ptr) : EXTERNAL_PAGE)
#define IN_HEAP(ptr) ((long) PAGE_GROUP(ptr) > (long) EXTERNAL_PAGE)
#define ROUND_DOWN_TO_PAGE(ptr) ((BPTR) (((long) ptr & ~PAGE_ALIGNMENT_MASK)))
#define ROUND_UP_TO_PAGE(ptr) (ROUND_DOWN_TO_PAGE(ptr) + BYTES_PER_PAGE)

#define MIN_GROUP_INDEX 5	// yields min 32 byte objects on x86_64
#define MAX_GROUP_INDEX 24	// yields max 16 megabyte objects
#define MIN_GROUP_SIZE (1 << MIN_GROUP_INDEX)
#define MAX_GROUP_SIZE ( 1 << MAX_GROUP_INDEX)
#define NUMBER_OF_GROUPS (MAX_GROUP_INDEX - MIN_GROUP_INDEX + 1)
#define LONG_ALIGNMENT (sizeof(long) - 1)
#define ROUND_UPTO_LONG_ALIGNMENT(n) (((((n) - 1)) & ~LONG_ALIGNMENT) + \
                                     sizeof(long)) 
#define BITS_PER_BYTE 8
#define BITS_PER_LONG (BITS_PER_BYTE * sizeof(long))


#define METADATAP(ptr) (((void *) ptr)  > RTpointers)

#define MIN(x,y) ((x < y) ? x : y)
#define MAX(x,y) ((x > y) ? x : y)
#define SWAP(x,y) {int tmp = x; x = y; y = tmp;}

typedef struct group_info {
  int size;
  int index;

  GCPTR last;			// used in rtgc and rtalloc
  GCPTR free;			// used in rtgc and rtalloc
  GCPTR gray;			// only used in rtgc
  GCPTR black;			// only used in rtgc and rtalloc
  GCPTR white;			// only used in rtgc

  int white_count;		// only used in rtgc
  int black_scanned_count;	// used in rtgc
  int black_alloc_count;        // used in rtgc and rtalloc

  pthread_mutex_t free_lock;	// used in rtgc and rtalloc
  pthread_mutex_t black_and_last_lock;	// used in rtgc and rtalloc
} GROUP_INFO;

typedef GROUP_INFO *GPTR;

typedef struct segment {
  BPTR first_segment_ptr;
  BPTR last_segment_ptr;
  int segment_page_count;
  int type;
} SEGMENT;

typedef struct hole {
  long page_count;		// only used in rtalloc
  struct hole *next;		// only used in rtalloc
} HOLE;

typedef HOLE *HOLE_PTR;

typedef struct page_info {
  GCPTR base;
  GPTR group;
} PAGE_INFO;

typedef PAGE_INFO *PPTR;

typedef struct thread_state {
  gregset_t registers;		// NREG is 23 on x86_64
  char *saved_stack_base;	// This is the LOWEST addressable byte
  int saved_stack_size;
} THREAD_STATE;
  
typedef struct thread_info {
  pthread_t pthread;
  long long *stack_base; // This is the LOWEST addressable byte of the stack
  int stack_size;
  char *stack_bottom;	 // HIGHEST address seen when thread started

  int saved_thread_index;     // copied stack and register states

  struct timeval max_pause_tv, total_pause_tv;
  struct thread_info *next;
  // These fields are only used at thread startup time
  void *(*start_func) (void *);
  char *args;
  volatile long started;
} THREAD_INFO;

typedef struct counter {
  int count;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} COUNTER;

void scan_object(GCPTR ptr, int total_size);
void RTinit_empty_pages(int first_page, int page_count, int type);
void rtgc_loop();
void init_signals_for_rtgc();
void lock_all_free_locks();
void unlock_all_free_locks();
int stop_all_mutators_and_save_state();
void RTroom();
void init_realtime_gc(void);
void Debugger(char *msg);
void *RTbig_malloc(size_t size);
void RTcopy_regs_to_stack(BPTR regptr);
void out_of_memory(char *space_name, int size);
void register_global_root(void *root);
void counter_init(COUNTER *c);
int counter_zero(COUNTER *c);
int counter_increment(COUNTER *c);
void counter_wait_threshold(COUNTER *c, int threshold);
void locked_byte_or(unsigned char *x, unsigned char y);
void locked_long_or(unsigned long *x, unsigned long y);
void locked_long_and(unsigned long *x, unsigned long y);
void locked_long_inc(volatile unsigned long *x);
void coalesce_all_free_pages();

extern BPTR first_partition_ptr;
extern BPTR last_partition_ptr;
extern BPTR first_static_ptr;
extern BPTR last_static_ptr;
extern BPTR static_frontier_ptr;

extern GROUP_INFO *groups;
extern PAGE_INFO *pages;
extern HOLE_PTR empty_pages;
extern volatile long gc_count;

extern SEGMENT *segments;
extern int total_segments;

extern THREAD_INFO *threads;
extern THREAD_INFO *live_threads;
extern THREAD_INFO *free_threads;
extern int total_threads;

extern THREAD_STATE *saved_threads;
extern int total_saved_threads;

extern long total_partition_pages;
extern int unmarked_color;
extern int marked_color;
extern int enable_write_barrier;

extern pthread_key_t thread_key;
extern char **global_roots;
extern int total_global_roots;
extern pthread_mutex_t threads_lock;
extern pthread_mutex_t empty_pages_lock;
extern pthread_mutex_t global_roots_lock;
extern pthread_mutex_t static_frontier_ptr_lock;

extern sem_t gc_semaphore;
extern volatile int run_gc;

#if USE_BIT_WRITE_BARRIER
extern LPTR RTwrite_vector;
#else
extern BPTR RTwrite_vector;
#endif
extern size_t RTwrite_vector_length;

extern long *RTno_write_barrier_state_ptr;
extern long saved_no_write_barrier_state;

#define LOCK(lock)  pthread_mutex_lock(&lock)
#define UNLOCK(lock) pthread_mutex_unlock(&lock)
#define WITH_LOCK(lock, code) LOCK(lock); \
			      code \
			      UNLOCK(lock);
