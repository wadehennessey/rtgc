/* Memory management configuration */

// HEY! get rid of max_segments?
#define MAX_HEAP_SEGMENTS 1
#define MAX_STATIC_SEGMENTS 1
#define MAX_SEGMENTS MAX_HEAP_SEGMENTS + MAX_STATIC_SEGMENTS

/* The heap is divided into multiple segments */
#define DEFAULT_HEAP_SEGMENT_SIZE 1 << 20
#define DEFAULT_STATIC_SEGMENT_SIZE 0     /* only 1 static segment for now */
#define CHECK_BASH 0
#define CHECK_SETFINIT 1
#define GC_POINTER_ALIGNMENT (sizeof(long))
#define PAGE_POWER 10
#define THREAD_LIMIT 100
#define INTERIOR_PTR_RETENTION_LIMIT 512

// HEY! switch this to use struct timeval with tv_sec and tv_usec
typedef struct tocktype {
  int hi;
  int lo;
} tock_type;
#define CPU_TOCKS(tr) (K_MicroSeconds(&(tr)));
#define SECONDS_PER_TOCK 1e-6

#define ELAPSED_MILLISECONDS(start, delta) { tock_type end; CPU_TOCKS(end); \
    delta = (end.lo - start.lo) / 1000.0; }
#define START_CODE_TIMING { tock_type start_tocks; double time; \
                            CPU_TOCKS(start_tocks)
#define END_CODE_TIMING(total) ELAPSED_MILLISECONDS(start_tocks, time); \
                               total = total + time; }

#define ENABLE_VISUAL_MEMORY 0
#define VISUAL_MEMORY_ON (ENABLE_VISUAL_MEMORY && visual_memory_on)
#define VISUAL_MEMORY_DEFAULT_ON 0
#define UPDATE_VISUAL_STATE() {if (VISUAL_MEMORY_ON) { \
                               SXdraw_visual_gc_state(); }}
#define ENABLE_GC_TIMING 0
#define DETECT_INVALID_REFS 0

