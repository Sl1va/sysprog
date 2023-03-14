#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"

// define states for FSA
#define S0 0 /* Initial state */
#define S1 1
#define S2 2
#define S3 3
#define S4 4
#define S5 5
#define S6 6
#define S7 7
#define S8 8
#define S9 9

// define final states for FSA
#define S_FLUSH 100 /* Flush */
#define S_DROP 101 /* Drop */
#define S_ERR 102 /* Error */

// define transitions for FSA
#define T_LET 0 /* Letter/Digit */
#define T_DEL 1 /* Delimeter (space or tab) */
#define T_AND 2 /* & */
#define T_OR 3 /* | */
#define T_ARR 4 /* > */
#define T_SQUOTE 5 /* ' */
#define T_DQUOTE 6 /* " */


char *read_cmdline(FILE *instream, unsigned int *size) {
    char *cmdline;
    STRINIT(cmdline, (*size));

    char c;
    char prev = '\0';
    bool sq = false, dq = false; // quotes

    while (c = fgetc(instream), c != EOF && (c != '\n' || prev == '\\' || sq || dq)) {
        if (c != '\n' || prev != '\\')
            STRAPPEND(cmdline, c, (*size));
        
        if (c == '\n' && prev == '\\')
            cmdline = realloc(cmdline, sizeof(char *) * (--*size));
        
        if (!sq && !dq) {
            if (c == '\'')
                sq = true;
            else if (c == '"')
                dq = true;
        } else if (sq && c == '\'')
            sq = false;
        else if (dq && c == '"')
            dq = false;

        prev = c;
    }

    // printf("%s\n", cmdline);
    if (c != EOF)
        return cmdline;
    else {
        free(cmdline);
        return NULL;
    }
}

int fsa[][7] = {
    [S0] = {[T_LET] = S1, [T_DEL] = S_DROP, [T_AND] = S2, [T_OR] = S4,
            [T_ARR] = S6, [T_SQUOTE] = S8, [T_DQUOTE] = S9},

    [S1] = {[T_LET] = S1, [T_DEL] = S_FLUSH, [T_AND] = S_FLUSH, [T_OR] = S_FLUSH,
            [T_ARR] = S_FLUSH, [T_SQUOTE] = S_FLUSH, [T_DQUOTE] = S_FLUSH},

    [S2] = {[T_LET] = S_FLUSH, [T_DEL] = S_FLUSH, [T_AND] = S3, [T_OR] = S_ERR,
            [T_ARR] = S_ERR, [T_SQUOTE] = S_FLUSH, [T_DQUOTE] = S_FLUSH},

    [S3] = {[T_LET] = S_FLUSH, [T_DEL] = S_FLUSH, [T_AND] = S_ERR, [T_OR] = S_ERR,
            [T_ARR] = S_ERR, [T_SQUOTE] = S_FLUSH, [T_DQUOTE] = S_FLUSH},

    [S4] = {[T_LET] = S_FLUSH, [T_DEL] = S_FLUSH, [T_AND] = S_ERR, [T_OR] = S5,
            [T_ARR] = S_ERR, [T_SQUOTE] = S_FLUSH, [T_DQUOTE] = S_FLUSH},

    [S5] = {[T_LET] = S_FLUSH, [T_DEL] = S_FLUSH, [T_AND] = S_ERR, [T_OR] = S_ERR,
            [T_ARR] = S_ERR, [T_SQUOTE] = S_FLUSH, [T_DQUOTE] = S_FLUSH},
            
    [S6] = {[T_LET] = S_FLUSH, [T_DEL] = S_FLUSH, [T_AND] = S_ERR, [T_OR] = S_ERR,
            [T_ARR] = S7, [T_SQUOTE] = S_FLUSH, [T_DQUOTE] = S_FLUSH},

    [S7] = {[T_LET] = S_FLUSH, [T_DEL] = S_FLUSH, [T_AND] = S_ERR, [T_OR] = S_ERR,
            [T_ARR] = S_ERR, [T_SQUOTE] = S_FLUSH, [T_DQUOTE] = S_FLUSH},

    [S8] = {[T_LET] = S8, [T_DEL] = S8, [T_AND] = S8, [T_OR] = S8,
            [T_ARR] = S8, [T_SQUOTE] = S_FLUSH, [T_DQUOTE] = S8},

    [S9] = {[T_LET] = S9, [T_DEL] = S9, [T_AND] = S9, [T_OR] = S9,
            [T_ARR] = S9, [T_SQUOTE] = S9, [T_DQUOTE] = S_FLUSH},
};


char **cmdline_tokens(const char *_cmdline, unsigned int size, int *num_tokens) {
    // cmdline basic delimeters are spaces and tabs
    // then logical operators (&& and ||) and pipes (|)
    // and bg signature (&)

    // single quotes and double quotes force to interpret
    // the whole string as a single argument even if
    // there delimeter symbols there.

    // backslash (\) is considered as end symbol and 
    // forces not to interpret next symbol anyhow
    // (works even inside quotes)

    *num_tokens = 0;
    char **tokens = (char **) malloc(sizeof(char*) * (*num_tokens));

    // a little trick to make it easier to process clauses
    char *cmdline = strdup(_cmdline);
    STRAPPEND(cmdline, ' ', size);

    int token_size;
    char *token_buf;

    STRINIT(token_buf, token_size);

    int cur_state = S0;

    for (int i = 0; i < size; ++i) {
        int transition;
        
        switch (cmdline[i]) {
            case '#':
                if (cur_state == S0) goto end;
                transition = T_LET;
                break;

            case '\'':
                transition = T_SQUOTE;
                if (cur_state == S8 || cur_state == S0)
                    ++i;
                break;
            
            case '"':
                transition = T_DQUOTE;
                if (cur_state == S9 || cur_state == S0) {
                    if (cmdline[i + 1] == '\\') i += 2; // safe (see trick at the begining)
                    else ++i;
                }
                break;

            case ' ':
            case '\t':
                transition = T_DEL;
                break;
            
            case '&':
                transition = T_AND;
                break;
            
            case '|':
                transition = T_OR;
                break;
            
            case '>':
                transition = T_ARR;
                break;
            
            case '\\':
                ++i;
            default:
                transition = T_LET;
        }

        int next_state = fsa[cur_state][transition];

        if (next_state == S_FLUSH) {
            tokens = (char **) realloc(tokens, sizeof(char *) * (++*num_tokens));
            tokens[*num_tokens - 1] = strdup(token_buf);
            STRRESET(token_buf, token_size);
            cur_state = S0;
            --i;
        } else if (next_state == S_DROP) {
            cur_state = S0;
        } else if (next_state == S_ERR) { 
            free_tokens(tokens, *num_tokens);

            tokens = NULL;
            
            goto end;
        } else {
            cur_state = next_state;
            STRAPPEND(token_buf, cmdline[i], token_size);
        }
    }

end:
    free(token_buf);
    free(cmdline);

    return tokens;
}

void free_tokens(char **tokens, int num_tokens) {
    if (tokens) {
        for (int i = 0; i < num_tokens; ++i)
            free(tokens[i]);
        free(tokens);
    }
}


struct shell_job *retrieve_jobs(char **_tokens, int num_tokens, int *num_jobs) {
    if (!num_tokens)
        return NULL;
    
    // copy tokens for more flexibility
    char **tokens = (char **) malloc(sizeof(char *) * num_tokens);
    for (int i = 0; i < num_tokens; ++i) {
        tokens[i] = strdup(_tokens[i]);
    }

    // if job is not defined explicitly, make it simple fg
    if (strcmp(tokens[num_tokens - 1], "&") && strcmp(tokens[num_tokens - 1], "&&") && strcmp(tokens[num_tokens - 1], "||")) {
        tokens = realloc(tokens, sizeof(char *) * (++num_tokens));
        tokens[num_tokens - 1] = strdup("&&");
    }
    
    *num_jobs = 0;
    struct shell_job *jobs = (struct shell_job *) malloc(sizeof(struct shell_job) * (*num_jobs));

    int job_num_tokens = 0;
    char **job_tokens = (char **) malloc(sizeof(char *) * job_num_tokens);
    for (int i = 0; i < num_tokens; ++i) {
        const char *token = tokens[i];

        if (!strcmp(token, "&&") || !strcmp(token, "&") || !strcmp(token, "||")) {
            enum job_operator operator = OP_AND;

            if (!strcmp(token, "&"))
                operator = OP_BG;
            else if (!strcmp(token, "||"))
                operator = OP_OR;

            jobs = realloc(jobs, sizeof(struct shell_job) * (++*num_jobs));
            jobs[*num_jobs - 1] = (struct shell_job) {
                .tokens = job_tokens,
                .num_tokens = job_num_tokens,
                .operator = operator,
            };

            job_num_tokens = 0;
            job_tokens = NULL;
        } else {
            job_tokens = realloc(job_tokens, sizeof(char *) * (++job_num_tokens));
            job_tokens[job_num_tokens - 1] = strdup(token);
        }

    }

    free_tokens(tokens, num_tokens);

    return jobs;
}

void free_jobs(struct shell_job *jobs, int num_jobs) {
    if (jobs) {
        for (int i = 0; i < num_jobs; ++i) {
            for (int j = 0; j < jobs[i].num_tokens; ++j)
                free(jobs[i].tokens[j]);
            free(jobs[i].tokens);
        }
        free(jobs);
    }
}

struct cmd *retrieve_cmds(struct shell_job *_job, int *num_cmds) {
    /* "small" trick to prevent expression losing statements */ 
    int _job_num_tokens = _job->num_tokens;
    char **_job_tokens = (char**) malloc(sizeof(char*) * _job_num_tokens);

    for (int i = 0; i < _job_num_tokens; ++i)
        _job_tokens[i] = strdup(_job->tokens[i]);

    if (strcmp(_job_tokens[_job_num_tokens - 1], "|")){
        _job_tokens = realloc(_job_tokens, sizeof(char *) * (++_job_num_tokens));
        _job_tokens[_job_num_tokens - 1] = strdup("|");
    }
    
    struct shell_job *job = (struct shell_job *) malloc(sizeof(struct shell_job));
    *job = (struct shell_job) {
        .tokens = _job_tokens,
        .num_tokens = _job_num_tokens,
        .operator = _job->operator,
    };

    *num_cmds = 0;
    struct cmd *cmds = (struct cmd *) malloc(sizeof(struct cmd) * (*num_cmds));

    int cmd_len = 0;
    char **cur_cmd = (char **) malloc(sizeof(char *) * cmd_len);

    char *out_fname = NULL;
    bool out_append = false;

    for (int i = 0; i < job->num_tokens; ++i) {
        const char *token = job->tokens[i];

        if (!strcmp(token, ">") || !strcmp(token, ">>")) {
            if (i == job->num_tokens - 1) {
                free_cmds(cmds, *num_cmds);
                *num_cmds = 0;
                cmds = NULL;
                goto end;
            }

            ++i;
            if (out_fname)
                free(out_fname);
            out_fname = strdup(job->tokens[i]);

            if (!strcmp(token, ">>"))
                out_append = true;
            else
                out_append = false;

        } else if (!strcmp(token, "|")) {
            if (i == 0) {
                free_cmds(cmds, *num_cmds);
                *num_cmds = 0;
                cmds = NULL;
                goto end;
            }

            cmds = realloc(cmds, sizeof(struct cmd) * (++*num_cmds));
            
            char *_out_fname = NULL;
            if (out_fname)
                _out_fname = strdup(out_fname);

            cmds[*num_cmds - 1] = (struct cmd) {
                .name = cur_cmd[0],
                .args = cur_cmd,
                .argc = cmd_len,
                .output_fname = _out_fname,
                .output_append = out_append,
            };

            cmd_len = 0;
            cur_cmd = NULL;
            free(out_fname);
            out_fname = NULL;
            out_append = false;

        } else {
            cur_cmd = realloc(cur_cmd, sizeof(char *) * (++cmd_len));
            cur_cmd[cmd_len - 1] = strdup(token);
        }
    }

end:
    for (int i = 0; i < cmd_len; ++i)
        free(cur_cmd[i]);
    free(cur_cmd);

    free(out_fname);
    
    free_jobs(job, 1);
    return cmds;
}


void free_cmds(struct cmd *cmds, int num_cmds) {
    if (!cmds)
        return;

    for (int i = 0; i < num_cmds; ++i) {
        for (int j = 0; j < cmds[i].argc; ++j)
            free(cmds[i].args[j]);
        
        free(cmds[i].args);

        if (cmds[i].output_fname)
            free(cmds[i].output_fname);
    }
    free(cmds);
}