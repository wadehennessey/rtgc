
all:	
	gcc -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c vizmem.c -lpthread

tags:
	etags *.[c,h]

clean:  
	rm -f a

