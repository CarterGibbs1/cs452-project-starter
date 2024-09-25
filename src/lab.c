#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "lab.h"

#define TOOMANYARGS (void*) 1

// Environment variables
char *MY_PROMPT;
char *HOME_DIR;

// Constants
long ARG_MAX; // number of arguments allowed per command
const int MAX_STR_LENGTH = 20; // max size/length of argument
const unsigned int PATH_MAX = 1028;

// Structs
struct CommandArguments {
    unsigned int numArgs;
    char** args;
};

void print_version() {
    printf("Version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
}

void shell_exit() {
    int exit_status = EXIT_SUCCESS;
    exit(exit_status);
}

void cd_command(struct CommandArguments* command_arg) {
    if (command_arg != NULL && command_arg->numArgs != 0 && command_arg->numArgs != 1) {
        fprintf(stderr, "Too many arguments.\n");
        return;
    }
    char *toDir = (command_arg->numArgs == 0) ? HOME_DIR : command_arg->args[0];
    chdir(toDir);
    switch (errno) {
        case 0: ;
            char* cwd = malloc((PATH_MAX) * sizeof(char));
            getcwd(cwd, PATH_MAX);
            printf("%s\n", cwd);
            free(cwd);
            break;
        case ENOTDIR:
        case ENOENT:
            fprintf(stderr, "Directory not found.\n");
            break;
        default:
            fprintf(stderr, "Error not recognized. Error code: %d\n", errno);
            break;
    }
}

void execCommand(char *command, char** argv) {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Forking failed.\n");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        exit(execvp(command, argv));
    } else {
        waitpid(pid, NULL, 0);
    }
}

bool startsWith(char *line, char *prefix) {
    if (strlen(line) < strlen(prefix)) return false;
    for (size_t i = 0; i < strlen(prefix); i++) {
        if (line[i] != prefix[i]) return false;
    }
    return true;
}

char* stripBeginningWhitespace(char* line) {
    size_t i = 0;
    for (i=0; i<strlen(line) && line[i] == ' '; i++);
    char* retVal = malloc((strlen(line) - i + 1) * sizeof(char));
    size_t j;
    for (j=0; j < strlen(line) - i; j++) {
        retVal[j] = line[i+j];
    }
    retVal[j] = '\0';
    return retVal;
}

char* stripEndingWhitespace(char* line) {
    int i;
    for (i=strlen(line)-1; i>=0 && line[i] == ' '; i--);
    i++;
    char* retVal = malloc((i + 1) * sizeof(char));
    strncpy(retVal, line, i);
    retVal[i] = '\0';
    return retVal;
}

char* stripBeginEndWhitespace(char* line) {
    if (strlen(line) == 0) return strdup(line);
    char* strippedBeginning = stripBeginningWhitespace(line);
    char* strippedEndBegin = stripEndingWhitespace(strippedBeginning);
    char* retVal = strdup(strippedEndBegin);
    free(strippedBeginning);
    free(strippedEndBegin);
    return retVal;
}

char* stripCommand(char* line) {
    size_t i = 0;
    while (i < strlen(line)) {
        if (line[i] == ' ') break;
        i++;
    }
    char* command = malloc((i+1) * sizeof(char));

    if (command == NULL) {
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }

    for (size_t j = 0; j < i; j++) command[j] = line[j];
    command[i] = '\0';
    return command;
}

struct CommandArguments* stripCommandArguments(char* line) {
    // Skip beginning
    size_t i = 0;
    while (i < strlen(line)) {
        if (line[i] == ' ') break;
        i++;
    }
    i++;
    // If no arguments
    if (i >= strlen(line)) {
        struct CommandArguments *retVal = malloc(sizeof(struct CommandArguments));
        retVal->args = NULL;
        retVal->numArgs = 0;
        return retVal;
    }
    size_t remaining_length = strlen(line) - i;
    unsigned int curr = i;
    while(curr < strlen(line) && line[curr] == ' ') curr++;

    // Get total number of arguments to not overallocate.
    unsigned int numArgs = 1;
    bool newArg = false;
    while (curr < strlen(line)) {
        if (!newArg && line[curr] == ' ') newArg = true;
        else if (newArg && line[curr] != ' ') {
            numArgs++;
            newArg = false;
        }
        curr++;
    }

    //printf("%d\n", numArgs);
    // Print error if number of arguments provided is more than allowed.
    if (numArgs > ARG_MAX) {
        fprintf(stderr, "Number of arguments provided exceeds maximum number of arguments. %d > %ld\n",
            numArgs, ARG_MAX);
        return TOOMANYARGS;
    }

    struct CommandArguments *retVal = malloc(sizeof(struct CommandArguments));
    retVal->numArgs = numArgs;

    // allocate str[]
    char **args = (char**) malloc(numArgs * sizeof(char*));
    for (unsigned int i = 0; i < numArgs; i++) {
        args[i] = (char*) malloc((MAX_STR_LENGTH + 1) * sizeof(char));
    }
    retVal->args = args;

    //printf("%d\n", retVal->numArgs);

    // Loop through each argument until end.
    size_t j;
    size_t curr_str = 0;
    size_t curr_char = 0;
    for (j = 0; j < remaining_length; j++) {
        // find next arg if not in one
        while (j < remaining_length && curr_char == 0 && line[i + j] == ' ') j++;
        if (j >= remaining_length) break;

        // if we find ' ', then we need to increment to next arg
        if (line[i + j] == ' ') {
            curr_str++;
            curr_char = 0;
            continue;
        }
        //printf("%ld, %ld: %c\n", curr_str, curr_char,line[i + j]);
        // otherwise, keep building current str
        args[curr_str][curr_char] = line[i + j];
        args[curr_str][curr_char+1] = '\0';
        curr_char++;
    }
    return retVal;
}

void freeCommandArgs(struct CommandArguments* comm_args) {
    if (comm_args == NULL) return;
    if (comm_args->args != NULL){
        for (unsigned int i = 0; i < comm_args->numArgs; i++) {
            free(comm_args->args[i]);
        }
        free(comm_args->args);
    }
    free(comm_args);
}   

void handle_shell_line(char *line) {
    char *line_copy = strdup(line);
    for (int i = 0; line_copy[i]; i++) line_copy[i] = tolower(line_copy[i]);
    char* stripped = stripBeginEndWhitespace(line_copy);
    free(line_copy);

    char *command = stripCommand(stripped);
    struct CommandArguments *commandArguments = stripCommandArguments(stripped);   // command_args is NULL if none
    if (commandArguments == TOOMANYARGS) {
        free(command);
        free(stripped);
        return;
    }

    if (strcmp(command, "exit") == 0 /*|| feof(stdin)*/) {     // TODO FIX feof to exit on EOF
        free(command);
        freeCommandArgs(commandArguments);
        free(stripped);
        free(line);
        shell_exit();
    }
    
    else if (strcmp(command, "cd") == 0) {
        cd_command(commandArguments);
    }

    else if (strcmp(command, "history") == 0) {
        register HIST_ENTRY **list = history_list();
        if (list) for (int i = 0; list[i]; i++) printf ("%s\n", list[i]->line);
    }

    else {
        execCommand(command, commandArguments->args);
    }

    free(stripped);
    free(command);
    freeCommandArgs(commandArguments);
}

void shell_loop() {
    char *line;
    using_history();
    while ((line=readline(MY_PROMPT))){
        handle_shell_line(line);
        add_history(line);
        free(line);
    }
}

void set_environment_variables() {
    HOME_DIR = getenv("MY_PROMPT") ? getenv("MY_PROMPT") : getpwuid(getuid())->pw_dir;
    if (HOME_DIR == NULL) {
        fprintf(stderr, "Home directory not found.\n");
        exit(EXIT_FAILURE);
    }
    MY_PROMPT = getenv("MY_PROMPT") ? getenv("MY_PROMPT") : "$ ";
    ARG_MAX = sysconf(_SC_ARG_MAX);
}

int main(int argc, char *argv[]) {
    set_environment_variables();
    
    int opt;
    // Use getopt to parse command-line arguments
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                // Print the version and exit
                print_version();
                return 0;
                break;
            default: break;
        }
    }

    shell_loop();

    return 0;
}