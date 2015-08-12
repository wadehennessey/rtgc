/* Interface to the memory manager */

/* Predefined Metadata */
#define SXpointers   ((void *) 0)
#define SXnopointers ((void *) 1)

#define SXbeerBash(lhs, rhs) ((lhs) = (rhs))
#define setf_init(lhs, rhs) ((lhs) = (rhs))

#define INVALID_ADDRESS 0xEF

void * SXwrite_barrier(void * lhs_address, void * rhs);

void * SXsafe_bash(void * lhs_address, void * rhs);

void * SXsafe_setfInit(void * lhs_address, void * rhs);

void * SXallocate(void * metadata, int number_of_bytes);

void * SXstaticAllocate(void * metadata, int number_of_bytes);

void * SXreallocate(void *pointer, int new_size);

void * ptrcpy(void * p1, void * p2, int num_bytes);

void * ptrset(void * p1, int data, int num_bytes);

int SXgc(void);

int SXlargestFreeHeapBlock(void);

int SXstackAllocationSize(void * metadata, int size);

int SXallocationTrueSize(void * metadata, int size);

int SXtrueSize(void *ptr);

LPTR SXinitializeObject(void * metadata, void *base, int total_size, int real);

void init_realtime_gc();


