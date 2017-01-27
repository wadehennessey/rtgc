# Copyright 2017 Wade Lawrence Hennessey
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

