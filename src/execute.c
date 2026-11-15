#include "shell.h"
#include <fcntl.h>

// ---------- History storage ----------
char* history[HISTORY_SIZE];
int history_count = 0;

void add_to_history(const char* cmd) {
    if (history_count == HISTORY_SIZE) {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++)
            history[i - 1] = history[i];
        history_count--;
    }
    history[history_count++] = strdup(cmd);
}

// ---------- I/O redirection ----------
// Runs inside a child process. Finds "<" and ">" in argv, opens the file,
// uses dup2() to replace stdin/stdout, and removes those tokens from argv.
static int apply_redirections(char** argv) {
    int j = 0;
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "<") == 0 || strcmp(argv[i], ">") == 0) {
            int is_input = (argv[i][0] == '<');
            if (argv[i + 1] == NULL) {
                fprintf(stderr, "syntax error: missing filename after %s\n", argv[i]);
                return -1;
            }
            int fd;
            if (is_input)
                fd = open(argv[i + 1], O_RDONLY);
            else
                fd = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror(argv[i + 1]);
                return -1;
            }
            dup2(fd, is_input ? STDIN_FILENO : STDOUT_FILENO);
            close(fd);                  // the original descriptor is no longer needed
            i++;                        // skip the filename
        } else {
            argv[j++] = argv[i];        // keep normal arguments
        }
    }
    argv[j] = NULL;
    return 0;
}

// ---------- External command execution (with pipes and redirection) ----------
int execute(char* arglist[]) {
    // 1) Validate the pipe syntax and count the commands
    int ncmds = 1;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            if (i == 0 || arglist[i + 1] == NULL || strcmp(arglist[i + 1], "|") == 0) {
                fprintf(stderr, "syntax error near |\n");
                return 0;
            }
            ncmds++;
        }
    }

    int launched = 0;
    int prev_read = -1;                 // read end of the previous pipe
    int start = 0;

    // 2) Start one child per command
    for (int c = 0; c < ncmds; c++) {
        int end = start;
        while (arglist[end] != NULL && strcmp(arglist[end], "|") != 0)
            end++;
        int len = end - start;

        // private copy of this command's argument pointers
        char** argv = malloc((len + 1) * sizeof(char*));
        for (int k = 0; k < len; k++)
            argv[k] = arglist[start + k];
        argv[len] = NULL;

        int fd[2] = {-1, -1};
        if (c < ncmds - 1 && pipe(fd) < 0) {
            perror("pipe");
            free(argv);
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            free(argv);
            break;
        }

        if (pid == 0) {                 // ----- child -----
            if (prev_read != -1) {      // read from the previous command
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (fd[1] != -1) {          // write into the next command
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }
            if (apply_redirections(argv) < 0)
                exit(1);
            if (argv[0] == NULL)
                exit(1);
            execvp(argv[0], argv);
            perror("Command not found");
            exit(1);
        }

        // ----- parent: close the pipe ends it does not need -----
        if (prev_read != -1) close(prev_read);
        if (fd[1] != -1) close(fd[1]);
        prev_read = fd[0];
        launched++;
        free(argv);
        start = end + 1;
    }

    if (prev_read != -1) close(prev_read);

    // 3) Wait for all children
    for (int i = 0; i < launched; i++)
        wait(NULL);

    return 0;
}

// ---------- Built-in commands ----------
int handle_builtin(char** arglist) {
    if (arglist[0] == NULL)
        return 1;

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL)
            fprintf(stderr, "cd: missing directory argument\n");
        else if (chdir(arglist[1]) != 0)
            perror("cd");
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <directory>  - change the current directory\n");
        printf("  exit            - exit the shell\n");
        printf("  help            - show this help message\n");
        printf("  jobs            - show background jobs (not yet implemented)\n");
        printf("  history         - show the list of previous commands\n");
        printf("  !n              - re-run the n-th command from history\n");
        printf("Operators: <  >  |  (put spaces around them)\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        for (int i = 0; i < history_count; i++)
            printf("%d  %s\n", i + 1, history[i]);
        return 1;
    }

    return 0;
}
