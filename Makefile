
all:	
	gcc -g -o a a.c rtglobals.c rtalloc.c rtgc.c rtstop.c vizmem.c -lpthread

clean:  
	rm -f a

