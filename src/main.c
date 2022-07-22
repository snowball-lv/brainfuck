#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <brainfuck/ir.h>
#include <brainfuck/brainfuck.h>

static char *OPTS[] = {
    "-c:default, compiles to NASM and prints to stdout",
    "-cir:compiles to NASM using intermediate representation",
    "-i:interpret using ASM interpreter",
    "-ic:interpret using C interpreter",
    "-iir:convert to intermediate representation and interpret that",
    0,
};

static void usage() {
    printf("Usage: brainfuck [options] file\n");
    printf("Options:\n");
    for (char **opt = OPTS; *opt; opt++) {
        char *sep = strchr(*opt, ':');
        printf("    %-7.*s %s\n", (int)(sep - *opt), *opt, sep + 1);
    }
    exit(1);
}

static char *readsrc(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("*** couldn't open file [%s]\n", path);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    char *src = malloc(size + 1);
    rewind(fp);
    int r = fread(src, size, 1, fp);
    if (r != 1) {
        printf("*** couldn't read file [%s]\n", path);
        exit(1);
    }
    src[size] = 0;
    fclose(fp);
    return src;
}

void bfwrite(int c) {
    putchar(c);
}

int bfread() {
    return getchar();
}

int main(int argc, char **argv) {
    int run = 0;
    int runc = 0;
    int runir = 0;
    int compile = 1;
    int compileir = 0;
    char *file = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) run = 1;
        else if (strcmp(argv[i], "-ic") == 0) runc = 1;
        else if (strcmp(argv[i], "-c") == 0) compile = 1;
        else if (strcmp(argv[i], "-cir") == 0) compileir = 1;
        else if (strcmp(argv[i], "-iir") == 0) runir = 1;
        else if (!file) file = argv[i];
        else printf("*** unknown option [%s]\n", argv[i]);
    }
    if (!file) {
        printf("*** source file required\n");
        usage();
    }
    char *src = readsrc(file);
    if (run) interpretasm(src);
    else if (runc) interpretc(src);
    else if (runir) interpretir(src);
    else if (compileir) genirnasm(src);
    else if (compile) gennasm(src);
    free(src);
    return 0;
}
