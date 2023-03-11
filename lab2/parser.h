#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdbool.h>


#define STRINIT(str, sz_ident)                                  \
    do {                                                        \
        sz_ident = 0;                                           \
        str = (char *) malloc(sizeof(char) * (sz_ident + 1));   \
        str[0] = '\0';                                          \
    } while (0)


#define STRAPPEND(str, c, sz)                                   \
    do {                                                        \
        ++sz;                                                   \
        str = (char*) realloc(str, sizeof(char) * (sz + 1));    \
        str[sz - 1] = c;                                        \
        str[sz] = '\0';                                         \
    } while (0)


#define STRRESET(str, sz_ident) \
    do {                        \
        free(str);              \
        STRINIT(str, sz_ident); \
    } while (0)


struct cmd {
    char *name;
    char **args;
    int argc;

    char *output_fname;
    bool output_append;
};

enum job_operator {
    OP_AND = 0,
    OP_OR,
    OP_BG,
};

struct shell_job {
    char **tokens;
    int num_tokens;
    enum job_operator operator;
};

char *read_cmdline(FILE *instream, unsigned int *size);

char **cmdline_tokens(const char *cmdline, unsigned int size, int *num_tokens);

void free_tokens(char **tokens, int num_tokens);

struct shell_job *retrieve_jobs(char **tokens, int num_tokens, int *num_jobs);

void free_jobs(struct shell_job *jobs, int num_jobs);

struct cmd *retrieve_cmds(struct shell_job *job, int *num_cmds);

void free_cmds(struct cmd *cmds, int num_cmds);

#endif /* PARSER_H */