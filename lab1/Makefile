all: coro_sort

coro_sort: coro_sort.o libcoro.o
	gcc coro_sort.o libcoro.o -o coro_sort

coro_sort.o: coro_sort.c
	gcc -c coro_sort.c -o coro_sort.o 

libcoro.o: libcoro.c libcoro.h
	gcc -c libcoro.c -o libcoro.o

clean:
	rm *.o
	rm coro_sort