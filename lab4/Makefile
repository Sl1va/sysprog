all: test.o thread_pool.o
	gcc test.o thread_pool.o -o test -lpthread

test.o: test.c thread_pool.h
	gcc -O0 -ggdb -c test.c -o test.o -I ../sysprog/utils -lpthread

thread_pool.o: thread_pool.c thread_pool.h
	gcc -O0 -ggdb -c thread_pool.c -o thread_pool.o -lpthread

clean:
	rm -f *.o test