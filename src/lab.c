#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lab.h"

void print_version() {
    printf("Version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
}

int main(int argc, char *argv[]) {
    int opt;
    
    // Use getopt to parse command-line arguments
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                // Print the version and exit
                print_version();
                break;
            default:
                fprintf(stderr, "Usage: %s [-v]\n", argv[0]);
                return 1;
                break;
        }
    }
    return 0;
}