#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <brainfuck/ir.h>
#include <brainfuck/amd64.h>

enum {
    R_NONE,
    R_R12,
    R_R13,
    R_R14,
    R_R15,
    R_MAX,
};

static int tmpr(Chunk *chunk, int tmp) {
    assert(chunk->tmps[tmp].reg);
    return chunk->tmps[tmp].reg;
}

static char *rstr(int reg) {
    switch (reg) {
    case R_R12: return "r12";
    case R_R13: return "r13";
    case R_R14: return "r14";
    case R_R15: return "r15";
    }
    return "???";
}

static char *rstrb(int reg) {
    switch (reg) {
    case R_R12: return "r12b";
    case R_R13: return "r13b";
    case R_R14: return "r14b";
    case R_R15: return "r15b";
    }
    return "???";
}

static void genblk(Chunk *chunk, Block *blk) {
    printf(".L%i:\n", blk->lbl);
    for (int ip = 0; ip < blk->inscnt; ip++) {
        Ins *i = &blk->ins[ip];
        printf("; "); printins(chunk, i);
        switch (i->op) {
        case OP_READ:
            printf("call getchar\n");
            printf("mov [%s], al\n", rstr(tmpr(chunk, chunk->dptmpid)));
            break;
        case OP_WRITE:
            printf("movzx rdi, byte [%s]\n", rstr(tmpr(chunk, chunk->dptmpid)));
            printf("call putchar\n");
            break;
        case OP_ADD: {
            assert(isreftmp(i->args[0]));
            assert(isrefint(chunk, i->args[1]));
            int cons = chunk->cons[i->args[1].val].as.int_;
            printf("add %s, %i\n", rstr(tmpr(chunk, i->args[0].val)), cons);
            break;
        }
        case OP_LOAD8:
            printf("movzx %s, byte [%s]\n",
                    rstr(tmpr(chunk, i->args[0].val)),
                    rstr(tmpr(chunk, i->args[1].val)));
            break;
        case OP_STORE8:
            printf("mov byte [%s], %s\n",
                    rstr(tmpr(chunk, i->args[0].val)),
                    rstrb(tmpr(chunk, i->args[1].val)));
            break;
        case OP_NOT: {
            printf("cmp %s, 0\n", rstr(tmpr(chunk, i->args[0].val)));
            printf("setz %s\n", rstrb(tmpr(chunk, i->args[0].val)));
            break;
        }
        case OP_CJMP:
            printf("cmp %s, 0\n",  rstr(tmpr(chunk, i->args[0].val)));
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
    printf("; save registers\n");
    int slots = (R_MAX * 8 + 15) / 16;
    printf("sub rsp, %i\n", slots * 16);
    for (int r = R_R12; r < R_MAX; r++)
        printf("mov [rbp - %i], %s\n", (r - R_R12 + 1) * 8, rstr(r));
    printf("; reserve and zero bf data area\n");
    printf("mov rcx, 16 * 1875\n");
    printf(".zerodata:\n");
    printf("dec rsp\n");
    printf("mov byte [rsp], 0\n");
    printf("loop .zerodata\n");
    printf("mov %s, rsp\n", rstr(tmpr(chunk, chunk->dptmpid)));
    for (int i = 0; i < chunk->blkcnt; i++)
        genblk(chunk, &chunk->blocks[i]);
    printf("; restore registers\n");
    for (int r = R_R12; r < R_MAX; r++)
        printf("mov %s, [rbp - %i]\n", rstr(r), (r - R_R12 + 1) * 8);
}

static void color(Chunk *chunk) {
    int reg = R_R12;
    for (int i = 0; i < chunk->tmpcnt; i++) {
        if (reg >= R_MAX) {
            printf("*** out of registers\n");
            exit(1);
        }
        chunk->tmps[i].reg = reg;
        reg++;
    }
}

void amd64gen(Chunk *chunk) {
    color(chunk);
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
