# (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

a:	a.c
	gcc -o a -Og -ggdb3 a.c -L./ -lrtgc -lpthread -lc -lm -ldl

opt-a:	a.c
	gcc -o a -O2 -ggdb3 a.c -L./ -lrtgc -lpthread -lc -lm -ldl

lib:
	gcc -shared -fPIC -o librtgc.so -ggdb3 rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c 

opt-lib:
	gcc -shared -fPIC -o librtgc.so -O2 -ggdb3 rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c 

all:
	gcc -ggdb3 -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c  -lpthread

debug:	
	gcc -ggdb3 -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

opt:
	gcc -O2 -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

install:
	cp allocate.h /usr/local/include
	cp librtgc.so /usr/local/lib64
	/sbin/ldconfig

tags:
	etags *.[c,h]

clean:  
	rm -f a *.o *.so

