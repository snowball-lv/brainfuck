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
    OP_ARG,
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
    char precolored;
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
    int freeregs;
    char *(*rstr)(int reg);
} Target;

typedef struct {
    char *name;
    Block **blks;
    int nblks;
    Tmp *tmps;
    int tmpcnt;
    Cons *cons;
    int conscnt;
    int lblcnt;
    Target *target;
} Func;

struct Mod {
    Func **fns;
    int nfns;
};

Mod *newmod();
Func *newfn(Mod *m);
Block *newblk(Func *fn);
int newlbl(Func *fn);
int newtmp(Func *fn);
int newint(Func *fn, int i);
int newstr(Func *fn, char *str);
Ref reftmp(int id);
Ref reflbl(int id);
Ref refcons(int id);

int isreftmp(Ref r);
int isrefint(Func *fn, Ref r);
int isrefstr(Func *fn, Ref r);

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
Ins icall(Ref dst, Ref name);
Ins iscratch();
Ins iarg(int n, Ref arg);

void printfn(FILE *out, Func *fn);
void printins(FILE *out, Func *fn, Ins *i);

void liveness(Func *fn);
void color(Task *t, Func *fn);
