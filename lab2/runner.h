#ifndef RUNNER_H
#define RUNNER_H

#include "parser.h"

int chain_jobs(struct shell_job *jobs, int num_jobs);

int run_job(struct shell_job *job);

int builtin_cd(char **argv, int argc);

int builtin_exit(char **argv, int argc);

#endif /* RUNNER_H */