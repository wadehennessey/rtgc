# (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

CC = gcc

a:	a.c
	$(CC) -o a -g a.c -L./ -lrtgc

opt-a:	
	$(CC) -o a -O2 -g -DNDEBUG a.c -L./ -lrtgc

lib:
	$(CC) -shared -fPIC -o librtgc.so -g rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

opt-lib:
	$(CC) -shared -fPIC -o librtgc.so -O2 -g -DNDEBUG rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread 

all:
	$(CC) -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c  -lpthread

debug:	
	$(CC) -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

opt:
	$(CC) -O2 -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

sigtime:sigtime.c
	$(CC) -o sigtime -g sigtime.c -lpthread

install:
	cp allocate.h /usr/local/include
	cp librtgc.so /usr/local/lib64
	/sbin/ldconfig

tags:
	etags *.[c,h]

clean:  
	rm -f a *.o *.so

