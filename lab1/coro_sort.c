#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "libcoro.h"

typedef struct file_sorter {
    char *filename;
    int *arr;
    int sz;

    // timestamp from last yield
    uint64_t yield_ts;
    uint64_t total_time;
} file_sorter_t;

typedef struct coro_pool {
    file_sorter_t *queue;
    int qsize;
    int qnext;
} coro_pool_t;

typedef struct coro_worker {
    coro_pool_t *pool;
    int wid;
} coro_worker_t;

int QUANTUM;

uint64_t microtime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = (uint64_t) ts.tv_sec * 1000000 + (uint64_t) ts.tv_nsec / 1000;
    return us;
}

#define DEFINE_FILE_SORTER(name) (file_sorter_t) {.filename=name, .arr=NULL, .sz=0, .yield_ts=-1, .total_time=0}
#define DEFINE_CORO_POOL(q, sz) (coro_pool_t) {.queue=q, .qsize=sz, .qnext=0}

#define GETBIT(n, k) (n & (1 << k)) >> k

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a < b ? b : a)

#define YIELD_WITH_TIMER(sorter)                                    \
    do {                                                            \
        (sorter)->total_time += microtime() - (sorter)->yield_ts;   \
        coro_yield();                                               \
        (sorter)->yield_ts = microtime();                           \
    } while (0)


// Radix Sort
// Time Complexity: O(n)
// Memory Complexity O(n)
void radix_sort_coro(file_sorter_t *sorter) {
    int *orig = sorter->arr;
    int arr_size = sorter->sz;
    struct coro *this = coro_this();

    int *radix[2] = {
        (int *)malloc(sizeof(int) * arr_size),
        (int *)malloc(sizeof(int) * arr_size)
    };

    int sz[2] = {0, 0};

    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j < arr_size; ++j){
            int b = GETBIT(orig[j], i);
            radix[b][sz[b]++] = orig[j];
        }

        int p = 0;
        for (int j = 0; j < 2; ++j) {
            for (int k = 0; k < sz[j]; ++k) {
                orig[p++] = radix[j][k];
            }
            sz[j] = 0;
        }

        printf("\n%s: switch count %lld\n", sorter->filename, coro_switch_count(this));
        uint64_t ts = microtime();
        if (sorter->yield_ts == -1)
            sorter->yield_ts = ts;

        if (ts - sorter->yield_ts >= QUANTUM){    
	        printf("%s: quantum has last (%llu us): yield\n", sorter->filename, (long long) (ts - sorter->yield_ts));
            YIELD_WITH_TIMER(sorter);
        } else {
            printf("%s: quantum did not last yet (%llu us): continue\n", sorter->filename, (long long) (ts - sorter->yield_ts));
        }
    }

    printf("%s: sort finished\n", sorter->filename);

    free(radix[0]);
    free(radix[1]);
}


int sort_file(void *data) {
    file_sorter_t *sorter = (file_sorter_t *) data;

    printf("Sorting %s\n", sorter->filename);

    FILE *f = fopen(sorter->filename, "r");
    
    int next;
    sorter->arr = NULL;
    sorter->sz = 0;

    while (fscanf(f, "%d", &next) != EOF) {
        sorter->sz++;
        sorter->arr = (int *) realloc(sorter->arr, sizeof(int) * sorter->sz);
        sorter->arr[sorter->sz - 1] = next;
    }

    radix_sort_coro(sorter);
    
    fclose(f);
    return 0;
}

int coro_worker_f(void *data) {
    coro_worker_t *worker = (coro_worker_t*) data;
    struct coro *this = coro_this();

    printf("Started worker #%d\n", worker->wid);

    uint64_t time_total = 0;
    while (worker->pool->qnext < worker->pool->qsize) {
        file_sorter_t *queue = worker->pool->queue;
        printf("Worker #%d: picked %s\n", worker->wid, queue[worker->pool->qnext].filename);
        int this_sorter = worker->pool->qnext++;
        coro_yield();
        sort_file(&queue[this_sorter]);
        printf("Worker #%d: sorted %s\n", worker->wid, queue[this_sorter].filename);

        time_total += queue[this_sorter].total_time;
    }

    printf("Finished worker #%d\n", worker->wid);
    coro_yield();
    printf("Worker #%d: uptime %llu us, %lld context switches\n", worker->wid, (long long) time_total, coro_switch_count(this));

    return 0;
}


void coro_pool_f(int pool_size, file_sorter_t *queue, int qsize) {
    // run `pool_size` coroutines
    coro_pool_t shared_pool = DEFINE_CORO_POOL(queue, qsize);

    int num_workers = MIN(pool_size, qsize);
    coro_worker_t *workers = (coro_worker_t *) malloc(sizeof(coro_worker_t) * num_workers);

    for (int i = 0; i < num_workers; ++i) {
        workers[i] = (coro_worker_t) {.pool=&shared_pool, .wid=i + 1};
        coro_new(coro_worker_f, (void*) &workers[i]);
    }

    struct coro *c;
    while ((c = coro_sched_wait()) != NULL) {
        // printf("Finished %d\n", coro_status(c));
        coro_delete(c);
    }

    free(workers);
}


int main(int argc, char *argv[]){
    uint64_t time_start, time_stop;
    time_start = microtime();

    if (argc < 4) {
        fprintf(stderr, "Usage: %s LATENCY COROUTINES FILE...\n", argv[0]);
        exit(1);
    }

    int latency;
    if (!sscanf(argv[1], "%d", &latency) || latency <= 0) {
        fprintf(stderr, "LATENCY should be natural number\n");
        exit(1);
    }

    int num_cor;
    if (!sscanf(argv[2], "%d", &num_cor) || num_cor <= 0) {
        fprintf(stderr, "COROUTINES should be natural number\n");
        exit(1);
    }

    coro_sched_init();

    int num_files = argc - 3;
    file_sorter_t *sorters = (file_sorter_t *) malloc(sizeof(file_sorter_t) * num_files);

    QUANTUM = latency / num_cor;

    printf("quantum: %d\n", QUANTUM);

    for (int i = 3; i < argc; ++i)
        sorters[i - 3] = DEFINE_FILE_SORTER(argv[i]);

    // run pool of coroutines and complete sorting files separately
    coro_pool_f(num_cor, sorters, num_files);

    // merge sorted arrays and write to file
    FILE *out = fopen("sort_result.txt", "w");

    int *off = (int *) malloc(sizeof(int) * num_files);
    memset(off, 0, sizeof(int) * num_files);

    int final_size = 0;
    for (int i = 0; i < num_files; ++i)
        final_size += sorters[i].sz;

    for (int i = 0; i < final_size; ++i) {
        int idx = -1;

        for (int j = 0; j < num_files; ++j) {
            if (sorters[j].sz == off[j]) continue;

            if (idx == -1 || sorters[j].arr[off[j]] < sorters[idx].arr[off[idx]])
                idx = j;
        }

        fprintf(out, "%d ", sorters[idx].arr[off[idx]++]);
    }

    free(off);
    free(sorters);
    fclose(out);

    time_stop = microtime();
    printf("Total execution time: %llu us\n", (long long) (time_stop - time_start));
}