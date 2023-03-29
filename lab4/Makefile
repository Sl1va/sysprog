all: test.o thread_pool.o
	gcc test.o thread_pool.o -o test

test.o: test.c thread_pool.h
	gcc -O0 -ggdb -c test.c -o test.o -I ../utils

thread_pool.o: thread_pool.c thread_pool.h
	gcc -O0 -ggdb -c thread_pool.c -o thread_pool.o

clean:
	rm -f *.o test