// clang-check wbtest.c --ast-dump --
#include <string.h>

char *global_var;

typedef struct cons {
  void *car;
  struct cons *cdr;
} CONS;

void s1(CONS *lhs, CONS *rhs) {
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

long nb2(long x) {
  return(x + 7);
}

void nb3(long p[], long x) {
  p[7] = x;
}



