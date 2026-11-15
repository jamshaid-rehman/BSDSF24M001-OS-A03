#include "shell.h"

// ---------- Variable storage: linked list of key-value pairs ----------
typedef struct Var {
    char* name;
    char* value;
    struct Var* next;
} Var;

static Var* var_head = NULL;

static Var* find_var(const char* name) {
    for (Var* v = var_head; v != NULL; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v;
    return NULL;
}

// Creates the variable, or updates it if it already exists
void set_variable(const char* name, const char* value) {
    Var* v = find_var(name);
    if (v != NULL) {
        free(v->value);
        v->value = strdup(value);
        return;
    }
    v = malloc(sizeof(Var));
    v->name = strdup(name);
    v->value = strdup(value);
    v->next = var_head;
    var_head = v;
}

const char* get_variable(const char* name) {
    Var* v = find_var(name);
    return v ? v->value : NULL;
}

// Prints every variable as NAME=value
void print_variables(void) {
    if (var_head == NULL) {
        printf("No variables defined.\n");
        return;
    }
    for (Var* v = var_head; v != NULL; v = v->next)
        printf("%s=%s\n", v->name, v->value);
}

// A token is an assignment if it looks like NAME=value:
// starts with a letter or '_', then letters/digits/'_', then '='
int is_assignment(const char* tok) {
    if (!(isalpha((unsigned char)tok[0]) || tok[0] == '_'))
        return 0;
    int i = 1;
    while (isalnum((unsigned char)tok[i]) || tok[i] == '_')
        i++;
    return tok[i] == '=';
}

// Replaces every argument that starts with '$' by the variable's value.
// An undefined variable becomes an empty string.
void expand_variables(char** arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        if (arglist[i][0] == '$' && arglist[i][1] != '\0') {
            const char* val = get_variable(arglist[i] + 1);
            char* expanded = strdup(val ? val : "");
            free(arglist[i]);               // free the old string, keep the new one
            arglist[i] = expanded;
        }
    }
}
