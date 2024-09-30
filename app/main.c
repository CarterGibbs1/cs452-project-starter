#include "../src/lab.h"

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