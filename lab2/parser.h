#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdbool.h>

struct cmd {
    char *name;
    char **args;
    int argc;

    char *output_fname;
    bool output_append;
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

struct cmd *retrieve_cmds(struct shell_job *job, int *num_cmds);

void free_cmds(struct cmd *cmds, int num_cmds);

#endif /* PARSER_H */