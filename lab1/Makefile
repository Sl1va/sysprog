all: coro_sort

coro_sort: coro_sort.o libcoro.o
	gcc coro_sort.o libcoro.o -o coro_sort

coro_sort.o: coro_sort.c
	gcc -c coro_sort.c -o coro_sort.o 

libcoro.o: libcoro.c libcoro.h
	gcc -c libcoro.c -o libcoro.o

test: coro_sort
	./coro_sort 900 3 tests/*
	python3 checker.py -f sort_result.txt

clean:
	rm -f *.o
	rm -f coro_sort
	rm -f sort_result.txt