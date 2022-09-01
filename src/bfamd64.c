#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <brainfuck/brainfuck.h>
#include <brainfuck/ir.h>
#include <brainfuck/amd64.h>

static int newlabel() {
    static int COUNTER = 0;
    return COUNTER++;
}

static int genblock(Task *t, int blocknum, int pos) {
    int newblock;
    for (; t->src[pos]; pos++) {
        switch (t->src[pos]) {
        case '>':
            // fprintf(t->out, "; >\n");
            fprintf(t->out, "inc r12\n");
            break;
        case '<':
            // fprintf(t->out, "; <\n");
            fprintf(t->out, "dec r12\n");
            break;
        case '+':
            // fprintf(t->out, "; +\n");
            fprintf(t->out, "inc byte [r12]\n");
            break;
        case '-':
            // fprintf(t->out, "; -\n");
            fprintf(t->out, "dec byte [r12]\n");
            break;
        case '.':
            // fprintf(t->out, "; .\n");
            fprintf(t->out, "movzx rdi, byte [r12]\n");
            fprintf(t->out, "call putchar\n");
            break;
        case ',':
            // fprintf(t->out, "; ,\n");
            fprintf(t->out, "call getchar\n");
            fprintf(t->out, "mov [r12], al\n");
            break;
        case '[':
            // fprintf(t->out, "; [\n");
            newblock = newlabel();
            fprintf(t->out, "cmp byte [r12], 0\n");
            fprintf(t->out, "jz .L%iend\n", newblock);
            fprintf(t->out, ".L%istart:\n", newblock);
            pos = genblock(t, newblock, pos + 1);
            break;
        case ']':
            // fprintf(t->out, "; ]\n");
            fprintf(t->out, "cmp byte [r12], 0\n");
            fprintf(t->out, "jnz .L%istart\n", blocknum);
            fprintf(t->out, ".L%iend:\n", blocknum);
            return pos;
        }
    }
    return pos;
}

void gennasm(Task *t) {
    fprintf(t->out, "bits 64\n");
    fprintf(t->out, "extern putchar\n");
    fprintf(t->out, "extern getchar\n");
    fprintf(t->out, "section .text\n");
    fprintf(t->out, "global main\n");
    fprintf(t->out, "main:\n");
    fprintf(t->out, "push rbp\n");
    fprintf(t->out, "mov rbp, rsp\n");
    fprintf(t->out, "sub rsp, 16\n");
    fprintf(t->out, "mov [rbp - 8], r12\n");

    // reserve and zero data area
    fprintf(t->out, "mov rcx, 16 * 1875\n");
    fprintf(t->out, ".zerodata:\n");
    fprintf(t->out, "dec rsp\n");
    fprintf(t->out, "mov byte [rsp], 0\n");
    fprintf(t->out, "loop .zerodata\n");
    fprintf(t->out, "mov r12, rsp\n");
    
    genblock(t, newlabel(), 0);

    fprintf(t->out, "mov r12, [rbp - 8]\n");
    fprintf(t->out, "mov rsp, rbp\n");
    fprintf(t->out, "pop rbp\n");
    fprintf(t->out, "mov rax, 0\n");
    fprintf(t->out, "ret\n");
}

void genirnasm(Task *t) {
    t->fn = malloc(sizeof(Func));
    memset(t->fn, 0, sizeof(Func));
    bftoir(t);
    amd64gen(t);
}
