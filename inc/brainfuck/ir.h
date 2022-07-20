#pragma once

enum {
    OP_NONE,
    OP_WRITE, OP_READ,
    OP_ADD,
    OP_LOAD, OP_STORE,
    OP_CJMP,
    OP_LABEL,
    OP_NOT,
    OP_NOP,
};

typedef struct {
    enum {
        REF_NONE,
        REF_TMP,
        REF_CONS,
        REF_LBL,
    } type;
    int val;
} Ref;

typedef struct {
    int op;
    Ref args[2];
} Ins;

typedef struct {
    enum {
        CONS_NONE,
        CONS_INT,
    } type;
    union {
        int int_;
    } as;
} Cons;

typedef struct {
    Ins *ins;
    int inscnt;
    int lbl;
} Block;

typedef struct {
    Block *blocks;
    int blkcnt;
    int tmpcnt;
    Cons *cons;
    int conscnt;
    int dptmpid;
    int lblcnt;
} Chunk;

int newblk(Chunk *chunk);
int newlbl(Chunk *chunk);
int newtmp(Chunk *chunk);
int newint(Chunk *chunk, int i);
Ref reftmp(int id);
Ref reflbl(int id);
Ref refcons(int id);

int isreftmp(Ref r);
int isrefint(Chunk *chunk, Ref r);

void emitins(Block *blk, Ins i);
void erase(Block *blk, int pos, int cnt);
void insert(Block *blk, int pos, Ins *ins, int cnt);

Ins iload(Ref dst, Ref src);
Ins istore(Ref dst, Ref src);
Ins iadd(Ref dst, Ref src);
Ins icjmp(Ref src, Ref lbl);
Ins ilabel(Ref lbl);
Ins inot(Ref tmp);

void printchunk(Chunk *chunk);