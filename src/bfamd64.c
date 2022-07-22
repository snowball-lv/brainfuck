#include <stdio.h>
#include <brainfuck/ir.h>
#include <brainfuck/amd64.h>
#include <brainfuck/brainfuck.h>

static int newlabel() {
    static int COUNTER = 0;
    return COUNTER++;
}

static int genblock(char *src, int blocknum, int pos) {
    int newblock;
    for (; src[pos]; pos++) {
        switch (src[pos]) {
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
            pos = genblock(src, newblock, pos + 1);
            break;
        case ']':
            // printf("; ]\n");
            printf("cmp byte [r12], 0\n");
            printf("jnz .L%istart\n", blocknum);
            printf(".L%iend:\n", blocknum);
            return pos;
        }
    }
    return pos;
}

void gennasm(char *src) {
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
    
    genblock(src, newlabel(), 0);

    printf("mov r12, [rbp - 8]\n");
    printf("mov rsp, rbp\n");
    printf("pop rbp\n");
    printf("mov rax, 0\n");
    printf("ret\n");
}

void genirnasm(char *src) {
    Chunk chunk = {0};
    bftoir(&chunk, src);
    amd64gen(&chunk);
}
