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
const char *MY_PROMPT;
const char *HOME_DIR;

// Constants
const int ARG_MAX = 2;   // number of arguments allowed per command

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

char* stripCommandArguments(char* line) {
    size_t i = 0;
    while (i < strlen(line)) {
        if (line[i] == ' ') break;
        i++;
    }
    i++;
    if (i >= strlen(line)) return strdup("");
    size_t remaining_length = strlen(line) - i;
    char* command_args = malloc((remaining_length + 1) * sizeof(char));

    if (command_args == NULL) {
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }

    size_t j;
    for (j = 0; j < remaining_length; j++) {
        command_args[j] = line[i + j];
    }
    command_args[remaining_length] = '\0';
    return command_args;
}

void handle_shell_line(char *line) {
    char *line_copy = strdup(line);
    for (int i = 0; line_copy[i]; i++) line_copy[i] = tolower(line_copy[i]);
    char* stripped = stripBeginningWhitespace(line_copy);
    free(line_copy);

    char *command = stripCommand(stripped);
    char *command_args = stripCommandArguments(stripped);   // command_args is "" if none

    if (strcmp(command, "exit") == 0 /*|| feof(stdin)*/) {     // TODO FIX feof to exit on EOF
        free(command);
        free(command_args);
        free(stripped);
        free(line);
        shell_exit();
    }
    
    else if (strcmp(command, "cd") == 0) {
        cd_command(command_args);
    }

    else if (strcmp(command, "history") == 0) {
        register HIST_ENTRY **list = history_list();
        if (list) for (int i = 0; list[i]; i++) printf ("%s\n", list[i]->line);
    }

    free(stripped);
    free(command);
    free(command_args);
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