#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "FCIT> "

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char** arglist, int background, const char* cmdtext);
void reap_jobs(void);

// Shell variables
int is_assignment(const char* tok);
void set_variable(const char* name, const char* value);
const char* get_variable(const char* name);
void print_variables(void);
void expand_variables(char** arglist);
int handle_builtin(char** arglist);

#define HISTORY_SIZE 20
extern char* history[HISTORY_SIZE];
extern int history_count;
void add_to_history(const char* cmd);

#endif // SHELL_H
