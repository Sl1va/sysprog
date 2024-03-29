#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>

#include "parser.h"
#include "runner.h"

int chain_jobs(struct shell_job *_jobs, int _num_jobs) {

    int num_jobs = 0;
    struct shell_job *jobs = NULL;

    for (int k = 0; k < _num_jobs; ++k) {
        jobs = realloc(jobs, sizeof(struct shell_job) * (++num_jobs));
        
        char **job_tokens = malloc(sizeof(char *) * _jobs[k].num_tokens);
        for (int j = 0; j < _jobs[k].num_tokens; ++j)
            job_tokens[j] = strdup(_jobs[k].tokens[j]);

        jobs[num_jobs - 1] = (struct shell_job) {
            .tokens = job_tokens,
            .num_tokens = _jobs[k].num_tokens,
            .operator = _jobs[k].operator,
        };

        if (jobs[k].operator == OP_BG) {

            int pid = fork();

            if (!pid) {
                setsid(); // make bg proces group leader (to avoid zombies)

                // this buch of jobs is executing in fork
                for (int i = 0; i < num_jobs; ++i) {
                    if (run_job(&jobs[i])) {
                        if (jobs[i].operator != OP_OR) 
                            exit(1);
                        else ++i;
                    } 
                }

                exit(0);
            } else if (pid < 0) {
                printf("chain_jobs: Failed to fork\n");
                free_jobs(jobs, num_jobs);
                return 1;   
            }

            // clear jobs bunch
            free_jobs(jobs, num_jobs);
            jobs = NULL;
            num_jobs = 0; 
        }
    }

    if (jobs) {
        for (int i = 0; i < num_jobs; ++i) {
            if (run_job(&jobs[i])) {
                if (jobs[i].operator == OP_AND) {
                    free_jobs(jobs, num_jobs);
                    return 1;
                }
            } else if (jobs[i].operator == OP_OR) ++i; 
        }
    }

    free_jobs(jobs, num_jobs);
    return 0;
}

int run_job(struct shell_job *job) {
    
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
    
    // handle explicitly if last command of pipe is exit
    if (num_cmds == 1 && !strcmp(cmds[0].name, "exit"))
        builtin_exit(cmds[0].args, cmds[0].argc);

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

            if (!strcmp(cmds[i].name, "exit")) {
                if (i > 0)
                    close(fds[i - 1][0]);
                close(fds[i][1]);
                builtin_exit(cmds[i].args, cmds[i].argc);
            }

            if (i > 0) {    
                if (dup2(fds[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    exit(1);
                }
            }

            if (cmds[i].output_fname) {
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
                    exit(1);
                }

                if (dup2(outfd, STDOUT_FILENO) == -1) {
                    perror("dup2 file redirect");
                    exit(1);
                }
            } else if (i < num_cmds - 1) {
                if (dup2(fds[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    exit(1);
                }
            }
            execvp(cmds[i].name, cmds[i].args);

            // execvp failed
            exit(1);
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
        
        int term_id = -1;
        for (int j = 0; j < num_cmds; ++j)
            if (pid == pids[j])
                term_id = j;
        
        if (term_id == num_cmds - 1)
            ret_value = status;

        if (term_id == -1) continue;

        close(fds[term_id][1]);
        close(fds[term_id][0]);

        pids[term_id] = -1;

        for (int j = term_id - 1; j >= 0; --j) {
            if (pids[j] == -1) continue;

            close(fds[j][1]);
            close(fds[j][0]);
            pids[j] = -1;
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

int builtin_exit(char **argv, int argc) {
    if (argc < 2)
        exit(0);

    int ret_code = 0;
    sscanf(argv[1], "%d", &ret_code);
    exit(ret_code);
    
    return 0;
}