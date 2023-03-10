#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "parser.h"

int main() {
    bool run = true;

    while (run) {
        unsigned int cmdline_size;
        char *cmdline = read_cmdline("> ", stdin, stdout, &cmdline_size);
        char **tokens = NULL;
        struct shell_job *jobs = NULL;

        if (!cmdline) {
            run = false;
            continue;
        }

        int num_tokens;
        tokens = cmdline_tokens(cmdline, cmdline_size, &num_tokens);
        
        if (!tokens) {
            printf("Parsing error occured!\n");
            goto loop_out;
        }

        int num_jobs;
        jobs = retrieve_jobs(tokens, num_tokens, &num_jobs);

        if (!jobs) {
            printf("Error occured while parsing jobs\n");
            goto loop_out;
        }

        printf("num_jobs=%d\n", num_jobs);
        for (int i = 0; i < num_jobs; ++i) {
            const struct shell_job job = jobs[i];

            printf("(job %d, num_tokens=%d, bg=%d)", i, job.num_tokens, job.bg);
            for (int j = 0; j < job.num_tokens; ++j) {
                printf(" <%s>", job.tokens[j]);
            }
            printf("\n");
            
        }

        loop_out:

        free_jobs(jobs, num_jobs);
        free_tokens(tokens, num_tokens);        
        free(cmdline);
    }
}