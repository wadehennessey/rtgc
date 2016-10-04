// clang-check wbtest.c --ast-dump --

typedef struct cons {
  void *car;
  struct cons *cdr;
} CONS;

void b1(CONS *c) {
  c->car = 0;
}

void b2(long *p[], long *x) {
  p[7] = x;
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



