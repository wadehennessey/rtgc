// clang-check wbtest.c --ast-dump --
#include <string.h>

char *global_var;

typedef struct cons {
  void *car;
  struct cons *cdr;
} CONS;

typedef union header {
  struct {
    union header *ptr;
    void *ptr2;
  } one;
  void *ptr1;
} HEADER;

void s1(struct cons *lhs, CONS *rhs) {
  *lhs = *rhs;
}

CONS s2(struct cons *rhs) {
  CONS foo = *rhs;
  foo = *rhs;
  return(foo);
}

void u1(union header *lhs, HEADER*rhs) {
  *lhs = *rhs;
}

void mem1(CONS *lhs, CONS *rhs) {
  memset(lhs, 0, sizeof(CONS));
}

void mem2(CONS *lhs, CONS *rhs) {
  memcpy(lhs, rhs, sizeof(CONS));
}

void b1(CONS *c) {
  c->car = 0;
}

void b2(long *p[], long *x) {
  p[7] = x;
}

void b3(char *x) {
  global_var = x;
}

void nb1(CONS *c) {
  CONS *x;
  x = c->car;
  b1(x);
}

void nb2(long p[], long x) {
  p[7] = x;
}

void compound_ptr_assign(long *p[], long *x) {
  p[7] += 0;
}


