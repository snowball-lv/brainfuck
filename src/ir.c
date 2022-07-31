#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/ir.h>

int newblk(Chunk *chunk) {
    chunk->blkcnt++;
    chunk->blocks = realloc(chunk->blocks, chunk->blkcnt * sizeof(Block));
    chunk->blocks[chunk->blkcnt - 1] = (Block){0};
    return chunk->blkcnt - 1;
}

int newlbl(Chunk *chunk) {
    // make label 0 invalid
    if (!chunk->lblcnt) chunk->lblcnt++;
    return chunk->lblcnt++;
}

int newtmp(Chunk *chunk) {
    chunk->tmpcnt++;
    chunk->tmps = realloc(chunk->tmps, chunk->tmpcnt * sizeof(Tmp));
    chunk->tmps[chunk->tmpcnt - 1] = (Tmp){0};
    return chunk->tmpcnt - 1;
}

static int newcons(Chunk *chunk) {
    chunk->conscnt++;
    chunk->cons = realloc(chunk->cons, chunk->conscnt * sizeof(Cons));
    chunk->cons[chunk->conscnt - 1] = (Cons){0};
    return chunk->conscnt - 1;
}

int newint(Chunk *chunk, int i) {
    int id = newcons(chunk);
    chunk->cons[id].type = CONS_INT;
    chunk->cons[id].as.int_ = i;
    return id;
}

int newstr(Chunk *chunk, char *str) {
    int id = newcons(chunk);
    chunk->cons[id].type = CONS_STR;
    chunk->cons[id].as.str = str;
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

int isrefint(Chunk *chunk, Ref r) {
    return isrefcons(r) && chunk->cons[r.val].type == CONS_INT;
}

int isrefstr(Chunk *chunk, Ref r) {
    return isrefcons(r) && chunk->cons[r.val].type == CONS_STR;
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

Ins icall(Ref name) {
    assert(isrefcons(name));
    return (Ins){.op = OP_CALL, .args = {name}};
}

Ins iscratch() {
    return (Ins){.op = OP_SCRATCH};
}

static void reftostr(Chunk *chunk, char *buf, Ref r) {
    sprintf(buf, "[???]");
    if (r.type == REF_TMP) sprintf(buf, "$%i", r.val);
    if (r.type == REF_CONS) {
        Cons *cons = &chunk->cons[r.val];
        if (cons->type == CONS_INT) sprintf(buf, "%i", cons->as.int_);
        if (cons->type == CONS_STR) sprintf(buf, "%s", cons->as.str);
    }
    if (r.type == REF_LBL) sprintf(buf, "@L%i", r.val);
}

void printins(Chunk *chunk, Ins *i) {
    char bufs[3][16];
    switch (i->op) {
    case OP_NOP: printf("NOP\n"); break;
    case OP_ADD:
        reftostr(chunk, bufs[0], i->dst);
        reftostr(chunk, bufs[1], i->args[0]);
        reftostr(chunk, bufs[2], i->args[1]);
        printf("%s = ADD %s, %s\n", bufs[0], bufs[1], bufs[2]);
        break;
    case OP_LOAD8:
        reftostr(chunk, bufs[0], i->dst);
        reftostr(chunk, bufs[1], i->args[0]);
        printf("%s = LOAD BYTE [%s]\n", bufs[0], bufs[1]);
        break;
    case OP_STORE8:
        reftostr(chunk, bufs[0], i->args[0]);
        reftostr(chunk, bufs[1], i->args[1]);
        printf("[%s] = STORE BYTE %s\n", bufs[0], bufs[1]);
        break;
    case OP_JMP:
        reftostr(chunk, bufs[0], i->args[0]);
        printf("JMP %s\n", bufs[0]);
        break;
    case OP_CJMP:
        reftostr(chunk, bufs[0], i->args[0]);
        reftostr(chunk, bufs[1], i->args[1]);
        printf("CJMP %s, %s\n", bufs[0], bufs[1]);
        break;
    case OP_NOT:
        reftostr(chunk, bufs[0], i->dst);
        reftostr(chunk, bufs[1], i->args[0]);
        printf("%s = NOT %s\n", bufs[0], bufs[1]);
        break;
    case OP_ALLOC:
        reftostr(chunk, bufs[0], i->dst);
        reftostr(chunk, bufs[1], i->args[0]);
        printf("%s = ALLOC %s\n", bufs[0], bufs[1]);
        break;
    case OP_MOV:
        reftostr(chunk, bufs[0], i->dst);
        reftostr(chunk, bufs[1], i->args[0]);
        printf("%s = %s\n", bufs[0], bufs[1]);
        break;
    case OP_SCRATCH:
        printf("SCRATCH\n");
        break;
    case OP_CALL:
        reftostr(chunk, bufs[0], i->args[0]);
        printf("CALL [%s]\n", bufs[0]);
        break;
    default: printf("???\n"); break;
    }
}

static void printblock(Chunk *chunk, Block *blk) {
    printf("Block @L%i ->", blk->lbl);
    if (blk->outlbls[0]) printf(" @L%i", blk->outlbls[0]);
    if (blk->outlbls[1]) printf(" @L%i", blk->outlbls[1]);
    printf("\n");
    if (blk->livein) {
        printf("Live in");
        for (int tmp = 0; tmp < chunk->tmpcnt; tmp++)
            if (blk->livein[tmp]) printf(" $%i", tmp);
        printf("\n");
    }
    if (blk->liveout) {
        printf("Live out");
        for (int tmp = 0; tmp < chunk->tmpcnt; tmp++)
            if (blk->liveout[tmp]) printf(" $%i", tmp);
        printf("\n");
    }
    for (int ip = 0; ip < blk->inscnt; ip++) {
        printf("%3i: ", ip);
        printins(chunk, &blk->ins[ip]);
    }
}

void printchunk(Chunk *chunk) {
    for (int i = 0; i < chunk->blkcnt; i++)
        printblock(chunk, &chunk->blocks[i]);
}

Block *findblk(Chunk *chunk, int lbl) {
    for (int i = 0; i < chunk->blkcnt; i++)
        if (chunk->blocks[i].lbl == lbl) return &chunk->blocks[i];
    return 0;
}

static void setunion(char *dst, char *src, int cnt) {
    for (int i = 0; i < cnt; i++)
        dst[i] = dst[i] || src[i];
}

#define DEF(n) ((i->def & BIT((n))) && isreftmp(i->args[(n)]))
#define USE(n) ((i->use & BIT((n))) && isreftmp(i->args[(n)]))
#define TMP(n) (i->args[(n)].val)

static int blkliveness(Chunk *chunk, Block *blk) {
    int change = 0;
    char *set = malloc(chunk->tmpcnt);
    memcpy(set, blk->liveout, chunk->tmpcnt);
    for (int i = 0; i < 2; i++) {
        if (!blk->outlbls[i]) continue;
        Block *outblk = findblk(chunk, blk->outlbls[i]);
        setunion(blk->liveout, outblk->livein, chunk->tmpcnt);
    }
    change = memcmp(set, blk->liveout, chunk->tmpcnt);
    memcpy(set, blk->liveout, chunk->tmpcnt);
    for (int ip = blk->inscnt - 1; ip >= 0; ip--) {
        Ins *i = &blk->ins[ip];
        // def
        if (isreftmp(i->dst)) set[i->dst.val] = 0;
        if (i->op == OP_CALL || i->op == OP_SCRATCH) {
            Target *t = chunk->target;
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
            Target *t = chunk->target;
            for (int i = 0; i < t->nparams; i++) {
                int r = t->params[i];
                int tmp = t->rtmps[r];
                set[tmp] = 1;
            }
        }
    }
    change |= memcmp(set, blk->livein, chunk->tmpcnt);
    memcpy(blk->livein, set, chunk->tmpcnt);
    free(set);
    return change;
}

void liveness(Chunk *chunk) {
    for (int bi = 0; bi < chunk->blkcnt; bi++) {
        Block *blk = &chunk->blocks[bi];
        blk->livein = malloc(chunk->tmpcnt);
        blk->liveout = malloc(chunk->tmpcnt);
        memset(blk->livein, 0, chunk->tmpcnt);
        memset(blk->liveout, 0, chunk->tmpcnt);
    }
again:
    for (int bi = 0; bi < chunk->blkcnt; bi++) {
        Block *blk = &chunk->blocks[bi];
        if (blkliveness(chunk, blk)) goto again;
    }
}

void interferes(Chunk *chunk, char *set, int tmp) {
    char *live = malloc(chunk->tmpcnt);
    for (int bi = 0; bi < chunk->blkcnt; bi++) {
        Block *blk = &chunk->blocks[bi];
        memcpy(live, blk->liveout, chunk->tmpcnt);
        if (live[tmp]) setunion(set, live, chunk->tmpcnt);
        for (int ip = blk->inscnt - 1; ip >= 0; ip--) {
            Ins *i = &blk->ins[ip];
            // add defined tmps to live set
            if (isreftmp(i->dst)) live[i->dst.val] = 1;
            if (i->op == OP_CALL || i->op == OP_SCRATCH) {
                for (int i = 0; i < chunk->target->nscratch; i++) {
                    int r = chunk->target->scratch[i];
                    int tmp = chunk->target->rtmps[r];
                    live[tmp] = 1;
                }
            }
            if (live[tmp]) setunion(set, live, chunk->tmpcnt);
            // update live set
            // def
            if (isreftmp(i->dst)) live[i->dst.val] = 0;
            if (i->op == OP_CALL || i->op == OP_SCRATCH) {
                for (int i = 0; i < chunk->target->nscratch; i++) {
                    int r = chunk->target->scratch[i];
                    int tmp = chunk->target->rtmps[r];
                    live[tmp] = 0;
                }
            }
            // use
            if (isreftmp(i->args[0])) live[i->args[0].val] = 1;
            if (isreftmp(i->args[1])) live[i->args[1].val] = 1;
            if (i->op == OP_CALL) {
                for (int i = 0; i < chunk->target->nparams; i++) {
                    int r = chunk->target->params[i];
                    int tmp = chunk->target->rtmps[r];
                    live[tmp] = 1;
                }
            }
            if (live[tmp]) setunion(set, live, chunk->tmpcnt);
        }
    }
    free(live);
    // don't interfere with yourself
    set[tmp] = 0;
}
