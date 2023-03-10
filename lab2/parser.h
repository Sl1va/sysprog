#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdbool.h>

struct cmd {
    const char *name;
    const char **args;
    int argc;
};

struct shell_job {
    char **tokens;
    int num_tokens;
    bool bg;
};

char *read_cmdline(const char *invite, FILE *instream, FILE *outstream, unsigned int *size);

char **cmdline_tokens(const char *cmdline, unsigned int size, int *num_tokens);

void free_tokens(char **tokens, int num_tokens);

struct shell_job *retrieve_jobs(char **tokens, int num_tokens, int *num_jobs);

void free_jobs(struct shell_job *jobs, int num_jobs);

#endif /* PARSER_H */