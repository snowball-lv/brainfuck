#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <brainfuck/ir.h>
#include <brainfuck/amd64.h>

static void load(char *reg, int tmp) {
    printf("mov %s, [rbp - %i]\n", reg, (tmp + 1) * 8);
}

static void store(int tmp, char *reg) {
    printf("mov [rbp - %i], %s\n",  (tmp + 1) * 8, reg);
}

static void genblk(Chunk *chunk, Block *blk) {
    printf(".L%i:\n", blk->lbl);
    for (int ip = 0; ip < blk->inscnt; ip++) {
        Ins *i = &blk->ins[ip];
        switch (i->op) {
        case OP_READ:
            printf("call getchar\n");
            printf("; mov $%i, al\n", chunk->dptmpid);
            load("rcx", chunk->dptmpid);
            printf("mov [rcx], al\n");
            break;
        case OP_WRITE:
            printf("; movzx rdi, byte [$%i]\n", chunk->dptmpid);
            load("rax", chunk->dptmpid);
            printf("movzx rdi, byte [rax]\n");
            printf("call putchar\n");
            break;
        case OP_ADD: {
            assert(isreftmp(i->args[0]));
            assert(isrefint(chunk, i->args[1]));
            int cons = chunk->cons[i->args[1].val].as.int_;
            printf("; add $%i, %i\n", i->args[0].val, cons);
            load("rax", i->args[0].val);
            printf("add rax, %i\n", cons);
            store(i->args[0].val, "rax");
            break;
        }
        case OP_LOAD8:
            printf("; mov $%i, byte [$%i]\n", i->args[0].val, i->args[1].val);
            load("rax", i->args[1].val);
            printf("movzx rcx, byte [rax]\n");
            store(i->args[0].val, "rcx");
            break;
        case OP_STORE8:
            printf("; mov byte [$%i], $%i\n", i->args[1].val, i->args[0].val);
            load("rax", i->args[1].val);
            load("rcx", i->args[0].val);
            printf("mov byte [rax], cl\n");
            break;
        case OP_NOT:
            printf("; not $%i\n",  i->args[0].val);
            printf("mov rax, 0\n");
            load("rcx", i->args[0].val);
            printf("cmp rcx, 0\n");
            printf("setz al\n");
            store(i->args[0].val, "rax");
            break;
        case OP_CJMP:
            printf("; cmp $%i, 0\n",  i->args[0].val);
            load("rax", i->args[0].val);
            printf("cmp rax, 0\n");
            printf("jnz .L%i\n",  i->args[1].val);
            break;
        case OP_JMP:
            printf("jmp .L%i\n",  i->args[0].val);
            break;
        default:
            printf("*** can't generate op [%i]\n", i->op);
            exit(1);
        }
    }
}

static void genchunk(Chunk *chunk) {
    printf("; reserve space for temporaries\n");
    int slots = (chunk->tmpcnt * 8 + 15) / 16;
    printf("sub rsp, 16 * %i\n", slots);
    printf("; reserve and zero bf data area\n");
    printf("mov rcx, 16 * 1875\n");
    printf(".zerodata:\n");
    printf("dec rsp\n");
    printf("mov byte [rsp], 0\n");
    printf("loop .zerodata\n");
    printf("; mov $%i, rsp\n", chunk->dptmpid);
    store(chunk->dptmpid, "rsp");
    for (int i = 0; i < chunk->blkcnt; i++)
        genblk(chunk, &chunk->blocks[i]);
}

void amd64gen(Chunk *chunk) {
    printf("bits 64\n");
    printf("extern putchar\n");
    printf("extern getchar\n");
    printf("section .text\n");
    printf("global main\n");
    printf("main:\n");
    printf("push rbp\n");
    printf("mov rbp, rsp\n");
    printf("\n");
    // printchunk(chunk);
    // printf("\n");
    genchunk(chunk);
    printf("\n");
    printf("mov rsp, rbp\n");
    printf("pop rbp\n");
    printf("mov rax, 0\n");
    printf("ret\n");
}
