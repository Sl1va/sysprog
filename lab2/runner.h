#ifndef RUNNER_H
#define RUNNER_H

#include "parser.h"

int chain_jobs(struct shell_job *jobs, int num_jobs);

int run_job(struct shell_job *job);

#endif /* RUNNER_H */