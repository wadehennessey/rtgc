
typedef struct cons {
  void *car;
  struct cons *cdr;
} CONS;

void t1(CONS *c) {
  c->car = 0;
}

  
