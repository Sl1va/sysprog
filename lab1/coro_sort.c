#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libcoro.h"

typedef struct file_sorter {
    char *filename;
    int *arr;
    int sz;
} file_sorter_t;

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
    file_sorter_t test = {.filename="tests/test1.txt"};
    sort_file(&test);

    // printf("%d\n%d\n%d\n", test.sz, test.arr[0], test.arr[test.sz - 1]);
    for (int i = 0; i < test.sz; ++i) {
        printf("%d ", test.arr[i]);
    }
}