#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <termios.h>
#include "lab.h"

#define TOOMANYARGS (void*) 1

// Environment variables
char *MY_PROMPT;
char *MY_PROMPT_DEFAULT = "shell>";
char *HOME_DIR;

// Constants
long ARG_MAX; // number of arguments allowed per command
const int MAX_STR_LENGTH = 20; // max size/length of argument
const unsigned int PATH_MAX = 1028;

// Structs
// bg_job uses linked_list structure for ease.
struct bg_job {
    int job_num;
    pid_t pid;
    char *command;
    struct bg_job *next;
    bool is_running;
};

struct bg_job *head = NULL;
int num_jobs = 0;

/**
struct CommandArguments {
    unsigned int numArgs;
    char** args;
}; */

// Method Stubs
void print_str_arr(char**, unsigned int);

void print_version() {
    printf("Version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
}

/////

void shell_exit() {
    int exit_status = EXIT_SUCCESS;
    exit(exit_status);
}

void check_background_jobs() {
    struct bg_job *curr = head;
    while (curr != NULL) {
        int status;
        pid_t result = waitpid(curr->pid, &status, WNOHANG);
        if (result == 0) {
            // traverse
            curr = curr->next;
        } else if (result < 0) {
            curr->is_running = false;;
        }
    }
}

// print all jobs, for jobs command
void print_all_jobs() {
    check_background_jobs();
    struct bg_job *curr = head;
    struct bg_job *prev = NULL;

    while (curr != NULL) {
        if (curr->is_running) {
            printf("[%d] %d Running %s\n", curr->job_num, curr->pid, curr->command);
        } else {
            printf("[%d] Done\t%s\n", curr->job_num, curr->command);
        }
        

        if (curr->is_running) {
            if (prev == NULL) {
                head = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr->command);
            struct bg_job *temp = curr;
            curr = curr->next;
            free(temp);
        } else {
            // Traverse to the next job
            prev = curr;
            curr = curr->next;
        }
    }
}

char* stripBeginningWhitespace(char const *line) {
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

char* stripEndingWhitespace(char const *line) {
    int i;
    for (i=strlen(line)-1; i>=0 && line[i] == ' '; i--);
    i++;
    char* retVal = malloc((i + 1) * sizeof(char));
    strncpy(retVal, line, i);
    retVal[i] = '\0';
    return retVal;
}

char* stripBeginEndWhitespace(char const *line) {
    if (strlen(line) == 0) return strdup(line);
    char* strippedBeginning = stripBeginningWhitespace(line);
    char* strippedEndBegin = stripEndingWhitespace(strippedBeginning);
    char* retVal = strdup(strippedEndBegin);
    free(strippedBeginning);
    free(strippedEndBegin);
    return retVal;
}

char *get_prompt(const char *env) {
    char *choice = getenv(env) ? getenv(env) : MY_PROMPT_DEFAULT;
    char *retVal = strdup(choice);
    return retVal;
}

char* join_strings(char** str_array, int start) {
    // total length
    int total_length = 0;
    int size;
    for (size = 0; str_array[size + start] != NULL; size++) {
        total_length += strlen(str_array[size + start]);
    }
    size++;
    if (size <= 1) {
        return strdup("");
    }
    total_length += (size - start);
    char* result = (char*)malloc(total_length * sizeof(char));
    result[0] = '\0';
    for (int i = 0; i < size - 1; i++) {
        strcat(result, str_array[i + start]);
        if (i < size - 2) {
            strcat(result, " ");
        }
    }
    result[total_length - 1] = '\0';
    return result;
}

void print_str_arr_null_escaped(char** strArr) {
    for (unsigned int i = 0; strArr[i] != NULL; i++) {
        printf("%s, ", strArr[i]);
    }
    printf("\n");
}


int change_dir(char **dir) {
    char *dir_str = join_strings(dir, 1);
    // special cases
    if (strcmp(dir_str, "") == 0) {
        free(dir_str);
        dir_str = strdup(getenv("HOME"));
    }
    chdir(dir_str);
    free(dir_str);
    switch (errno) {
        case 0: ;
            char* cwd = malloc((PATH_MAX) * sizeof(char));
            getcwd(cwd, PATH_MAX);
            //printf("%s\n", cwd);
            free(cwd);
            return 0;
        case ENOTDIR:
        case ENOENT:
            fprintf(stderr, "Directory not found.\n");
            return -1;
        default:
            fprintf(stderr, "Error not recognized. Error code: %d\n", errno);
            return -1;
    }
}

char **cmd_parse(char const *line) {
    if (ARG_MAX == 0) ARG_MAX = sysconf(_SC_ARG_MAX);
    char *line_copy = strdup(line);
    char *strippedLine = trim_white(line_copy);
    if (strlen(strippedLine) == 0) {
        free(strippedLine);
        return NULL;
    }
    // Get total number of arguments to not overallocate.
    size_t curr = 0;
    unsigned int numArgs = 1;
    bool newArg = false;
    while (curr < strlen(strippedLine)) {
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
        free(strippedLine);
        return TOOMANYARGS;
    }
    // allocate str[]
    char **retVal = (char**) malloc((numArgs + 1) * sizeof(char*));
    for (unsigned int i = 0; i < numArgs; i++) {
        retVal[i] = (char*) malloc((MAX_STR_LENGTH + 1) * sizeof(char));
    }
    retVal[numArgs] = NULL;

    //printf("%d\n", retVal->numArgs);
    size_t i;
    size_t curr_str = 0;
    size_t curr_char = 0;
    for (i = 0; i < strlen(strippedLine); i++) {
        // find next arg if not in one
        while (i < strlen(strippedLine) && curr_char == 0 && line[i] == ' ') i++;
        if (i >= strlen(strippedLine)) break;

        // if we find ' ', then we need to increment to next arg
        if (line[i] == ' ') {
            curr_str++;
            curr_char = 0;
            continue;
        }
        //printf("%ld, %ld: %c\n", curr_str, curr_char,line[i + j]);
        retVal[curr_str][curr_char] = line[i];
        retVal[curr_str][curr_char+1] = '\0';
        curr_char++;
    }
    free(strippedLine);
    return retVal;
}

void cmd_free(char ** line) {
    if (line == NULL) return;
    for (unsigned int i = 0; line[i] != NULL; i++) {
        free(line[i]);
    }
    free(line);
}

char *trim_white(char *line) {
    //return stripBeginEndWhitespace(line);
    size_t start = 0;
    while (start < strlen(line) && line[start] == ' ') start++;
    size_t end = 0;
    while (end < strlen(line) && line[strlen(line) - end - 1] == ' ') end++;
    if (start == strlen(line) && end == strlen(line)) {
        line[0] = '\0';
        return line;
    }
    for (size_t i = 0; i < strlen(line) - start; i++) {
        //printf("%d, %d\n", i, start);
        line[i] = line[start + i];
    }
    line[strlen(line) - start - end] = '\0';
    return line;
}

bool do_builtin(struct shell *sh, char **argv) {
    if (strcmp(argv[0],  "exit") == 0) {
        shell_exit();
    }
    else if (strcmp(argv[0],  "cd") == 0) {
        change_dir(argv);
    }
    else if (strcmp(argv[0],  "jobs") == 0) {
        print_all_jobs();
    }
    else if (strcmp(argv[0],  "history") == 0) {
        register HIST_ENTRY **list = history_list();
        if (list) for (int i = 0; list[i]; i++) printf ("%s\n", list[i]->line);
    } else {
        return false;
    }
    return true;
}

void sh_init(struct shell *sh) {
    sh->shell_terminal = STDIN_FILENO;
    sh->shell_is_interactive = isatty(sh->shell_terminal);
    sh->prompt = get_prompt("MY_PROMPT");

    if (sh->shell_is_interactive) {
        /* Loop until we are in the foreground.  */
        while (tcgetpgrp(sh->shell_terminal) != (sh->shell_pgid = getpgrp()))
            kill(sh->shell_pgid, SIGTTIN);

        /* Ignore interactive and job-control signals.  */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);

        /* Put ourselves in our own process group.  */
        sh->shell_pgid = getpid();
        if (setpgid (sh->shell_pgid, sh->shell_pgid) < 0) {
            perror ("Couldn't put the shell in its own process group");
            exit (1);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp(sh->shell_terminal, sh->shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr(sh->shell_terminal, &sh->shell_tmodes);
    }
}

void sh_destroy(struct shell *sh) {
    free(sh->prompt);
    free(sh);
}

void parse_args(int argc, char **argv) {
    int opt;
    // Use getopt to parse command-line arguments
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                // Print the version and exit
                print_version();
                exit(EXIT_SUCCESS);
                break;
            default:
                break;
        }
    }
}

// print only completed jobs
void print_and_clean_jobs() {
    check_background_jobs();
    struct bg_job *curr = head;
    struct bg_job *prev = NULL;
    while (curr != NULL) {
        if (curr->is_running) {
            // traverse
            curr = curr->next;
            prev = curr;
        } else if (!curr->is_running) {
            printf("[%d] Done %s\n", curr->job_num, curr->command);

            if (prev == NULL) {
                head = curr->next;
            } else {
                prev->next = curr->next;
            }

            free(curr->command);
            struct bg_job *temp = curr;
            curr = curr->next;
            free(temp);
        }
    }
}

// checks if it should run as a background process
bool is_bg_process(char *line) {
    if (strlen(line) > 0 && line[strlen(line) - 1] == '&') {
        line[strlen(line) - 1] = '\0';
        return true;
    }
    return false;
}

void execCommand(struct shell *sh, char **cmd, bool run_in_bg) {
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Forking failed.\n");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        pid_t child = getpid();
        setpgid(child, child);
        if (!run_in_bg)
            tcsetpgrp(sh->shell_terminal,child);

        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        int exit_val = execvp(cmd[0], cmd);
        if (exit_val == -1) {
            printf("Unknown command: %s\n", cmd[0]);
        }

        exit(exit_val);
    } else {
        if (run_in_bg) {
            struct bg_job *job = (struct bg_job*) malloc(sizeof(struct bg_job));
            job->job_num = ++num_jobs;
            job->pid = pid;
            char *tmp2 = join_strings(cmd, 0);
            char *tmp = malloc((strlen(tmp2) + 2) * sizeof(char));
            tmp[0] = '\0';
            strcat(tmp, tmp2);
            strcat(tmp, "&");
            free(tmp2);
            job->command = tmp;
            job->next = head;
            head = job;
            printf("[%d] %d %s\n", job->job_num, pid, job->command);
        }
        else {
            waitpid(pid, NULL, 0);
            tcsetpgrp(sh->shell_terminal, sh->shell_pgid);
        }
    }
}

void handle_shell_line(struct shell *sh, char *line) {
    char *cmd = strdup(line);
    for (int i = 0; cmd[i]; i++) cmd[i] = tolower(cmd[i]);
    trim_white(cmd);
    bool run_in_bg = is_bg_process(cmd);
    char** parsed_cmd = cmd_parse(cmd);
    if (parsed_cmd != NULL && !do_builtin(sh, parsed_cmd)) {
        execCommand(sh, parsed_cmd, run_in_bg);
        cmd_free(parsed_cmd);
        print_and_clean_jobs();
    }
    free(cmd);
}


void shell_loop(struct shell *sh) {
    char *line;
    using_history();
    while ((line=readline(sh->prompt))){
        handle_shell_line(sh, line);
        add_history(line);
        free(line);
    }
}

/**

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

void freeStringArr(char** strArr, unsigned int length) {
    if (strArr == NULL) return;
    for (unsigned int i = 0; i < length; i++) {
        free(strArr[i]);
    }
    free(strArr);
}   

void execCommand(char *command, struct CommandArguments *argv) {
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Forking failed.\n");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        // Build final command;
        char **_argv = (char**) malloc((argv->numArgs + 2) * sizeof(char*));
        // first command
        _argv[0] = (char*) malloc((strlen(command) + 1) * sizeof(char));
        strcpy(_argv[0], command);
        // arguments
        for (unsigned int i = 1; i < argv->numArgs + 1; i++) {
            _argv[i] = (char*) malloc((strlen(argv->args[i-1]) + 1) * sizeof(char));
            strcpy(_argv[i], argv->args[i-1]);
        }
        _argv[argv->numArgs + 1] = NULL;
        //print_str_arr(_argv, argv->numArgs + 1);
        int exit_val = execvp(command, _argv);
        freeStringArr(_argv, 1+argv->numArgs);
        exit(exit_val);
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

void print_str_arr(char** strArr, unsigned int length) {
    for (unsigned int i = 0; i < length; i++) {
        printf("%s, ", strArr[i]);
    }
    printf("\n");
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

    if (strcmp(command, "exit") == 0) {
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
        execCommand(command, commandArguments);
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
}*/