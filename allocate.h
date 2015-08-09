/* Interface to the memory manager */

/* Predefined Metadata */
#define SXpointers   ((void *) 0)
#define SXnopointers ((void *) 1)

#define SXbeerBash(lhs, rhs) ((lhs) = (rhs))
#define setf_init(lhs, rhs) ((lhs) = (rhs))

extern void * SXwrite_barrier(void * lhs_address, void * rhs);

extern void * SXsafe_bash(void * lhs_address, void * rhs);

extern void * SXsafe_setfInit(void * lhs_address, void * rhs);

extern void * SXallocate(void * metadata, SXint number_of_bytes);

extern void * SXstaticAllocate(void * metadata, SXint number_of_bytes);

extern void * SXreallocate(void *pointer, SXint new_size);

extern void * ptrcpy(void * p1, void * p2, int num_bytes);

extern void * ptrset(void * p1, int data, int num_bytes);

extern int SXgc(void);

extern int SXlargestFreeHeapBlock(void);

extern SXobject SXtotalFreeSystemSpace(void);

extern SXint SXstackAllocationSize(void * metadata, SXint size);

extern SXint SXallocationTrueSize(void * metadata, SXint size);

extern SXint SXtrueSize(void *ptr);

extern LPTR SXinitializeObject(void * metadata, void *base, int total_size, int real);

extern void init_realtime_gc();

#define INVALID_ADDRESS 0xEF


