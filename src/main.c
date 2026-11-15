#include "shell.h"
#include <dirent.h>

// ---------- Tab completion for command names ----------
static char* builtins[] = {"cd", "exit", "help", "jobs", "history", "set", NULL};

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

// ---------- Helpers ----------
static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n';
}

static char* trim(char* s) {
    while (is_space(*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && is_space(s[len - 1])) s[--len] = '\0';
    return s;
}

// ---------- Run ONE command (no ';' inside). Returns its exit status ----------
static int run_one(char* seg) {
    seg = trim(seg);
    size_t len = strlen(seg);

    int background = 0;
    if (len > 0 && seg[len - 1] == '&') {
        background = 1;
        seg[--len] = '\0';
        seg = trim(seg);
        len = strlen(seg);
    }
    if (len == 0)
        return 0;

    int status = 0;
    char* text = strdup(seg);
    char** arglist = tokenize(seg);
    if (arglist != NULL) {
        expand_variables(arglist);          // replace $NAME with its value
        if (handle_builtin(arglist) == 0)
            status = execute(arglist, background, text);
        for (int i = 0; arglist[i] != NULL; i++)
            free(arglist[i]);
        free(arglist);
    }
    free(text);
    return status;
}

// ---------- Split a line on ';' (outside quotes), run each part in order ----------
static int run_line(const char* line) {
    char* copy = strdup(line);
    char* seg = copy;
    char quote = 0;
    int status = 0;

    for (char* p = copy; ; p++) {
        if (*p == '"' || *p == '\'') {
            if (!quote) quote = *p;
            else if (quote == *p) quote = 0;
        }
        if ((*p == ';' && !quote) || *p == '\0') {
            int last = (*p == '\0');
            *p = '\0';
            status = run_one(seg);
            if (last) break;
            seg = p + 1;
        }
    }
    free(copy);
    return status;
}

// ---------- if / then / else / fi ----------
#define MAXL 64

static int starts_with_if(const char* s) {
    while (is_space(*s)) s++;
    return strncmp(s, "if", 2) == 0 && (s[2] == ' ' || s[2] == '\t' || s[2] == '\0');
}

static void free_block(char** b, int n) {
    for (int i = 0; i < n; i++) free(b[i]);
}

static void handle_if(const char* firstline) {
    char* cond[MAXL];  int ncond = 0;
    char* thenb[MAXL]; int nthen = 0;
    char* elseb[MAXL]; int nelse = 0;
    int section = 0;              // 0 = condition, 1 = then block, 2 = else block
    int saw_then = 0, closed = 0;

    const char* rest = firstline;
    while (is_space(*rest)) rest++;
    rest += 2;
    char* tmp = strdup(rest);
    char* t = trim(tmp);
    if (*t) cond[ncond++] = strdup(t);
    free(tmp);

    while (1) {
        char* line = readline("> ");
        if (line == NULL) break;
        char* l = trim(line);

        if (strcmp(l, "fi") == 0) {
            closed = 1;
            free(line);
            break;
        } else if (strcmp(l, "then") == 0) {
            section = 1;
            saw_then = 1;
        } else if (strcmp(l, "else") == 0) {
            if (!saw_then) {
                fprintf(stderr, "syntax error: else without then\n");
                free(line);
                free_block(cond, ncond); free_block(thenb, nthen); free_block(elseb, nelse);
                return;
            }
            section = 2;
        } else if (*l) {
            if (section == 0 && ncond < MAXL)      cond[ncond++] = strdup(l);
            else if (section == 1 && nthen < MAXL) thenb[nthen++] = strdup(l);
            else if (section == 2 && nelse < MAXL) elseb[nelse++] = strdup(l);
        }
        free(line);
    }

    if (!closed || !saw_then || ncond == 0) {
        fprintf(stderr, "syntax error: expected  if <cmd> / then / [else] / fi\n");
    } else {
        int status = 0;
        for (int i = 0; i < ncond; i++)
            status = run_line(cond[i]);

        if (status == 0) {
            for (int i = 0; i < nthen; i++) run_line(thenb[i]);
        } else {
            for (int i = 0; i < nelse; i++) run_line(elseb[i]);
        }
    }

    free_block(cond, ncond);
    free_block(thenb, nthen);
    free_block(elseb, nelse);
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
            if (starts_with_if(cmdline))
                handle_if(cmdline);
            else
                run_line(cmdline);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
