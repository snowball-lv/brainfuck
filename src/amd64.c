#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/ir.h>
#include <brainfuck/amd64.h>

enum {
    R_NONE,
    R_RAX,
    R_RCX,
    R_RDX,
    R_RBX,
    R_RSP,
    R_RBP,
    R_RSI,
    R_RDI,
    R_R8,
    R_R9,
    R_R10,
    R_R11,
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
    case R_RAX: return "rax";
    case R_RCX: return "rcx";
    case R_RDX: return "rdx";
    case R_RBX: return "rbx";
    case R_RSP: return "rsp";
    case R_RBP: return "rbp";
    case R_RSI: return "rsi";
    case R_RDI: return "rdi";
    case R_R8: return "r8";
    case R_R9: return "r9";
    case R_R10: return "r10";
    case R_R11: return "r11";
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

static char *tmptostr(char *buf, Chunk *chunk, int tmp) {
    char *ptr = buf + sprintf(buf, "$%i", tmp);
    if (chunk->tmps[tmp].reg)
        sprintf(ptr, "(%s)", rstr(chunk->tmps[tmp].reg));
    return buf;
}

static void printusedef(Chunk *chunk, Ins *i) {
    char buf[16];
    printf(";     use:");
    if (isreftmp(i->args[0])) printf(" %s", tmptostr(buf, chunk, i->args[0].val));
    if (isreftmp(i->args[1])) printf(" %s", tmptostr(buf, chunk, i->args[1].val));
    if (i->op == OP_CALL) {
        for (int i = 0; i < chunk->target->nparams; i++) {
            int r = chunk->target->params[i];
            int tmp = chunk->target->rtmps[r];
            printf(" %s", tmptostr(buf, chunk, tmp));
        }
    }
    printf("\n");
    printf(";     def:");
    if (isreftmp(i->dst)) printf(" %s", tmptostr(buf, chunk, i->dst.val));
    if (i->op == OP_CALL) {
        for (int i = 0; i < chunk->target->nscratch; i++) {
            int r = chunk->target->scratch[i];
            int tmp = chunk->target->rtmps[r];
            printf(" %s", tmptostr(buf, chunk, tmp));
        }
    }
    else if (i->op == OP_SCRATCH) {
        for (int i = 0; i < chunk->target->nscratch; i++) {
            int r = chunk->target->scratch[i];
            int tmp = chunk->target->rtmps[r];
            printf(" %s", tmptostr(buf, chunk, tmp));
        }
    }
    printf("\n");
}

static void genblk(Chunk *chunk, Block *blk) {
    char bufs[3][16];
    printf(".L%i:\n", blk->lbl);
    for (int ip = 0; ip < blk->inscnt; ip++) {
        Ins *i = &blk->ins[ip];
        printf("; "); printins(chunk, i);
        printusedef(chunk, i);
        switch (i->op) {
        case OP_SCRATCH:
            printf("; scratch\n");
            break;
        case OP_ARG: {
            int argn = i->args[0].val;
            int r = chunk->target->params[argn];
            reftostr(chunk, bufs[0], i->args[1]);
            printf("mov %s, %s\n", rstr(r), bufs[0]);
            break;
        }
        case OP_CALL:
            reftostr(chunk, bufs[0], i->dst);
            reftostr(chunk, bufs[1], i->args[0]);
            printf("call %s\n", bufs[1]);
            printf("mov %s, rax\n", bufs[0]);
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
        if (!chunk->tmps[tmp].reg) {
            if (!freeregs) {
                printf("*** spill $%i\n", tmp);
                exit(1);
            }
            chunk->tmps[tmp].reg = __builtin_ctz(freeregs);
        }
        printf("; $%i = %s\n", tmp, rstr(chunk->tmps[tmp].reg));
    }
    free(set);
}

static Target T = {
    .ret = (int[]){R_RAX, R_RDX},
    .nret = 2,
    .params = (int[]){R_RDI, R_RSI, R_RDX, R_RCX, R_R8, R_R9},
    .nparams = 6,
    .scratch = (int[]){R_RAX, R_RDI, R_RSI, R_RDX, R_RCX, R_R8, R_R9, R_R10, R_R11},
    .nscratch = 9,
};

static void filter(Chunk *chunk) {
    for (int bi = 0; bi < chunk->blkcnt; bi++) {
        Block *blk = &chunk->blocks[bi];
        for (int ip = 0; ip < blk->inscnt; ip++) {
            Ins *i = &blk->ins[ip];
            if (i->op == OP_ARG) {
                int argn = i->args[0].val;
                assert(argn < chunk->target->nparams);
                int reg = chunk->target->params[argn];
                int rtmp = chunk->target->rtmps[reg];
                assert(isreftmp(i->args[1]));
                *i = imov(reftmp(rtmp), i->args[1]);
            }
        }
    }
}

void amd64gen(Chunk *chunk) {
    // create pre-colored tmp for each register
    T.rtmps = malloc(R_MAX * sizeof(int));
    T.nrtmps = R_MAX;
    for (int r = 0; r < R_MAX; r++) {
        int tmp = newtmp(chunk);
        chunk->tmps[tmp].reg = r;
        T.rtmps[r] = tmp;
    }
    chunk->target = &T;
    filter(chunk);
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
