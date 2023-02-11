all: coro_sort

coro_sort: coro_sort.o libcoro.o
	clang coro_sort.o libcoro.o -o coro_sort -Wall

coro_sort.o: coro_sort.c
	clang -c coro_sort.c -o coro_sort.o -Wall

libcoro.o: libcoro.c libcoro.h
	clang -c libcoro.c -o libcoro.o -Wall

test: coro_sort
	./coro_sort 900 3 tests/*
	python3 checker.py -f sort_result.txt

clean:
	rm -f *.o
	rm -f coro_sort
	rm -f sort_result.txt