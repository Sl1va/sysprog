#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "parser.h"

int main() {
    bool run = true;

    while (run) {
        unsigned int cmdline_size;
        char *cmdline = read_cmdline("> ", stdin, stdout, &cmdline_size);

        if (!cmdline) {
            run = false;
            continue;
        }

        // printf("%d \"%s\"\n", cmdline_size, cmdline);
        int num_tokens, err;
        cmdline_tokens(cmdline, cmdline_size, &num_tokens, &err);
        free(cmdline);
    }
}