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
        int num_tokens;
        char **tokens = cmdline_tokens(cmdline, cmdline_size, &num_tokens);
        
        if (!tokens) {
            printf("Parsing error occured!\n");
            goto loop_out;
        }

        printf("%d", num_tokens);
        for (int i = 0; i < num_tokens; ++i) {
            printf(" <%s>", tokens[i]);
        }
        printf("\n");

        loop_out:
        if (tokens) {
            for (int i = 0; i < num_tokens; ++i)
                free(tokens[i]);
            free(tokens);
        }
        free(cmdline);
    }
}