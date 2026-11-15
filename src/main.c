#include "shell.h"
#include <dirent.h>

// ---------- Tab completion for command names ----------
static char* builtins[] = {"cd", "exit", "help", "jobs", "history", NULL};

static char* command_generator(const char* text, int state) {
    static int b;
    static char *pathcopy, *saveptr, *dirname_;
    static DIR* d;
    static size_t len;
    struct dirent* e;

    if (!state) {
        b = 0;
        len = strlen(text);
        free(pathcopy);
        if (d) { closedir(d); d = NULL; }
        const char* p = getenv("PATH");
        pathcopy = strdup(p ? p : "");
        dirname_ = strtok_r(pathcopy, ":", &saveptr);
    }

    while (builtins[b]) {
        const char* name = builtins[b++];
        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }

    while (1) {
        if (!d) {
            if (!dirname_) return NULL;
            d = opendir(dirname_);
            dirname_ = strtok_r(NULL, ":", &saveptr);
            if (!d) continue;
        }
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.' && text[0] != '.') continue;
            if (strncmp(e->d_name, text, len) == 0)
                return strdup(e->d_name);
        }
        closedir(d);
        d = NULL;
    }
}

static char** shell_completion(const char* text, int start, int end) {
    (void)end;
    if (start == 0)
        return rl_completion_matches(text, command_generator);
    return NULL;
}

// ---------- Run ONE command (no ';' inside) ----------
static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n';
}

static void run_one(char* seg) {
    while (is_space(*seg)) seg++;
    size_t len = strlen(seg);
    while (len > 0 && is_space(seg[len - 1])) seg[--len] = '\0';

    int background = 0;
    if (len > 0 && seg[len - 1] == '&') {
        background = 1;
        seg[--len] = '\0';
        while (len > 0 && is_space(seg[len - 1])) seg[--len] = '\0';
    }
    if (len == 0)
        return;

    char* text = strdup(seg);
    char** arglist = tokenize(seg);
    if (arglist != NULL) {
        if (handle_builtin(arglist) == 0)
            execute(arglist, background, text);
        for (int i = 0; arglist[i] != NULL; i++)
            free(arglist[i]);
        free(arglist);
    }
    free(text);
}

// ---------- Split a line on ';' (outside quotes) and run each part in order ----------
static void run_line(const char* line) {
    char* copy = strdup(line);
    char* seg = copy;
    char quote = 0;

    for (char* p = copy; ; p++) {
        if (*p == '"' || *p == '\'') {
            if (!quote) quote = *p;
            else if (quote == *p) quote = 0;
        }
        if ((*p == ';' && !quote) || *p == '\0') {
            int last = (*p == '\0');
            *p = '\0';
            run_one(seg);
            if (last) break;
            seg = p + 1;
        }
    }
    free(copy);
}

int main() {
    char* cmdline;

    rl_attempted_completion_function = shell_completion;

    while (1) {
        reap_jobs();

        cmdline = readline(PROMPT);
        if (cmdline == NULL)
            break;

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

        int blank = 1;
        for (int i = 0; cmdline[i] != '\0'; i++) {
            if (!is_space(cmdline[i])) {
                blank = 0;
                break;
            }
        }
        if (!blank) {
            add_history(cmdline);
            add_to_history(cmdline);
            run_line(cmdline);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
