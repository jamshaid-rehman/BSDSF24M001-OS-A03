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

// ---------- Background job list ----------
#define MAX_JOBS 64
typedef struct {
    pid_t pid;
    char cmd[256];
} Job;

static Job job_list[MAX_JOBS];
static int job_count = 0;

static void add_job(pid_t pid, const char* cmd) {
    if (job_count == MAX_JOBS) {
        fprintf(stderr, "too many background jobs\n");
        return;
    }
    job_list[job_count].pid = pid;
    strncpy(job_list[job_count].cmd, cmd, sizeof(job_list[job_count].cmd) - 1);
    job_list[job_count].cmd[sizeof(job_list[job_count].cmd) - 1] = '\0';
    job_count++;
}

// Returns 1 if the pid was in the list (and removes it), 0 otherwise
static int remove_job(pid_t pid, char* cmd_out, size_t size) {
    for (int i = 0; i < job_count; i++) {
        if (job_list[i].pid == pid) {
            strncpy(cmd_out, job_list[i].cmd, size - 1);
            cmd_out[size - 1] = '\0';
            for (int j = i + 1; j < job_count; j++)
                job_list[j - 1] = job_list[j];
            job_count--;
            return 1;
        }
    }
    return 0;
}

// Non-blocking cleanup of finished background children (prevents zombies)
void reap_jobs(void) {
    int status;
    pid_t pid;
    char cmd[256];
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (remove_job(pid, cmd, sizeof(cmd)))
            printf("[done] %d  %s\n", pid, cmd);
    }
}

// ---------- I/O redirection ----------
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
            close(fd);
            i++;
        } else {
            argv[j++] = argv[i];
        }
    }
    argv[j] = NULL;
    return 0;
}

// ---------- External command execution (pipes, redirection, background) ----------
int execute(char* arglist[], int background, const char* cmdtext) {
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

    pid_t* pids = malloc(ncmds * sizeof(pid_t));
    int launched = 0;
    int prev_read = -1;
    int start = 0;

    for (int c = 0; c < ncmds; c++) {
        int end = start;
        while (arglist[end] != NULL && strcmp(arglist[end], "|") != 0)
            end++;
        int len = end - start;

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
            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (fd[1] != -1) {
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

        // ----- parent -----
        if (prev_read != -1) close(prev_read);
        if (fd[1] != -1) close(fd[1]);
        prev_read = fd[0];
        pids[launched++] = pid;
        free(argv);
        start = end + 1;
    }

    if (prev_read != -1) close(prev_read);

    if (background) {
        // Do NOT wait: remember the last process of the pipeline and return to the prompt
        if (launched > 0) {
            add_job(pids[launched - 1], cmdtext);
            printf("[bg] started pid %d\n", pids[launched - 1]);
        }
    } else {
        // Foreground: wait for exactly our own children
        for (int i = 0; i < launched; i++)
            waitpid(pids[i], NULL, 0);
    }

    free(pids);
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
        printf("  jobs            - list running background jobs\n");
        printf("  history         - show the list of previous commands\n");
        printf("  !n              - re-run the n-th command from history\n");
        printf("Operators: <  >  |  (spaces around them), ; chains commands, & runs in background\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        reap_jobs();                    // drop jobs that already finished
        if (job_count == 0) {
            printf("No background jobs.\n");
        } else {
            for (int i = 0; i < job_count; i++)
                printf("[%d] %d  %s\n", i + 1, job_list[i].pid, job_list[i].cmd);
        }
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        for (int i = 0; i < history_count; i++)
            printf("%d  %s\n", i + 1, history[i]);
        return 1;
    }

    return 0;
}
