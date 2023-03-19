all: test

test: test.o userfs.o
	gcc test.o userfs.o -o test

test.o: test.c
	gcc -O0 -ggdb -c test.c -o test.o -I ../utils

userfs.o: userfs.c
	gcc -Wall -O0 -ggdb -c userfs.c -o userfs.o

clean:
	rm -rf *.o a.out