#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

struct cmd {
    const char *name;
    const char **args;
    int argc;
};

char *read_cmdline(const char *invite, FILE *instream, FILE *outstream, unsigned int *size);

char **cmdline_tokens(const char *cmdline, unsigned int size, int *num_tokens);

#endif /* PARSER_H */