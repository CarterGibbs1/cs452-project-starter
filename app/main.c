#include "../src/lab.h"

void shell_loop(struct shell*);

int main(int argc, char *argv[]) {

    parse_args(argc, argv);

    struct shell *sh = (struct shell *) malloc(sizeof(struct shell));
    sh_init(sh);
    shell_loop(sh);
    sh_destroy(sh);
}