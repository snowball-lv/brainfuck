#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/brainfuck.h>
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

static void printusedef(Task *t, Chunk *chunk, Ins *i) {
    char buf[16];
    fprintf(t->out, ";     use:");
    if (isreftmp(i->args[0])) fprintf(t->out, " %s", tmptostr(buf, chunk, i->args[0].val));
    if (isreftmp(i->args[1])) fprintf(t->out, " %s", tmptostr(buf, chunk, i->args[1].val));
    if (i->op == OP_CALL) {
        for (int i = 0; i < chunk->target->nparams; i++) {
            int r = chunk->target->params[i];
            int tmp = chunk->target->rtmps[r];
            fprintf(t->out, " %s", tmptostr(buf, chunk, tmp));
        }
    }
    fprintf(t->out, "\n");
    fprintf(t->out, ";     def:");
    if (isreftmp(i->dst)) fprintf(t->out, " %s", tmptostr(buf, chunk, i->dst.val));
    if (i->op == OP_CALL) {
        for (int i = 0; i < chunk->target->nscratch; i++) {
            int r = chunk->target->scratch[i];
            int tmp = chunk->target->rtmps[r];
            fprintf(t->out, " %s", tmptostr(buf, chunk, tmp));
        }
    }
    else if (i->op == OP_SCRATCH) {
        for (int i = 0; i < chunk->target->nscratch; i++) {
            int r = chunk->target->scratch[i];
            int tmp = chunk->target->rtmps[r];
            fprintf(t->out, " %s", tmptostr(buf, chunk, tmp));
        }
    }
    fprintf(t->out, "\n");
}

static void genblk(Task *t, Chunk *chunk, Block *blk) {
    char bufs[3][16];
    fprintf(t->out, ".L%i:\n", blk->lbl);
    for (int ip = 0; ip < blk->inscnt; ip++) {
        Ins *i = &blk->ins[ip];
        fprintf(t->out, "; "); printins(t->out, chunk, i);
        printusedef(t, chunk, i);
        switch (i->op) {
        case OP_SCRATCH:
            fprintf(t->out, "; scratch\n");
            break;
        case OP_ARG: {
            int argn = i->args[0].val;
            int r = chunk->target->params[argn];
            reftostr(chunk, bufs[0], i->args[1]);
            fprintf(t->out, "mov %s, %s\n", rstr(r), bufs[0]);
            break;
        }
        case OP_CALL:
            reftostr(chunk, bufs[0], i->dst);
            reftostr(chunk, bufs[1], i->args[0]);
            fprintf(t->out, "call %s\n", bufs[1]);
            fprintf(t->out, "mov %s, rax\n", bufs[0]);
            break;
        case OP_ADD: {
            assert(isreftmp(i->dst));
            reftostr(chunk, bufs[0], i->dst);
            reftostr(chunk, bufs[1], i->args[0]);
            reftostr(chunk, bufs[2], i->args[1]);
            fprintf(t->out, "mov %s, %s\n", bufs[0], bufs[1]);
            fprintf(t->out, "add %s, %s\n", bufs[0], bufs[2]);
            break;
        }
        case OP_LOAD8:
            fprintf(t->out, "movzx %s, byte [%s]\n",
                    rstr(tmpr(chunk, i->dst.val)),
                    rstr(tmpr(chunk, i->args[0].val)));
            break;
        case OP_STORE8:
            fprintf(t->out, "mov byte [%s], %s\n",
                    rstr(tmpr(chunk, i->args[0].val)),
                    rstrb(tmpr(chunk, i->args[1].val)));
            break;
        case OP_NOT: {
            fprintf(t->out, "cmp %s, 0\n", rstr(tmpr(chunk, i->args[0].val)));
            fprintf(t->out, "setz %s\n", rstrb(tmpr(chunk, i->dst.val)));
            break;
        }
        case OP_CJMP:
            fprintf(t->out, "cmp %s, 0\n", rstr(tmpr(chunk, i->args[0].val)));
            fprintf(t->out, "jnz .L%i\n",  i->args[1].val);
            break;
        case OP_JMP:
            fprintf(t->out, "jmp .L%i\n",  i->args[0].val);
            break;
        case OP_ALLOC: {
            assert(isreftmp(i->dst));
            assert(isrefint(chunk, i->args[0]));
            int size = chunk->cons[i->args[0].val].as.int_;
            int slots = (size + 15) / 16;
            fprintf(t->out, "sub rsp, %i * %i\n", slots, 16);
            fprintf(t->out, "mov %s, rsp\n", rstr(tmpr(chunk, i->dst.val)));
            break;
        }
        case OP_MOV: {
            assert(isreftmp(i->dst));
            reftostr(chunk, bufs[0], i->args[0]);
            fprintf(t->out, "mov %s, %s\n", rstr(tmpr(chunk, i->dst.val)), bufs[0]);
            break;
        }
        default:
            fprintf(t->out, "*** can't generate op [%i]\n", i->op);
            exit(1);
        }
    }
}

static void genchunk(Task *t, Chunk *chunk) {
    fprintf(t->out, "; save registers\n");
    int slots = (R_MAX * 8 + 15) / 16;
    fprintf(t->out, "sub rsp, %i\n", slots * 16);
    for (int r = R_R12; r < R_MAX; r++)
        fprintf(t->out, "mov [rbp - %i], %s\n", (r - R_R12 + 1) * 8, rstr(r));
    for (int i = 0; i < chunk->blkcnt; i++)
        genblk(t, chunk, &chunk->blocks[i]);
    fprintf(t->out, "; restore registers\n");
    for (int r = R_R12; r < R_MAX; r++)
        fprintf(t->out, "mov %s, [rbp - %i]\n", rstr(r), (r - R_R12 + 1) * 8);
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

void amd64gen(Task *t) {
    // create pre-colored tmp for each regist
    Chunk *chunk = t->chunk;
    T.rtmps = malloc(R_MAX * sizeof(int));
    T.nrtmps = R_MAX;
    for (int r = 0; r < R_MAX; r++) {
        int tmp = newtmp(chunk);
        chunk->tmps[tmp].reg = r;
        chunk->tmps[tmp].precolored = 1;
        T.rtmps[r] = tmp;
    }
    // set usable regs for temps
    T.freeregs = 0;
    for (int r = R_R12; r < R_MAX; r++)
        T.freeregs |= 1 << r;
    T.rstr = rstr;
    chunk->target = &T;
    filter(chunk);
    liveness(chunk);
    color(t, chunk);
    fprintf(t->out, "bits 64\n");
    fprintf(t->out, "extern putchar\n");
    fprintf(t->out, "extern getchar\n");
    fprintf(t->out, "section .text\n");
    fprintf(t->out, "global main\n");
    fprintf(t->out, "main:\n");
    fprintf(t->out, "push rbp\n");
    fprintf(t->out, "mov rbp, rsp\n");
    fprintf(t->out, "\n");
    fprintf(t->out, "%%if 0\n");
    printchunk(t->out, chunk);
    fprintf(t->out, "%%endif\n\n");
    genchunk(t, chunk);
    fprintf(t->out, "\n");
    fprintf(t->out, "mov rsp, rbp\n");
    fprintf(t->out, "pop rbp\n");
    fprintf(t->out, "mov rax, 0\n");
    fprintf(t->out, "ret\n");
}
