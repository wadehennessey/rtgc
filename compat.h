typedef int SXint;
typedef long SXlong;
typedef int *SXobject;
typedef int SXbool;
#define SXfalse 0
#define SXtrue  1
#define SXundefined 0
#define SX_PRIORITY_GC 1
#define SXiNonPreemptiable 1;
#define trueObject 1;

typedef struct metadata {
  int nBytes;
} MetaData;

typedef struct sxclass_o {
  int allocz;
} *SXclass_o;



