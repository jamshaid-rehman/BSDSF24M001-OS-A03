#include "shell.h"

int execute(char* arglist[]) {
    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            execvp(arglist[0], arglist);
            perror("Command not found"); // This line runs only if execvp fails
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            // printf("Child pid:%d exited with status %d\n", cpid, status >> 8);
            return 0;
    }
}

int handle_builtin(char** arglist) {
    if (arglist[0] == NULL)
        return 1;                        // empty command, nothing to do

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);                         // the shell process ends itself
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL)
            fprintf(stderr, "cd: missing directory argument\n");
        else if (chdir(arglist[1]) != 0)
            perror("cd");                // e.g. "cd: No such file or directory"
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <directory>  - change the current directory\n");
        printf("  exit            - exit the shell\n");
        printf("  help            - show this help message\n");
        printf("  jobs            - show background jobs (not yet implemented)\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;                            // not a built-in
}
