#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

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
                return 1;
        } else if (jobs[i].operator == OP_OR) ++i;
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
    int (*fds)[2] = NULL;
    int *pids;

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

    pids = malloc(sizeof(int) * num_cmds);
    fds = malloc(sizeof(int[2]) * num_cmds);

    for (int i = 0; i < num_cmds; ++i) {
        if (pipe(fds[i])) {
            printf("Failed to initialize pipe\n");
            ret_value = 1;
            goto end;
        }
    }

    for (int i = 0; i < num_cmds; ++i) {
        // terminate args with null-pointer by convention
        cmds[i].args = realloc(cmds[i].args, sizeof(char *) * (++cmds[i].argc));
        cmds[i].args[cmds[i].argc - 1] = NULL;

        // process builtins
        if (!strcmp(cmds[i].name, "cd")) {
            builtin_cd(cmds[i].args, cmds[i].argc);
            continue;
        }

        int pid = fork();

        if (!pid) {
            // child
            
            // MUST close not used pipes here
            for (int j = 0; j < num_cmds; ++j) {
                if (j != i - 1)
                    close(fds[j][0]);
                if (j != i)
                    close(fds[j][1]);
            }

            if (i > 0) {    
                close(STDIN_FILENO);
                if (dup2(fds[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    ret_value = 1;
                    goto end;
                }
            }

            if (cmds[i].output_fname) {
                close(STDOUT_FILENO);
                close(fds[i][1]);

                int flags = O_WRONLY | O_CREAT;
                if (cmds[i].output_append)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;
                
                // mode: rw.-rw.-r..
                int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

                int outfd = open(cmds[i].output_fname, flags, mode);

                if (outfd == -1) {
                    perror("redirect to file");
                    ret_value = 1;
                    goto end;
                }

                if (dup2(outfd, STDOUT_FILENO) == -1) {
                    perror("dup2 file redirect");
                    ret_value = 1;
                    goto end;
                }
            } else if (i < num_cmds - 1) {
                close(STDOUT_FILENO);
                if (dup2(fds[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    ret_value = 1;
                    goto end;
                }
            }
            execvp(cmds[i].name, cmds[i].args);

            // execvp failed
            ret_value = 1;
            goto end;
        } else if (pid > 0) {
            // parent
            pids[i] = pid;
        } else {
            // error
            printf("Failed to fork\n");
            ret_value = 1;
            goto end;
        }
    }

    for (int i = 0; i < num_cmds; ++i) {
        int status;
        int pid = wait(&status);
        if (status && !ret_value)
            ret_value = status;
        
        for (int j = 0; j < num_cmds; ++j){
            if (pid == pids[j]) {
                // printf("Closing write pipe for %d\n", j);
                close(fds[j][1]);
                close(fds[j][0]);
            }
        }
    }

end:
    for (int i = 0; i < num_cmds; ++i) {
        close(fds[i][0]);
        close(fds[i][1]);
    }

    free(fds);
    free(pids);
    free_cmds(cmds, num_cmds);
    return ret_value;    
}

int builtin_cd(char **argv, int argc) {
    if (argc < 2)
        return 1;
    
    chdir(argv[1]);
    return 0;   
}