#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "lab.h"

// Environment variables
char *MY_PROMPT;
char *HOME_DIR;

// Constants
long ARG_MAX; // number of arguments allowed per command
const int MAX_STR_LENGTH = 20; // max size/length of argument

void print_version() {
    printf("Version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
}

void shell_exit() {
    int exit_status = EXIT_SUCCESS;
    exit(exit_status);
}

void cd_command(char *command_arg) {
    char *toDir = (strlen(command_arg) == 0) ? HOME_DIR : command_arg;
    chdir(toDir);
    switch (errno) {
        case 0:
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

char** stripCommandArguments(char* line) {
    // Skip beginning
    size_t i = 0;
    while (i < strlen(line)) {
        if (line[i] == ' ') break;
        i++;
    }
    i++;
    // If no arguments
    if (i >= strlen(line)) return NULL;
    size_t remaining_length = strlen(line) - i;

    // TODO: Get total number of arguments to not overallocate.


    // allocate retVal
    char **args = (char**) malloc(ARG_MAX * sizeof(char*));
    for (int i = 0; i < ARG_MAX; i++) {
        args[i] = (char*) malloc((MAX_STR_LENGTH + 1) * sizeof(char));
    }

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
    return args;
}

void freeCommandArgs(char** commandArgs) {
    if (commandArgs == NULL) return;
    for (int i = 0; i < ARG_MAX; i++) {
        free(commandArgs[i]);
    }
    free(commandArgs);
}   

void handle_shell_line(char *line) {
    char *line_copy = strdup(line);
    for (int i = 0; line_copy[i]; i++) line_copy[i] = tolower(line_copy[i]);
    char* stripped = stripBeginEndWhitespace(line_copy);
    free(line_copy);

    char *command = stripCommand(stripped);
    char **command_args = stripCommandArguments(stripped);   // command_args is "" if none

    if (strcmp(command, "exit") == 0 /*|| feof(stdin)*/) {     // TODO FIX feof to exit on EOF
        free(command);
        freeCommandArgs(command_args);
        free(stripped);
        free(line);
        shell_exit();
    }
    
    else if (strcmp(command, "cd") == 0) {
        //cd_command(command_args);
    }

    else if (strcmp(command, "history") == 0) {
        register HIST_ENTRY **list = history_list();
        if (list) for (int i = 0; list[i]; i++) printf ("%s\n", list[i]->line);
    }

    free(stripped);
    free(command);
    freeCommandArgs(command_args);
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

void set_envionrment_variables() {
    HOME_DIR = getenv("MY_PROMPT") ? getenv("MY_PROMPT") : getpwuid(getuid())->pw_dir;
    MY_PROMPT = getenv("MY_PROMPT") ? getenv("MY_PROMPT") : "$ ";
    ARG_MAX = sysconf(_SC_ARG_MAX);
}

int main(int argc, char *argv[]) {
    set_envionrment_variables();

    printf("%s", HOME_DIR);
    
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