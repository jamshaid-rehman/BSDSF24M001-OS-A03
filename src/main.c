#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        // Handle !n BEFORE tokenizing or adding to history
        if (cmdline[0] == '!') {
            int n = atoi(cmdline + 1);
            if (n < 1 || n > history_count) {
                fprintf(stderr, "!%s: event not found\n", cmdline + 1);
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(history[n - 1]);
            printf("%s\n", cmdline);
        }

        // Store non-empty commands in history
        int blank = 1;
        for (int i = 0; cmdline[i] != '\0'; i++) {
            if (cmdline[i] != ' ' && cmdline[i] != '\t' && cmdline[i] != '\n') {
                blank = 0;
                break;
            }
        }
        if (!blank)
            add_to_history(cmdline);

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0) {
                execute(arglist);
            }
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
