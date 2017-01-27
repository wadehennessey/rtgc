/*
 * Copyright 2017 Wade Lawrence Hennessey
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

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
  p[7] += 8;
  p[7] -= p[6];
}


