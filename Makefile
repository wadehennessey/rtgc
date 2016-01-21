# (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

all:
	gcc -Og -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c vizmem.c atomic_booleans.s rtcoalesce.c  -lpthread

debug:	
	gcc -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c vizmem.c atomic_booleans.s rtcoalesce.c -lpthread

opt:
	gcc -O2 -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c vizmem.c atomic_booleans.s rtcoalesce.c -lpthread

tags:
	etags *.[c,h]

clean:  
	rm -f a

