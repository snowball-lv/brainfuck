#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void reftostr(Chunk *chunk, char *buf, Ref r) {
    sprintf(buf, "[???]");
    if (isreftmp(r)) sprintf(buf, "%s", rstr(tmpr(chunk, r.val)));
    if (isrefint(chunk, r)) sprintf(buf, "%i", chunk->cons[r.val].as.int_);
    if (isrefstr(chunk, r)) sprintf(buf, "%s", chunk->cons[r.val].as.str);
}

static void genblk(Chunk *chunk, Block *blk) {
    char bufs[3][16];
    printf(".L%i:\n", blk->lbl);
    for (int ip = 0; ip < blk->inscnt; ip++) {
        Ins *i = &blk->ins[ip];
        printf("; "); printins(chunk, i);
        switch (i->op) {
        case OP_CALL:
            reftostr(chunk, bufs[0], i->args[0]);
            printf("movzx rdi, byte [%s]\n", rstr(tmpr(chunk, chunk->dptmpid)));
            printf("call %s\n", bufs[0]);
            printf("mov [%s], al\n", rstr(tmpr(chunk, chunk->dptmpid)));
            break;
        case OP_ADD: {
            assert(isreftmp(i->dst));
            reftostr(chunk, bufs[0], i->dst);
            reftostr(chunk, bufs[1], i->args[0]);
            reftostr(chunk, bufs[2], i->args[1]);
            printf("mov %s, %s\n", bufs[0], bufs[1]);
            printf("add %s, %s\n", bufs[0], bufs[2]);
            break;
        }
        case OP_LOAD8:
            printf("movzx %s, byte [%s]\n",
                    rstr(tmpr(chunk, i->dst.val)),
                    rstr(tmpr(chunk, i->args[0].val)));
            break;
        case OP_STORE8:
            printf("mov byte [%s], %s\n",
                    rstr(tmpr(chunk, i->args[0].val)),
                    rstrb(tmpr(chunk, i->args[1].val)));
            break;
        case OP_NOT: {
            printf("cmp %s, 0\n", rstr(tmpr(chunk, i->args[0].val)));
            printf("setz %s\n", rstrb(tmpr(chunk, i->dst.val)));
            break;
        }
        case OP_CJMP:
            printf("cmp %s, 0\n", rstr(tmpr(chunk, i->args[0].val)));
            printf("jnz .L%i\n",  i->args[1].val);
            break;
        case OP_JMP:
            printf("jmp .L%i\n",  i->args[0].val);
            break;
        case OP_ALLOC: {
            assert(isreftmp(i->dst));
            assert(isrefint(chunk, i->args[0]));
            int size = chunk->cons[i->args[0].val].as.int_;
            int slots = (size + 15) / 16;
            printf("sub rsp, %i * %i\n", slots, 16);
            printf("mov %s, rsp\n", rstr(tmpr(chunk, i->dst.val)));
            break;
        }
        case OP_MOV: {
            assert(isreftmp(i->dst));
            reftostr(chunk, bufs[0], i->args[0]);
            printf("mov %s, %s\n", rstr(tmpr(chunk, i->dst.val)), bufs[0]);
            break;
        }
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
    for (int i = 0; i < chunk->blkcnt; i++)
        genblk(chunk, &chunk->blocks[i]);
    printf("; restore registers\n");
    for (int r = R_R12; r < R_MAX; r++)
        printf("mov %s, [rbp - %i]\n", rstr(r), (r - R_R12 + 1) * 8);
}

static int allregs() {
    int all = 0;
    for (int r = R_R12; r < R_MAX; r++)
        all |= 1 << r;
    return all;
}

static void color(Chunk *chunk) {
    char *set = malloc(chunk->tmpcnt);
    for (int tmp = 0; tmp < chunk->tmpcnt; tmp++) {
        int freeregs = allregs();
        memset(set, 0, chunk->tmpcnt);
        interferes(chunk, set, tmp);
        printf("; $%i", tmp);
        for (int i = 0; i < chunk->tmpcnt; i++) {
            if (!set[i]) continue;
            printf(" -- $%i", i);
            if (!chunk->tmps[i].reg) continue;
            printf("(%s)", rstr(chunk->tmps[i].reg));
            freeregs &= ~(1 << chunk->tmps[i].reg);
        }
        printf("\n");
        if (!freeregs) {
            printf("*** spill $%i\n", tmp);
            exit(1);
        }
        int reg = __builtin_ctz(freeregs);
        chunk->tmps[tmp].reg = reg;
        printf("; $%i = %s\n", tmp, rstr(reg));
    }
    free(set);
}

void amd64gen(Chunk *chunk) {
    liveness(chunk);
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
    printf("%%if 0\n");
    printchunk(chunk);
    printf("%%endif\n\n");
    genchunk(chunk);
    printf("\n");
    printf("mov rsp, rbp\n");
    printf("pop rbp\n");
    printf("mov rax, 0\n");
    printf("ret\n");
}
