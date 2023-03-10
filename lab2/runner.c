#include "parser.h"
#include "runner.h"

int run_job(struct shell_job *job) {
    
    /* DEBUG INFO START */
    printf("(num_tokens=%d, bg=%d)", job->num_tokens, job->bg);
    for (int j = 0; j < job->num_tokens; ++j) {
        printf(" <%s>", job->tokens[j]);
    }
    printf("\n");
    /* DEBIG INFO STOP */

    int num_cmds;
    struct cmd *cmds = retrieve_cmds(job, &num_cmds);
    if (!cmds) {
        printf("Failed to retrieve cmds\n");
        goto end;
    }

    printf("num_cmds=%d\n", num_cmds);
    for (int i = 0; i < num_cmds; ++i) {
        printf("(%d, argc=%d, name=%s, output_file=%s, is_append=%d) ", 
                i, cmds[i].argc, cmds[i].name, cmds[i].output_fname, cmds[i].output_append);

        for (int j = 0; j < cmds[i].argc; ++j)
            printf(" {%s}", cmds[i].args[j]);
        printf("\n");
    }

end:
    free_cmds(cmds, num_cmds);
    return 0;    
}
