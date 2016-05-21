# (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

a:	a.c
	gcc -o a -Og -g a.c -L/home/wade/rtgc -lrtgc -lpthread -lc -lm -ldl

opt-a:	a.c
	gcc -o a -O2 -g a.c -L/home/wade/rtgc -lrtgc -lpthread -lc -lm -ldl

lib:
	gcc -shared -fPIC -o librtgc.so -g rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c 

opt-lib:
	gcc -shared -fPIC -o librtgc.so -O2 -g rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c 

all:
	gcc -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c  -lpthread

debug:	
	gcc -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

opt:
	gcc -O2 -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c rtutil.c atomic-booleans.s rtcoalesce.c -lpthread

tags:
	etags *.[c,h]

clean:  
	rm -f a *.o *.so

