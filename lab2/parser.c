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

// define final states for FSA
#define S_F 100 /* Flush */
#define S_D 101 /* Drop */
#define S_E 102 /* Error */

// define transitions for FSA
#define T_L 0 /* Letter/Digit */
#define T_D 1 /* Delimeter (space or tab) */
#define T_A 2 /* & */
#define T_O 3 /* | */
#define T_P 4 /* > */


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


char *read_cmdline(const char *invite, FILE *instream, FILE *outstream, unsigned int *size) {
    char *cmdline;
    STRINIT(cmdline, (*size));

    fprintf(outstream, "%s", invite);

    char c;
    while (c = fgetc(instream), c != EOF && c != '\n')
        STRAPPEND(cmdline, c, (*size));

    if (c != EOF)
        return cmdline;
    else {
        free(cmdline);
        return NULL;
    }
}

int fsa[][5] = {
    [S0] = {[T_L] = S1, [T_D] = S_D, [T_A] = S2, [T_O] = S4, [T_P] = S6},
    [S1] = {[T_L] = S1, [T_D] = S_F, [T_A] = S_F, [T_O] = S_F, [T_P] = S_F},
    [S2] = {[T_L] = S_F, [T_D] = S_F, [T_A] = S3, [T_O] = S_E, [T_P] = S_E},
    [S3] = {[T_L] = S_F, [T_D] = S_F, [T_A] = S_E, [T_O] = S_E, [T_P] = S_E},
    [S4] = {[T_L] = S_F, [T_D] = S_F, [T_A] = S_E, [T_O] = S5, [T_P] = S_E},
    [S5] = {[T_L] = S_F, [T_D] = S_F, [T_A] = S_E, [T_O] = S_E, [T_P] = S_E},
    [S6] = {[T_L] = S_F, [T_D] = S_F, [T_A] = S_E, [T_O] = S_E, [T_P] = S7},
    [S7] = {[T_L] = S_F, [T_D] = S_F, [T_A] = S_E, [T_O] = S_E, [T_P] = S_E},
};


char **cmdline_tokens(const char *_cmdline, unsigned int size, int *num_tokens, int *err) {
    // cmdline basic delimeters are spaces and tabs
    // then logical operators (&& and ||) and pipes (|)
    // and bg signature (&)

    // single quotes and double quotes force to interpret
    // the whole string as a single argument even if
    // there delimeter symbols there.

    // backslash (\) is considered as end symbol and 
    // forces not to interpret next symbol anyhow
    // (works even inside quotes)

    // a little trick to make it easier to process clauses
    char *cmdline = strdup(_cmdline);
    STRAPPEND(cmdline, ' ', size);

    int token_size;
    char *token_buf;

    STRINIT(token_buf, token_size);

    bool single_quote = false;
    bool double_quote = false;

    int cur_state = S0;

    for (int i = 0; i < size; ++i) {
        int transition = T_L;

        // if no quotes are involved, process state via transitions
        if (!single_quote && !double_quote && cmdline[i] != '"' && cmdline[i] != '\'')
            switch (cmdline[i]) {
                case '#':
                    if (cur_state == S0) goto end;
                    transition = T_L;
                    break;

                case ' ':
                case '\t':
                    transition = T_D;
                    break;

                case '&':
                    transition = T_A;
                    break;

                case '|':
                    transition = T_O;
                    break;

                case '>':
                    transition = T_P;
                    break;

                case '\\':
                    ++i;
                default:
                    transition = T_L;
            }

        int next_state = fsa[cur_state][transition];

        // process quotes
        if (!single_quote && !double_quote) {
            if (cmdline[i] == '\'') {
                single_quote = true;
                ++i;
            }
            else if (cmdline[i] == '"') {
                double_quote = true;
                ++i;
            }
        } else if (single_quote && cmdline[i] == '\'') {
            single_quote = false;
            next_state = S_F;
            ++i;
        } else if (double_quote && cmdline[i] == '"') {
            double_quote = false;
            next_state = S_F;
            ++i;
        } else next_state = S0;

        if ((double_quote || single_quote) && cmdline[i] == '\\')
            ++i;
        

        // process next state
        if (next_state == S_F) {
            printf("%d <%s>\n", token_size, token_buf);
            STRRESET(token_buf, token_size);
            cur_state = S0;
            --i;
        } else if (next_state == S_D) {
            cur_state = S0;
        } else if (next_state == S_E) { 
            printf("Parsing error occured!\n");
            goto end;
        } else {
            cur_state = next_state;
            STRAPPEND(token_buf, cmdline[i], token_size);
        }
    }

end:
    free(token_buf);
    free(cmdline);

    // TODO:
    return NULL;
}