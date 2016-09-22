# (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

CC = gcc

a:	a.c
	$(CC) -o a -Og -ggdb3 a.c -L./ -lrtgc -lpthread -lc -lm -ldl

opt-a:	a.c
	$(CC) -o a -O2 -ggdb3 -DNDEBUG a.c -L./ -lrtgc -lpthread -lc -lm -ldl

lib:
	$(CC) -shared -fPIC -o librtgc.so -ggdb3 rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c 

opt-lib:
	$(CC) -shared -fPIC -o librtgc.so -O2 -ggdb3 -DNDEBUG rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c 

all:
	$(CC) -ggdb3 -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c  -lpthread

debug:	
	$(CC) -ggdb3 -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

opt:
	$(CC) -O2 -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

install:
	cp allocate.h /usr/local/include
	cp librtgc.so /usr/local/lib64
	/sbin/ldconfig

tags:
	etags *.[c,h]

clean:  
	rm -f a *.o *.so

