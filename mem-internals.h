/* Other parts of the system like to use IN_HEAP and IN_GLOBALS */
#define EMPTY_PAGE     ((GPTR) 0)
#define SYSTEM_PAGE    ((GPTR) 1)
#define STATIC_PAGE    ((GPTR) 2)
#define EXTERNAL_PAGE  ((GPTR) 3)

#define HEAP_SEGMENT 0
#define STATIC_SEGMENT 1

#define BYTES_PER_PAGE (1 << PAGE_POWER)
#define PAGE_ALIGNMENT_MASK (BYTES_PER_PAGE - 1)
#define PTR_TO_PAGE_INDEX(ptr) ((long) (((BPTR) ptr - first_partition_ptr) >> PAGE_POWER))
#define PAGE_INDEX_TO_PTR(page_index) (first_partition_ptr + ((page_index) << PAGE_POWER))
#define PTR_TO_GROUP(ptr) pages[PTR_TO_PAGE_INDEX(ptr)].group
#define IN_PARTITION(ptr) (((BPTR) ptr >= first_partition_ptr) && ((BPTR) ptr < last_partition_ptr))
#define PAGE_GROUP(ptr) (IN_PARTITION(ptr) ? PTR_TO_GROUP(ptr) : EXTERNAL_PAGE)
#define IN_HEAP(ptr) ((long) PAGE_GROUP(ptr) > (long) EXTERNAL_PAGE)
#define IN_STATIC(ptr) (((BPTR) ptr >= first_static_ptr) && ((BPTR) ptr < last_static_ptr))
#define IN_HEAP_OR_STATIC(ptr) (IN_HEAP(ptr) || IN_STATIC(ptr))
#define IN_GLOBALS(ptr) ((((BPTR ptr >= first_globals_ptr) && ((BPTR) ptr < last_globals_ptr))))
#define ROUND_DOWN_TO_PAGE(ptr) ((BPTR) (((long) ptr & ~PAGE_ALIGNMENT_MASK)))
#define ROUND_UP_TO_PAGE(ptr) (ROUND_DOWN_TO_PAGE(ptr) + BYTES_PER_PAGE)

typedef unsigned long * LPTR;
typedef unsigned char * BPTR;

extern BPTR first_partition_ptr; /* First heap object will always be scanned!*/
extern BPTR last_partition_ptr;
extern BPTR first_static_ptr;
extern BPTR last_static_ptr;
extern BPTR first_globals_ptr;
extern BPTR last_globals_ptr;

#define MIN_GROUP_INDEX 4	/* yields min 16 byte objects */
#define MAX_GROUP_INDEX 22	/* yields max 4 megabyte objects */
#define MIN_GROUP_SIZE (1 << MIN_GROUP_INDEX)
#define MAX_GROUP_SIZE ( 1 << MAX_GROUP_INDEX)
#define NUMBER_OF_GROUPS (MAX_GROUP_INDEX - MIN_GROUP_INDEX + 1)
#define MIN_OBJECT_ALIGNMENT (MIN_GROUP_SIZE - 1)
#define INSTANCE_TO_GCPTR(ptr) ((GCPTR) ((BPTR) ptr - sizeof(GC_HEADER)))
#define HEAP_OBJECT_TO_GCPTR(ptr) ((GCPTR) ((long) ptr & ~MIN_OBJECT_ALIGNMENT))
#define DOUBLE_ALIGNMENT (sizeof(double) - 1)
#define DOUBLE_ALIGNED_P(p) ((long) p == (((long) p) && ~DOUBLE_ALIGNMENT))
#define LONG_ALIGNMENT (sizeof(long) - 1)
#define ROUND_UPTO_LONG_ALIGNMENT(n) (((((n) - 1)) & ~LONG_ALIGNMENT) + \
                                     sizeof(long)) 
/* HEY!This is a pretty stupid definition. Something better?
   #define METADATAP(p) (IN_GLOBALS(p) !! !(isObj(p))) */

/* HEY! This is still pretty dumb, but beter... */
#define  CLASSP(ptr) (IN_HEAP_OR_STATIC(ptr) && (GET_INSTANCE_STORAGE_CLASS(ptr) == SC_INSTANCE))
#define METADATAP(ptr) (!(CLASSP(ptr)))

#define MAYBE_PAUSE_GC

#define MIN(x,y) ((x < y) ? x : y)
#define MAX(x,y) ((x > y) ? x : y)
#define SWAP(x,y) {int tmp; tmp = x; x = y; y = tmp;}

typedef struct group_info {
  int size;
  int index;

  GCPTR free_last;		/* used in rtgc and rtalloc */
  GCPTR free;			/* used in rtgc and rtalloc */
  GCPTR gray;			/* only used in rtgc */
  GCPTR black;			/* only used in rtgc */
  GCPTR white;			/* only used in rtgc */


  int total_object_count;	/* used in rtgc and rtalloc */
  int white_count;		/* only used in rtgc */
  int black_count;		/* used in rtgc and rtalloc */
  int green_count;		/* used in rtgc and rtalloc */

  pthread_mutex_t free_last_lock;	/* used in rtgc and rtalloc */
  pthread_mutex_t free_lock;	/* used in rtgc and rtalloc */
} GROUP_INFO;

typedef GROUP_INFO * GPTR;

typedef struct segment {
  BPTR first_segment_ptr;
  BPTR last_segment_ptr;
  int segment_page_count;
  int type;
} SEGMENT;

typedef struct hole {
  int page_count;		/* only used in rtalloc */
  struct hole *next;		/* only used in rtalloc */
} HOLE;

typedef HOLE * HOLE_PTR;

typedef struct page_info {
  GCPTR base;
  GPTR group;
  int bytes_used;
} PAGE_INFO;

typedef PAGE_INFO * PPTR;

typedef struct thread_info {
  SXobject thread;
} THREAD_INFO;

typedef THREAD_INFO * TPTR;

/* ANSI C lossage... */
void scan_memory_segment(BPTR low, BPTR high);
void scan_object(GCPTR ptr, int total_size);
void SXscan_thread(SXobject thread);
void *big_malloc(int size);
GCPTR interior_to_gcptr(BPTR ptr);
void SXinit_empty_pages(int first_page, int page_count, int type);
void verify_total_object_count(void);
void verify_header(GCPTR ptr);
void verify_group(GPTR group);
void verify_all_groups(void);
int SXprint_object_info(GCPTR ptr, int i);
int SXprint_page_info(int page_index);
void SXprint_group_info(GPTR group);
void SXprint_memory_summary(void);
void init_global_bounds();
void full_gc();
void K_MicroSeconds(tock_type *tr);

BPTR SXgetStackBase(SXobject thread);
BPTR SXgetStackTop(SXobject thread);
BPTR SXthread_registers(SXobject thread, int *num_registers);
SXint SXallocationTrueSize(void * metadata, SXint size);
SXint SXtrueSize(void *ptr);
void SXinit_heap(int default_heap_bytes, int static_size, BPTR first_partition_ptr, BPTR last_usable_ptr);
void SXinit_realtime_gc(void);
int SXmake_object_gray(GCPTR current, BPTR raw);
void Debugger(void);
void * SXbig_malloc(int size);
void SXcopy_regs_to_stack(BPTR regptr);
void out_of_memory(char *space_name, int size);


extern GROUP_INFO *groups;
extern PAGE_INFO *pages;

extern int next_thread; 	/* HEY! get rid of this... */

extern int gc_count;
extern int gc_increment;
extern int visual_memory_on;

extern SEGMENT *segments;
extern int total_segments;

extern THREAD_INFO *threads;
extern int total_threads;

extern int heap_bytes;
extern int total_partition_pages;
extern int memory_mutex;
extern int unmarked_color;
extern int marked_color;
extern int enable_write_barrier;

extern int total_allocation;
extern int total_requested_allocation;
extern int total_requested_objects;
extern int total_allocation_this_cycle;

extern char *last_gc_state;
extern double last_cycle_ms;
extern double last_gc_ms;
extern double last_write_barrier_ms;

extern SXbool SXstartingScriptX;
extern pthread_mutex_t flip_lock;
