#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/common.h>
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

static int tmpr(Func *fn, int tmp) {
    assert(fn->tmps[tmp].reg);
    return fn->tmps[tmp].reg;
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

static void reftostr(Func *fn, char *buf, Ref r) {
    sprintf(buf, "[???]");
    if (isreftmp(r)) sprintf(buf, "%s", rstr(tmpr(fn, r.val)));
    if (isrefint(fn, r)) sprintf(buf, "%i", fn->cons[r.val].as.int_);
    if (isrefstr(fn, r)) sprintf(buf, "%s", fn->cons[r.val].as.str);
}

static char *tmptostr(char *buf, Func *fn, int tmp) {
    char *ptr = buf + sprintf(buf, "$%i", tmp);
    if (fn->tmps[tmp].reg)
        sprintf(ptr, "(%s)", rstr(fn->tmps[tmp].reg));
    return buf;
}

static void printusedef(Task *t, Func *fn, Ins *i) {
    char buf[16];
    fprintf(t->out, ";     use:");
    if (isreftmp(i->args[0])) fprintf(t->out, " %s", tmptostr(buf, fn, i->args[0].val));
    if (isreftmp(i->args[1])) fprintf(t->out, " %s", tmptostr(buf, fn, i->args[1].val));
    if (i->op == OP_CALL) {
        for (int i = 0; i < fn->target->nparams; i++) {
            int r = fn->target->params[i];
            int tmp = fn->target->rtmps[r];
            fprintf(t->out, " %s", tmptostr(buf, fn, tmp));
        }
    }
    fprintf(t->out, "\n");
    fprintf(t->out, ";     def:");
    if (isreftmp(i->dst)) fprintf(t->out, " %s", tmptostr(buf, fn, i->dst.val));
    if (i->op == OP_CALL) {
        for (int i = 0; i < fn->target->nscratch; i++) {
            int r = fn->target->scratch[i];
            int tmp = fn->target->rtmps[r];
            fprintf(t->out, " %s", tmptostr(buf, fn, tmp));
        }
    }
    else if (i->op == OP_SCRATCH) {
        for (int i = 0; i < fn->target->nscratch; i++) {
            int r = fn->target->scratch[i];
            int tmp = fn->target->rtmps[r];
            fprintf(t->out, " %s", tmptostr(buf, fn, tmp));
        }
    }
    fprintf(t->out, "\n");
}

static void genblk(Task *t, Func *fn, Block *blk) {
    char bufs[3][16];
    fprintf(t->out, ".L%i:\n", blk->lbl);
    for (int ip = 0; ip < blk->inscnt; ip++) {
        Ins *i = &blk->ins[ip];
        fprintf(t->out, "; "); printins(t->out, fn, i);
        printusedef(t, fn, i);
        switch (i->op) {
        case OP_SCRATCH:
            fprintf(t->out, "; scratch\n");
            break;
        case OP_ARG: {
            int argn = i->args[0].val;
            int r = fn->target->params[argn];
            reftostr(fn, bufs[0], i->args[1]);
            fprintf(t->out, "mov %s, %s\n", rstr(r), bufs[0]);
            break;
        }
        case OP_CALL:
            reftostr(fn, bufs[0], i->dst);
            reftostr(fn, bufs[1], i->args[0]);
            fprintf(t->out, "call %s\n", bufs[1]);
            fprintf(t->out, "mov %s, rax\n", bufs[0]);
            break;
        case OP_ADD: {
            assert(isreftmp(i->dst));
            reftostr(fn, bufs[0], i->dst);
            reftostr(fn, bufs[1], i->args[0]);
            reftostr(fn, bufs[2], i->args[1]);
            fprintf(t->out, "mov %s, %s\n", bufs[0], bufs[1]);
            fprintf(t->out, "add %s, %s\n", bufs[0], bufs[2]);
            break;
        }
        case OP_LOAD8:
            fprintf(t->out, "movzx %s, byte [%s]\n",
                    rstr(tmpr(fn, i->dst.val)),
                    rstr(tmpr(fn, i->args[0].val)));
            break;
        case OP_STORE8:
            fprintf(t->out, "mov byte [%s], %s\n",
                    rstr(tmpr(fn, i->args[0].val)),
                    rstrb(tmpr(fn, i->args[1].val)));
            break;
        case OP_NOT: {
            fprintf(t->out, "cmp %s, 0\n", rstr(tmpr(fn, i->args[0].val)));
            fprintf(t->out, "setz %s\n", rstrb(tmpr(fn, i->dst.val)));
            break;
        }
        case OP_CJMP:
            fprintf(t->out, "cmp %s, 0\n", rstr(tmpr(fn, i->args[0].val)));
            fprintf(t->out, "jnz .L%i\n",  i->args[1].val);
            break;
        case OP_JMP:
            fprintf(t->out, "jmp .L%i\n",  i->args[0].val);
            break;
        case OP_ALLOC: {
            assert(isreftmp(i->dst));
            assert(isrefint(fn, i->args[0]));
            int size = fn->cons[i->args[0].val].as.int_;
            int slots = (size + 15) / 16;
            fprintf(t->out, "sub rsp, %i * %i\n", slots, 16);
            fprintf(t->out, "mov %s, rsp\n", rstr(tmpr(fn, i->dst.val)));
            break;
        }
        case OP_MOV: {
            assert(isreftmp(i->dst));
            reftostr(fn, bufs[0], i->args[0]);
            fprintf(t->out, "mov %s, %s\n", rstr(tmpr(fn, i->dst.val)), bufs[0]);
            break;
        }
        default:
            fprintf(t->out, "*** can't generate op [%i]\n", i->op);
            exit(1);
        }
    }
}

static void genfn(Task *t, Func *fn) {
    fprintf(t->out, "; save registers\n");
    int slots = (R_MAX * 8 + 15) / 16;
    fprintf(t->out, "sub rsp, %i\n", slots * 16);
    for (int r = R_R12; r < R_MAX; r++)
        fprintf(t->out, "mov [rbp - %i], %s\n", (r - R_R12 + 1) * 8, rstr(r));
    for (int i = 0; i < fn->nblks; i++)
        genblk(t, fn, fn->blks[i]);
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

static void filter(Func *fn) {
    for (int bi = 0; bi < fn->nblks; bi++) {
        Block *blk = fn->blks[bi];
        for (int ip = 0; ip < blk->inscnt; ip++) {
            Ins *i = &blk->ins[ip];
            if (i->op == OP_ARG) {
                int argn = i->args[0].val;
                assert(argn < fn->target->nparams);
                int reg = fn->target->params[argn];
                int rtmp = fn->target->rtmps[reg];
                assert(isreftmp(i->args[1]));
                *i = imov(reftmp(rtmp), i->args[1]);
            }
        }
    }
}

static void amd64genfn(Task *t, Func *fn) {
    // create pre-colored tmp for each regist
    T.rtmps = malloc(R_MAX * sizeof(int));
    T.nrtmps = R_MAX;
    for (int r = 0; r < R_MAX; r++) {
        int tmp = newtmp(fn);
        fn->tmps[tmp].reg = r;
        fn->tmps[tmp].precolored = 1;
        T.rtmps[r] = tmp;
    }
    // set usable regs for temps
    T.freeregs = 0;
    for (int r = R_R12; r < R_MAX; r++)
        T.freeregs |= 1 << r;
    T.rstr = rstr;
    fn->target = &T;
    filter(fn);
    liveness(fn);
    color(t, fn);
    fprintf(t->out, "bits 64\n");
    fprintf(t->out, "extern putchar\n");
    fprintf(t->out, "extern getchar\n");
    fprintf(t->out, "section .text\n");
    fprintf(t->out, "global %s\n", fn->name);
    fprintf(t->out, "%s:\n", fn->name);
    fprintf(t->out, "push rbp\n");
    fprintf(t->out, "mov rbp, rsp\n");
    fprintf(t->out, "\n");
    fprintf(t->out, "%%if 0\n");
    printfn(t->out, fn);
    fprintf(t->out, "%%endif\n\n");
    genfn(t, fn);
    fprintf(t->out, "\n");
    fprintf(t->out, "mov rsp, rbp\n");
    fprintf(t->out, "pop rbp\n");
    fprintf(t->out, "mov rax, 0\n");
    fprintf(t->out, "ret\n");
}

void amd64gen(Task *t) {
    for (int i = 0; i < t->m->nfns; i++)
        amd64genfn(t, t->m->fns[i]);
}
