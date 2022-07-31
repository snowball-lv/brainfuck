#pragma once

enum {
    OP_NONE,
    OP_ADD,
    OP_LOAD8, OP_STORE8,
    OP_JMP, OP_CJMP,
    OP_NOT,
    OP_NOP,
    OP_ALLOC,
    OP_MOV,
    OP_CALL,
    OP_SCRATCH,
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
    Ref dst;
    Ref args[2];
} Ins;

typedef struct {
    enum {
        CONS_NONE,
        CONS_INT,
        CONS_STR,
    } type;
    union {
        int int_;
        char *str;
    } as;
} Cons;

typedef struct {
    int reg;
} Tmp;

typedef struct {
    Ins *ins;
    int inscnt;
    int lbl;
    int outlbls[2];
    char *livein, *liveout;
} Block;

typedef struct {
    int *ret;
    int nret;
    int *params;
    int nparams;
    int *scratch;
    int nscratch;
    int *rtmps;
    int nrtmps;
} Target;

typedef struct {
    Block *blocks;
    int blkcnt;
    Block *curblk;
    Tmp *tmps;
    int tmpcnt;
    Cons *cons;
    int conscnt;
    int dptmpid;
    int lblcnt;
    Target *target;
} Chunk;

int newblk(Chunk *chunk);
int newlbl(Chunk *chunk);
int newtmp(Chunk *chunk);
int newint(Chunk *chunk, int i);
int newstr(Chunk *chunk, char *str);
Ref reftmp(int id);
Ref reflbl(int id);
Ref refcons(int id);

int isreftmp(Ref r);
int isrefint(Chunk *chunk, Ref r);
int isrefstr(Chunk *chunk, Ref r);

void emitins(Block *blk, Ins ins);
void erase(Block *blk, int pos, int cnt);
void insert(Block *blk, int pos, Ins *ins, int cnt);

Ins iload8(Ref dst, Ref src);
Ins istore8(Ref dst, Ref src);
Ins iadd(Ref dst, Ref src);
Ins ijmp(Ref lbl);
Ins icjmp(Ref cnd, Ref lbl);
Ins inot(Ref tmp);
Ins ialloc(Ref dst, Ref size);
Ins imov(Ref dst, Ref src);
Ins icall(Ref name);
Ins iscratch();

void printchunk(Chunk *chunk);
void printins(Chunk *chunk, Ins *i);

void liveness(Chunk *chunk);
Block *findblk(Chunk *chunk, int lbl);
void interferes(Chunk *chunk, char *set, int tmp);