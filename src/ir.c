#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/common.h>
#include <brainfuck/brainfuck.h>
#include <brainfuck/ir.h>

Mod *newmod() {
    Mod *m = malloc(sizeof(Mod));
    memset(m, 0, sizeof(Mod));
    return m;
}

Func *newfn(Mod *m) {
    Func *fn = malloc(sizeof(Func));
    memset(fn, 0, sizeof(Func));
    m->nfns++;
    m->fns = realloc(m->fns, m->nfns * sizeof(Func *));
    m->fns[m->nfns - 1] = fn;
    return fn;
}

Block *newblk(Func *fn) {
    Block *blk = malloc(sizeof(Block));
    memset(blk, 0, sizeof(Block));
    fn->nblks++;
    fn->blks = realloc(fn->blks, fn->nblks * sizeof(Block *));
    fn->blks[fn->nblks - 1] = blk;
    return blk;
}

int newlbl(Func *fn) {
    // make label 0 invalid
    if (!fn->lblcnt) fn->lblcnt++;
    return fn->lblcnt++;
}

int newtmp(Func *fn) {
    fn->tmpcnt++;
    fn->tmps = realloc(fn->tmps, fn->tmpcnt * sizeof(Tmp));
    fn->tmps[fn->tmpcnt - 1] = (Tmp){0};
    return fn->tmpcnt - 1;
}

static int newcons(Func *fn) {
    fn->conscnt++;
    fn->cons = realloc(fn->cons, fn->conscnt * sizeof(Cons));
    fn->cons[fn->conscnt - 1] = (Cons){0};
    return fn->conscnt - 1;
}

int newint(Func *fn, int i) {
    int id = newcons(fn);
    fn->cons[id].type = CONS_INT;
    fn->cons[id].as.int_ = i;
    return id;
}

int newstr(Func *fn, char *str) {
    int id = newcons(fn);
    fn->cons[id].type = CONS_STR;
    fn->cons[id].as.str = str;
    return id;
}

Ref reftmp(int id) {
    return (Ref){.type = REF_TMP, .val = id};
}

Ref reflbl(int id) {
    return (Ref){.type = REF_LBL, .val = id};
}

Ref refcons(int id) {
    return (Ref){.type = REF_CONS, .val = id};
}

int isreftmp(Ref r) {
    return r.type == REF_TMP;
}

int isrefcons(Ref r) {
    return r.type == REF_CONS;
}

int isrefint(Func *fn, Ref r) {
    return isrefcons(r) && fn->cons[r.val].type == CONS_INT;
}

int isrefstr(Func *fn, Ref r) {
    return isrefcons(r) && fn->cons[r.val].type == CONS_STR;
}

void emitins(Block *blk, Ins ins) {
    blk->inscnt++;
    blk->ins = realloc(blk->ins, blk->inscnt * sizeof(Ins));
    blk->ins[blk->inscnt - 1] = ins;
}

void erase(Block *blk, int pos, int cnt) {
    int tailcnt = blk->inscnt - (pos + cnt);
    memmove(&blk->ins[pos], &blk->ins[pos + cnt], tailcnt * sizeof(Ins));
    blk->inscnt -= cnt;
}

void insert(Block *blk, int pos, Ins *ins, int cnt) {
    int tailcnt = blk->inscnt - pos;
    blk->inscnt += cnt;
    blk->ins = realloc(blk->ins, blk->inscnt * sizeof(Ins));
    memmove(&blk->ins[pos + cnt], &blk->ins[pos], tailcnt * sizeof(Ins));
    memmove(&blk->ins[pos], ins, cnt * sizeof(Ins));
}

#define BIT(n) (1 << (n))

Ins iload8(Ref dst, Ref src) {
    assert(isreftmp(dst));
    assert(isreftmp(src));
    return (Ins){.op = OP_LOAD8, .dst = dst, .args = {src}};
}

Ins istore8(Ref dst, Ref src) {
    assert(isreftmp(dst));
    assert(isreftmp(src));
    return (Ins){.op = OP_STORE8, .args = {dst, src}};
}

Ins iadd(Ref dst, Ref src) {
    assert(isreftmp(dst));
    // assert(isreftmp(src));
    return (Ins){.op = OP_ADD, .dst = dst, .args = {dst, src}};
}

Ins icjmp(Ref cnd, Ref lbl) {
    assert(isreftmp(cnd));
    return (Ins){.op = OP_CJMP, .args = {cnd, lbl}};
}

Ins ijmp(Ref lbl) {
    return (Ins){.op = OP_JMP, .args = {lbl}};
}

Ins inot(Ref tmp) {
    assert(isreftmp(tmp));
    return (Ins){.op = OP_NOT, .dst = tmp, .args = {tmp}};
}

Ins ialloc(Ref dst, Ref size) {
    assert(isreftmp(dst));
    assert(isrefcons(size));
    return (Ins){.op = OP_ALLOC, .dst = dst, .args = {size}};
}

Ins imov(Ref dst, Ref src) {
    assert(isreftmp(dst));
    return (Ins){.op = OP_MOV, .dst = dst, .args = {src}};
}

Ins icall(Ref dst, Ref name) {
    assert(isrefcons(name));
    return (Ins){.op = OP_CALL, .dst = dst, .args = {name}};
}

Ins iscratch() {
    return (Ins){.op = OP_SCRATCH};
}

Ins iarg(int n, Ref arg) {
    return (Ins){.op = OP_ARG, .args = {{.val = n}, arg}};
}

static void reftostr(Func *fn, char *buf, Ref r) {
    sprintf(buf, "[???]");
    if (r.type == REF_TMP) sprintf(buf, "$%i", r.val);
    if (r.type == REF_CONS) {
        Cons *cons = &fn->cons[r.val];
        if (cons->type == CONS_INT) sprintf(buf, "%i", cons->as.int_);
        if (cons->type == CONS_STR) sprintf(buf, "%s", cons->as.str);
    }
    if (r.type == REF_LBL) sprintf(buf, "@L%i", r.val);
}

void printins(FILE *out, Func *fn, Ins *i) {
    char bufs[3][16];
    switch (i->op) {
    case OP_NOP: fprintf(out, "NOP\n"); break;
    case OP_ADD:
        reftostr(fn, bufs[0], i->dst);
        reftostr(fn, bufs[1], i->args[0]);
        reftostr(fn, bufs[2], i->args[1]);
        fprintf(out, "%s = ADD %s, %s\n", bufs[0], bufs[1], bufs[2]);
        break;
    case OP_LOAD8:
        reftostr(fn, bufs[0], i->dst);
        reftostr(fn, bufs[1], i->args[0]);
        fprintf(out, "%s = LOAD BYTE [%s]\n", bufs[0], bufs[1]);
        break;
    case OP_STORE8:
        reftostr(fn, bufs[0], i->args[0]);
        reftostr(fn, bufs[1], i->args[1]);
        fprintf(out, "[%s] = STORE BYTE %s\n", bufs[0], bufs[1]);
        break;
    case OP_JMP:
        reftostr(fn, bufs[0], i->args[0]);
        fprintf(out, "JMP %s\n", bufs[0]);
        break;
    case OP_CJMP:
        reftostr(fn, bufs[0], i->args[0]);
        reftostr(fn, bufs[1], i->args[1]);
        fprintf(out, "CJMP %s, %s\n", bufs[0], bufs[1]);
        break;
    case OP_NOT:
        reftostr(fn, bufs[0], i->dst);
        reftostr(fn, bufs[1], i->args[0]);
        fprintf(out, "%s = NOT %s\n", bufs[0], bufs[1]);
        break;
    case OP_ALLOC:
        reftostr(fn, bufs[0], i->dst);
        reftostr(fn, bufs[1], i->args[0]);
        fprintf(out, "%s = ALLOC %s\n", bufs[0], bufs[1]);
        break;
    case OP_MOV:
        reftostr(fn, bufs[0], i->dst);
        reftostr(fn, bufs[1], i->args[0]);
        fprintf(out, "%s = %s\n", bufs[0], bufs[1]);
        break;
    case OP_SCRATCH:
        fprintf(out, "SCRATCH\n");
        break;
    case OP_ARG:
        reftostr(fn, bufs[0], i->args[1]);
        fprintf(out, "ARG #%i, %s\n", i->args[0].val, bufs[0]);
        break;
    case OP_CALL:
        reftostr(fn, bufs[0], i->dst);
        reftostr(fn, bufs[1], i->args[0]);
        fprintf(out, "%s = CALL [%s]\n", bufs[0], bufs[1]);
        break;
    default: fprintf(out, "???\n"); break;
    }
}

static void printblock(FILE *out, Func *fn, Block *blk) {
    fprintf(out, "Block @L%i ->", blk->lbl);
    if (blk->outlbls[0]) fprintf(out, " @L%i", blk->outlbls[0]);
    if (blk->outlbls[1]) fprintf(out, " @L%i", blk->outlbls[1]);
    fprintf(out, "\n");
    if (blk->livein) {
        fprintf(out, "Live in");
        for (int tmp = 0; tmp < fn->tmpcnt; tmp++)
            if (blk->livein[tmp]) fprintf(out, " $%i", tmp);
        fprintf(out, "\n");
    }
    if (blk->liveout) {
        fprintf(out, "Live out");
        for (int tmp = 0; tmp < fn->tmpcnt; tmp++)
            if (blk->liveout[tmp]) fprintf(out, " $%i", tmp);
        fprintf(out, "\n");
    }
    for (int ip = 0; ip < blk->inscnt; ip++) {
        fprintf(out, "%3i: ", ip);
        printins(out, fn, &blk->ins[ip]);
    }
}

void printfn(FILE *out, Func *fn) {
    for (int i = 0; i < fn->nblks; i++)
        printblock(out, fn, fn->blks[i]);
}

Block *findblk(Func *fn, int lbl) {
    for (int i = 0; i < fn->nblks; i++)
        if (fn->blks[i]->lbl == lbl) return fn->blks[i];
    return 0;
}

static void setunion(char *dst, char *src, int cnt) {
    for (int i = 0; i < cnt; i++)
        dst[i] = dst[i] || src[i];
}

#define DEF(n) ((i->def & BIT((n))) && isreftmp(i->args[(n)]))
#define USE(n) ((i->use & BIT((n))) && isreftmp(i->args[(n)]))
#define TMP(n) (i->args[(n)].val)

static int blkliveness(Func *fn, Block *blk) {
    int change = 0;
    char *set = malloc(fn->tmpcnt);
    memcpy(set, blk->liveout, fn->tmpcnt);
    for (int i = 0; i < 2; i++) {
        if (!blk->outlbls[i]) continue;
        Block *outblk = findblk(fn, blk->outlbls[i]);
        setunion(blk->liveout, outblk->livein, fn->tmpcnt);
    }
    change = memcmp(set, blk->liveout, fn->tmpcnt);
    memcpy(set, blk->liveout, fn->tmpcnt);
    for (int ip = blk->inscnt - 1; ip >= 0; ip--) {
        Ins *i = &blk->ins[ip];
        if (i->op == OP_ARG) {
            printf("*** ARG pseudo-ops should be removed\n");
            exit(1);
        }
        // def
        if (isreftmp(i->dst)) set[i->dst.val] = 0;
        if (i->op == OP_CALL || i->op == OP_SCRATCH) {
            Target *t = fn->target;
            for (int i = 0; i < t->nscratch; i++) {
                int r = t->scratch[i];
                int tmp = t->rtmps[r];
                set[tmp] = 0;
            }
        }
        // use
        if (isreftmp(i->args[0])) set[i->args[0].val] = 1;
        if (isreftmp(i->args[1])) set[i->args[1].val] = 1;
        if (i->op == OP_CALL) {
            Target *t = fn->target;
            for (int i = 0; i < t->nparams; i++) {
                int r = t->params[i];
                int tmp = t->rtmps[r];
                set[tmp] = 1;
            }
        }
    }
    change |= memcmp(set, blk->livein, fn->tmpcnt);
    memcpy(blk->livein, set, fn->tmpcnt);
    free(set);
    return change;
}

void liveness(Func *fn) {
    for (int bi = 0; bi < fn->nblks; bi++) {
        Block *blk = fn->blks[bi];
        blk->livein = malloc(fn->tmpcnt);
        blk->liveout = malloc(fn->tmpcnt);
        memset(blk->livein, 0, fn->tmpcnt);
        memset(blk->liveout, 0, fn->tmpcnt);
    }
again:
    for (int bi = 0; bi < fn->nblks; bi++) {
        Block *blk = fn->blks[bi];
        if (blkliveness(fn, blk)) goto again;
    }
}

void interferes(Func *fn, char *set, int tmp) {
    char *live = malloc(fn->tmpcnt);
    for (int bi = 0; bi < fn->nblks; bi++) {
        Block *blk = fn->blks[bi];
        memcpy(live, blk->liveout, fn->tmpcnt);
        if (live[tmp]) setunion(set, live, fn->tmpcnt);
        for (int ip = blk->inscnt - 1; ip >= 0; ip--) {
            Ins *i = &blk->ins[ip];
            // add defined tmps to live set
            if (isreftmp(i->dst)) live[i->dst.val] = 1;
            if (i->op == OP_CALL || i->op == OP_SCRATCH) {
                for (int i = 0; i < fn->target->nscratch; i++) {
                    int r = fn->target->scratch[i];
                    int tmp = fn->target->rtmps[r];
                    live[tmp] = 1;
                }
            }
            if (live[tmp]) setunion(set, live, fn->tmpcnt);
            // update live set
            // def
            if (isreftmp(i->dst)) live[i->dst.val] = 0;
            if (i->op == OP_CALL || i->op == OP_SCRATCH) {
                for (int i = 0; i < fn->target->nscratch; i++) {
                    int r = fn->target->scratch[i];
                    int tmp = fn->target->rtmps[r];
                    live[tmp] = 0;
                }
            }
            // use
            if (isreftmp(i->args[0])) live[i->args[0].val] = 1;
            if (isreftmp(i->args[1])) live[i->args[1].val] = 1;
            if (i->op == OP_CALL) {
                for (int i = 0; i < fn->target->nparams; i++) {
                    int r = fn->target->params[i];
                    int tmp = fn->target->rtmps[r];
                    live[tmp] = 1;
                }
            }
            if (live[tmp]) setunion(set, live, fn->tmpcnt);
        }
    }
    free(live);
    // don't interfere with yourself
    set[tmp] = 0;
}

void color(Task *t, Func *fn) {
    char *set = malloc(fn->tmpcnt);
    for (int tmp = 0; tmp < fn->tmpcnt; tmp++) {
        if (fn->tmps[tmp].precolored) continue;
        fn->tmps[tmp].reg = 0;
        int freeregs = fn->target->freeregs;
        memset(set, 0, fn->tmpcnt);
        interferes(fn, set, tmp);
        fprintf(t->out, "; $%i -", tmp);
        for (int i = 0; i < fn->tmpcnt; i++) {
            if (!set[i]) continue;
            fprintf(t->out, " $%i", i);
            if (!fn->tmps[i].reg) continue;
            fprintf(t->out, "(%s)", fn->target->rstr(fn->tmps[i].reg));
            freeregs &= ~(1 << fn->tmps[i].reg);
        }
        fprintf(t->out, "\n");
        if (!freeregs) {
            fprintf(t->out, "*** spill $%i\n", tmp);
            exit(1);
        }
        int reg = __builtin_ctz(freeregs);
        fn->tmps[tmp].reg = reg;
        fprintf(t->out, "; $%i = %s\n", tmp, fn->target->rstr(reg));
    }
    free(set);
}
