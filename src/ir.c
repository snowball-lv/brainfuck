#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <brainfuck/ir.h>

int newblk(Chunk *chunk) {
    chunk->blkcnt++;
    chunk->blocks = realloc(chunk->blocks, chunk->blkcnt * sizeof(Block));
    chunk->blocks[chunk->blkcnt - 1] = (Block){0};
    return chunk->blkcnt - 1;
}

int newlbl(Chunk *chunk) {
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

int isrefint(Chunk *chunk, Ref r) {
    return r.type == REF_CONS && chunk->cons[r.val].type == CONS_INT;
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

Ins iload8(Ref dst, Ref src) {
    return (Ins){.op = OP_LOAD8, .args = {dst, src}};
}

Ins istore8(Ref dst, Ref src) {
    return (Ins){.op = OP_STORE8, .args = {dst, src}};
}

Ins iadd(Ref dst, Ref src) {
    return (Ins){.op = OP_ADD, .args = {dst, src}};
}

Ins icjmp(Ref src, Ref lbl) {
    return (Ins){.op = OP_CJMP, .args = {src, lbl}};
}

Ins ijmp(Ref lbl) {
    return (Ins){.op = OP_JMP, .args = {lbl}};
}

Ins inot(Ref tmp) {
    return (Ins){.op = OP_NOT, .args = {tmp}};
}

static void reftostr(Chunk *chunk, char *buf, Ref r) {
    sprintf(buf, "[???]");
    if (r.type == REF_TMP) sprintf(buf, "$%i", r.val);
    if (r.type == REF_CONS) {
        Cons *cons = &chunk->cons[r.val];
        if (cons->type == CONS_INT) sprintf(buf, "%i", cons->as.int_);
    }
    if (r.type == REF_LBL) sprintf(buf, "@L%i", r.val);
}

void printins(Chunk *chunk, Ins *i) {
    char bufs[2][16];
    switch (i->op) {
    case OP_WRITE: printf("WRITE\n"); break;
    case OP_READ: printf("READ\n"); break;
    case OP_NOP: printf("NOP\n"); break;
    case OP_ADD:
        reftostr(chunk, bufs[0], i->args[0]);
        reftostr(chunk, bufs[1], i->args[1]);
        printf("ADD %s, %s\n", bufs[0], bufs[1]);
        break;
    case OP_LOAD8:
        reftostr(chunk, bufs[0], i->args[0]);
        reftostr(chunk, bufs[1], i->args[1]);
        printf("LOAD BYTE %s, [%s]\n", bufs[0], bufs[1]);
        break;
    case OP_STORE8:
        reftostr(chunk, bufs[0], i->args[0]);
        reftostr(chunk, bufs[1], i->args[1]);
        printf("STORE BYTE %s, [%s]\n", bufs[0], bufs[1]);
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
        reftostr(chunk, bufs[0], i->args[0]);
        printf("NOT %s\n", bufs[0]);
        break;
    default: printf("???\n"); break;
    }
}

static void printblock(Chunk *chunk, Block *blk) {
    printf("Block @L%i\n", blk->lbl);
    for (int ip = 0; ip < blk->inscnt; ip++) {
        printf("%3i: ", ip);
        printins(chunk, &blk->ins[ip]);
    }
}

void printchunk(Chunk *chunk) {
    for (int i = 0; i < chunk->blkcnt; i++)
        printblock(chunk, &chunk->blocks[i]);
}