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

static void sanitize(Chunk *chunk) {
    int k = 0;
    for (int i = 0; i < chunk->size; i++) {
        switch (chunk->mem[i]) {
        case '>': case '<':
        case '+': case '-':
        case '.': case ',':
        case '[': case ']':
            chunk->mem[k++] = chunk->mem[i];
            break;
        }
    }
    chunk->size = k;
    chunk->mem[chunk->size] = 0;
}

static int newlabel() {
    static int COUNTER = 0;
    return COUNTER++;
}

static int genblock(Chunk *chunk, int blocknum, int pos) {
    int newblock;
    for (; pos < chunk->size; pos++) {
        switch (chunk->mem[pos]) {
        case '>':
            // printf("; >\n");
            printf("inc r12\n");
            break;
        case '<':
            // printf("; <\n");
            printf("dec r12\n");
            break;
        case '+':
            // printf("; +\n");
            printf("inc byte [r12]\n");
            break;
        case '-':
            // printf("; -\n");
            printf("dec byte [r12]\n");
            break;
        case '.':
            // printf("; .\n");
            printf("movzx rdi, byte [r12]\n");
            printf("call putchar\n");
            break;
        case ',':
            // printf("; ,\n");
            printf("call getchar\n");
            printf("mov [r12], al\n");
            break;
        case '[':
            // printf("; [\n");
            newblock = newlabel();
            printf("cmp byte [r12], 0\n");
            printf("jz .L%iend\n", newblock);
            printf(".L%istart:\n", newblock);
            pos = genblock(chunk, newblock, pos + 1);
            break;
        case ']':
            // printf("; ]\n");
            printf("cmp byte [r12], 0\n");
            printf("jnz .L%istart\n", blocknum);
            printf(".L%iend:\n", blocknum);
            return pos;
        }
    }
    return chunk->size;
}

static void gen(Chunk *chunk) {
    printf("bits 64\n");
    printf("extern putchar\n");
    printf("extern getchar\n");
    printf("section .text\n");
    printf("global main\n");
    printf("main:\n");
    printf("push rbp\n");
    printf("mov rbp, rsp\n");
    printf("sub rsp, 16\n");
    printf("mov [rbp - 8], r12\n");

    // reserve and zero data area
    printf("mov rcx, 16 * 1875\n");
    printf(".zerodata:\n");
    printf("dec rsp\n");
    printf("mov byte [rsp], 0\n");
    printf("loop .zerodata\n");
    printf("mov r12, rsp\n");
    
    genblock(chunk, newlabel(), 0);

    printf("mov r12, [rbp - 8]\n");
    printf("mov rsp, rbp\n");
    printf("pop rbp\n");
    printf("mov rax, 0\n");
    printf("ret\n");
}

int main(int argc, char **argv) {
    if (argc < 2) usage();
    Chunk chunk;
    readprogram(&chunk, argv[1]);
    sanitize(&chunk);
    gen(&chunk);
    // printf("%s", chunk.mem);
    // interpret2(chunk.mem, 0);
    return 0;
}
