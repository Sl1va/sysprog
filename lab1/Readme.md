### System Programming

#### Homework 1: Coroutines

Implementation of **Homework 1** on System programming course in Innopolis university.

Building:

```bash
make
```

Then executable `coro_sort` will be generated. Usage:

```bash
./coro_sort LATENCY COROUTINES FILE... 
```

here:

`LATENCY` - target latency

`COROUTINES` - number of coroutines in the pool

`FILE...` - files to sort (should be one or more)



Sorted result is saved in file `sort_result.txt`. You can test program on pre-generated tests running `make test` and it should be ok.



#### Implementation details

This task is implemented in C language (no C++/STL).

Target latency and number of coroutines in the pool can be configured.

Sorting algorithm - **Radix sort**.

#### Output format

When worker start it pick one of the files to sort:

```bash
Started worker #3                                                                         
Worker #3: picked tests/test1.txt
```

After each sorting iteration it checks if quantum has last. If so, it yields and gives control to other coroutine:

```bash
tests/test1.txt: switch count 2
tests/test1.txt: quantum did not last yet (71 mc): continue
tests/test1.txt: switch count 2
tests/test1.txt: quantum did not last yet (141 mc): continue                       
tests/test1.txt: switch count 2
tests/test1.txt: quantum did not last yet (212 mc): continue
tests/test1.txt: switch count 2
tests/test1.txt: quantum did not last yet (289 mc): continue
tests/test1.txt: switch count 2
tests/test1.txt: quantum has last (379 mc): yield
```

After worker sorted file, it pick next file if it exists:

```bash
tests/test2.txt: sort finished
Worker #2: sorted tests/test2.txt
Worker #2: picked tests/test5.txt
```



At the end there is some statistics of coroutines uptime, number of switches and total time:

```bash
Worker #3: uptime 19317 mc, 17 context switches
Worker #2: uptime 19387 mc, 17 context switches
Worker #1: uptime 19456 mc, 17 context switches
Total execution time: 25681 mc
```

