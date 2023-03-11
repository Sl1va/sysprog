#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "parser.h"
#include "runner.h"

static const char *JOB_OPERATOR_STR[] = {
    [OP_AND] = "AND",
    [OP_OR] = "OR",
    [OP_BG] = "BG",
};

int chain_jobs(struct shell_job *jobs, int num_jobs) {
    static int job_counter = 0;

    // printf("num_jobs=%d\n", num_jobs); // DEBUG MESSAGE
    for (int i = 0; i < num_jobs; ++i) {

        // run job in background
        if (jobs[i].operator == OP_BG) {
            ++job_counter;
            int pid = fork();
            if (!pid) {
                setsid(); // make bg proces group leader
                
                if (run_job(&jobs[i])) {
                    printf("[%d] Failed\n", job_counter);
                    exit(1);
                }

                printf("[%d] Done\n", job_counter);
                exit(0);
            } else if (pid > 0) {
                printf("[%d] Started job with pid %d\n", job_counter, pid);
            } else {
                printf("chain_jobs: Failed to fork\n");
                return 1;
            }
        }

        // run job in foreground
        if (run_job(&jobs[i])) {
            if (jobs[i].operator == OP_AND) 
                ++i;
            else 
                return 1;
        }
    }

    return 0;
}

int run_job(struct shell_job *job) {
    
    /* DEBUG INFO START */
    /*
    printf("(num_tokens=%d, operator=%s)", job->num_tokens, JOB_OPERATOR_STR[job->operator]);
    for (int j = 0; j < job->num_tokens; ++j) {
        printf(" <%s>", job->tokens[j]);
    }
    printf("\n");
    */
    /* DEBIG INFO STOP */
    int ret_value = 0;

    int num_cmds;
    struct cmd *cmds = retrieve_cmds(job, &num_cmds);
    if (!cmds) {
        printf("Failed to retrieve cmds\n");
        ret_value = 1;
        goto end;
    }

    /* DEBUG INFO START */
    /*
    printf("num_cmds=%d\n", num_cmds);
    for (int i = 0; i < num_cmds; ++i) {
        printf("(%d, argc=%d, name=%s, output_file=%s, is_append=%d) ", 
                i, cmds[i].argc, cmds[i].name, cmds[i].output_fname, cmds[i].output_append);

        for (int j = 0; j < cmds[i].argc; ++j)
            printf(" {%s}", cmds[i].args[j]);
        printf("\n");
    }
    */
    /* DEBIG INFO STOP */

    int input_size;
    char *input_buf;
    STRINIT(input_buf, input_size);

    for (int i = 0; i < num_cmds; ++i) {
        // terminate args with null-pointer by convention
        cmds[i].args = realloc(cmds[i].args, sizeof(char *) * (++cmds[i].argc));
        cmds[i].args[cmds[i].argc - 1] = NULL;

        int pin_fd[2];
        int pout_fd[2];

        if (pipe(pin_fd)) {
            printf("Failed to create pipe");
            ret_value = 1;
            goto end;
        }

        if (pipe(pout_fd)) {
            printf("Failed to create pipe");
            ret_value = 1;
            goto end;
        }

        int pid = fork();
        if (!pid) {
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(pin_fd[1]);
            close(pout_fd[0]);
            if (dup2(pin_fd[0], STDIN_FILENO) == -1) {
                perror("dup2 stdin");
                ret_value = 1;
                goto end;
            }
            if (dup2(pout_fd[1], STDOUT_FILENO) == -1) {
                perror("dup2 stdout");
                ret_value = 1;
                goto end;
            }

            execvp(cmds[i].name, cmds[i].args);
        } else if (pid > 0) {
            close(pin_fd[0]);
            close(pout_fd[1]);


            if (write(pin_fd[1], input_buf, input_size) == -1) {
                perror("Failed to write to pipe");
                goto end;
            }
            close(pin_fd[1]);
        
            STRRESET(input_buf, input_size);
            char c;
            while (read(pout_fd[0], &c, sizeof(char)) > 0)
                STRAPPEND(input_buf, c, input_size);
            close(pout_fd[0]);
        
            wait(NULL);
        } else {
            printf("run_job: Failed to fork\n");
            close(pin_fd[0]);
            close(pout_fd[0]);
            close(pin_fd[1]);
            close(pout_fd[1]);
            ret_value = 1;
            goto end;
        }
    }

    printf("%s", input_buf);

end:
    free_cmds(cmds, num_cmds);
    return ret_value;    
}
