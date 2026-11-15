#include "shell.h"

#define MAX_TOKENS 128

// Kept for compatibility (main now uses readline)
char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* buf = malloc(MAX_LEN);
    if (fgets(buf, MAX_LEN, fp) == NULL) {
        free(buf);
        return NULL;
    }
    return buf;
}

// Splits a line into tokens. Understands "double" and 'single' quotes,
// and treats < > | as separate tokens even without spaces.
char** tokenize(char* cmdline) {
    char** args = malloc(MAX_TOKENS * sizeof(char*));
    int n = 0;
    char* p = cmdline;

    while (*p && n < MAX_TOKENS - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;

        char buf[MAX_LEN];
        int len = 0;

        if (*p == '<' || *p == '>' || *p == '|') {
            buf[len++] = *p++;
        } else {
            char quote = 0;
            while (*p && len < MAX_LEN - 1) {
                if (quote) {
                    if (*p == quote) { quote = 0; p++; }
                    else buf[len++] = *p++;
                } else if (*p == '"' || *p == '\'') {
                    quote = *p++;
                } else if (*p == ' ' || *p == '\t' || *p == '\n' ||
                           *p == '<' || *p == '>' || *p == '|') {
                    break;
                } else {
                    buf[len++] = *p++;
                }
            }
        }
        buf[len] = '\0';
        args[n++] = strdup(buf);
    }
    args[n] = NULL;

    if (n == 0) {
        free(args);
        return NULL;
    }
    return args;
}
