#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libcoro.h"

typedef struct file_sorter {
    char *filename;
    int *arr;
    int sz;
} file_sorter_t;

#define DEFINE_FILE_SORTER(name) (file_sorter_t) {.filename=name, .arr=NULL, .sz=0}

#define GETBIT(n, k) (n & (1 << k)) >> k 

// Radix Sort
// Time Complexity: O(n)
// Memory Complexity O(n)
void radix_sort(int *orig, int arr_size) {
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
    }

    free(radix[0]);
    free(radix[1]);
}

int sort_file(void *data) {
    file_sorter_t *sorter = (file_sorter_t *) data;

    FILE *f = fopen(sorter->filename, "r");
    
    int next;
    sorter->arr = NULL;
    sorter->sz = 0;

    while (fscanf(f, "%d", &next) != EOF) {
        sorter->sz++;
        sorter->arr = (int *) realloc(sorter->arr, sizeof(int) * sorter->sz);
        sorter->arr[sorter->sz - 1] = next;
    }

    radix_sort(sorter->arr, sorter->sz);
}

int main(int argc, char *argv[]){
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

    int num_files = argc - 3;
    file_sorter_t *sorters = (file_sorter_t *) malloc(sizeof(file_sorter_t) * num_files);

    for (int i = 3; i < argc; ++i)
        sorters[i - 3] = DEFINE_FILE_SORTER(argv[i]);


    for (int i = 0; i < num_files; ++i) {
        sort_file(&sorters[i]);
    }

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
}