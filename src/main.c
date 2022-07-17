#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage() {
    printf("Usage: brainfuck file\n");
    exit(1);
}

typedef struct {
    char *mem;
    int size;
} Chunk;

static void readprogram(Chunk *dst, char *path) {
    FILE *fp = fopen(path, "r");
    fseek(fp, 0, SEEK_END);
    dst->size = ftell(fp);
    dst->mem = malloc(dst->size + 1);
    rewind(fp);
    int r = fread(dst->mem, dst->size, 1, fp);
    if (r != 1) printf("*** couldn't read file [%s]\n", path);
    dst->mem[dst->size] = 0;
    fclose(fp);
}

void bfwrite(int c) {
    putchar(c);
}

int bfread() {
    return getchar();
}

int interpret(char *buf, int ip) {
    char data[30000] = {0};
    int dp = 0;
    for (; buf[ip]; ip++) {
        switch (buf[ip]) {
        case '>': dp++; break;
        case '<': dp--; break;
        case '+': data[dp]++; break;
        case '-': data[dp]--; break;
        case '.': bfwrite(data[dp]); break;
        case ',': data[dp] = bfread(); break;
        case '[':
            if (data[dp]) break;
            ip++;
            for (int depth = 0;; ip++) {
                if (buf[ip] == ']') {
                    if (depth == 0) break;
                    depth--;
                }
                else if (buf[ip] == '[') {
                    depth++;
                }
            }
            break;
        case ']':
            if (!data[dp]) break;
            ip--;
            for (int depth = 0;; ip--) {
                if (buf[ip] == '[') {
                    if (depth == 0) break;
                    depth--;
                }
                else if (buf[ip] == ']') {
                    depth++;
                }
            }
            break;
        default: /* do nothing */ break;
        }
    }
    return 0;
}

int interpret2(char *buf, int ip);

int main(int argc, char **argv) {
    if (argc < 2) usage();
    Chunk chunk;
    readprogram(&chunk, argv[1]);
    interpret2(chunk.mem, 0);
    return 0;
}
