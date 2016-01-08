# (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

all:	
	gcc -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c vizmem.c -lpthread

opt:
	gcc -O2 -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c vizmem.c -lpthread

tags:
	etags *.[c,h]

clean:  
	rm -f a

