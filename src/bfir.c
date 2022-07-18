#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/brainfuck.h>
#include <brainfuck/ir.h>

enum {
    OP_NONE,
    OP_MOVL, OP_MOVR,
    OP_INC, OP_DEC,
    OP_WRITE, OP_READ,
    OP_JMPFWD, OP_JMPBCK,
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

static int newblk(Chunk *chunk) {
    chunk->blkcnt++;
    chunk->blocks = realloc(chunk->blocks, chunk->blkcnt * sizeof(Block));
    chunk->blocks[chunk->blkcnt - 1] = (Block){0};
    return chunk->blkcnt - 1;
}

static int newlbl(Chunk *chunk) {
    return chunk->lblcnt++;
}

static int newtmp(Chunk *chunk) {
    return chunk->tmpcnt++;
}

static int _newcons(Chunk *chunk) {
    chunk->conscnt++;
    chunk->cons = realloc(chunk->cons, chunk->conscnt * sizeof(Cons));
    chunk->cons[chunk->conscnt - 1] = (Cons){0};
    return chunk->conscnt - 1;
}

static int newint(Chunk *chunk, int i) {
    int id = _newcons(chunk);
    chunk->cons[id].type = CONS_INT;
    chunk->cons[id].as.int_ = i;
    return id;
}

static Ref reftmp(int id) {
    return (Ref){.type = REF_TMP, .val = id};
}

static Ref reflbl(int id) {
    return (Ref){.type = REF_LBL, .val = id};
}

static Ref refcons(int id) {
    return (Ref){.type = REF_CONS, .val = id};
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

static void printblock(Chunk *chunk, Block *blk) {
    char bufs[2][16];
    for (int ip = 0; ip < blk->inscnt; ip++) {
        printf("%3i: ", ip);
        Ins *i = &blk->ins[ip];
        switch (i->op) {
        case OP_MOVL: printf("MOVL\n"); break;
        case OP_MOVR: printf("MOVR\n"); break;
        case OP_INC: printf("INC\n"); break;
        case OP_DEC: printf("DEC\n"); break;
        case OP_WRITE: printf("WRITE\n"); break;
        case OP_READ: printf("READ\n"); break;
        case OP_JMPFWD: printf("JMPFWD\n"); break;
        case OP_JMPBCK: printf("JMPBCK\n"); break;
        case OP_NOP: printf("NOP\n"); break;
        case OP_ADD:
            reftostr(chunk, bufs[0], i->args[0]);
            reftostr(chunk, bufs[1], i->args[1]);
            printf("ADD %s, %s\n", bufs[0], bufs[1]);
            break;
        case OP_LOAD:
            reftostr(chunk, bufs[0], i->args[0]);
            reftostr(chunk, bufs[1], i->args[1]);
            printf("LOAD %s, [%s]\n", bufs[0], bufs[1]);
            break;
        case OP_STORE:
            reftostr(chunk, bufs[0], i->args[0]);
            reftostr(chunk, bufs[1], i->args[1]);
            printf("STORE %s, [%s]\n", bufs[0], bufs[1]);
            break;
        case OP_CJMP:
            reftostr(chunk, bufs[0], i->args[0]);
            reftostr(chunk, bufs[1], i->args[1]);
            printf("CJMP %s, %s\n", bufs[0], bufs[1]);
            break;
        case OP_LABEL:
            reftostr(chunk, bufs[0], i->args[0]);
            printf("%s:\n", bufs[0]);
            break;
        case OP_NOT:
            reftostr(chunk, bufs[0], i->args[0]);
            printf("NOT %s\n", bufs[0]);
            break;
        default: printf("???\n"); break;
        }
    }
}

static void printchunk(Chunk *chunk) {
    for (int i = 0; i < chunk->blkcnt; i++)
        printblock(chunk, &chunk->blocks[i]);
}

static int isreftmp(Ref r) {
    return r.type == REF_TMP;
}

static int isrefint(Chunk *chunk, Ref r) {
    return r.type == REF_CONS && chunk->cons[r.val].type == CONS_INT;
}

static void runir(Chunk *chunk) {
    char data[30000] = {0};
    int *tmps = malloc(chunk->tmpcnt * sizeof(int));
    memset(tmps, 0, chunk->tmpcnt * sizeof(int));
    #define DP tmps[chunk->dptmpid]
    Block *blk = &chunk->blocks[0];
    for (int ip = 0; ip < blk->inscnt; ip++) {
        Ins *i = &blk->ins[ip];
        switch (i->op) {
        // case OP_MOVL: DP--; break;
        // case OP_MOVR: DP++; break;
        // case OP_INC: data[DP]++; break;
        // case OP_DEC: data[DP]--; break;
        case OP_WRITE: bfwrite(data[DP]); break;
        case OP_READ: data[DP] = bfread(); break;
        // case OP_JMPFWD:
        //     if (data[DP]) break;
        //     ip++;
        //     for (int depth = 0;; ip++) {
        //         if (chunk->ins[ip].op == OP_JMPBCK)
        //             if (depth == 0) break;
        //             else depth--;
        //         else if (chunk->ins[ip].op == OP_JMPFWD)
        //             depth++;
        //     }
        //     break;
        // case OP_JMPBCK:
        //     if (!data[DP]) break;
        //     ip--;
        //     for (int depth = 0;; ip--) {
        //         if (chunk->ins[ip].op == OP_JMPFWD)
        //             if (depth == 0) break;
        //             else depth--;
        //         else if (chunk->ins[ip].op == OP_JMPBCK)
        //             depth++;
        //     }
        //     break;
        case OP_ADD:
            assert(isreftmp(i->args[0]));
            assert(isrefint(chunk, i->args[1]));
            tmps[i->args[0].val] += chunk->cons[i->args[1].val].as.int_;
            break;
        case OP_LOAD:
            assert(isreftmp(i->args[0]));
            assert(isreftmp(i->args[1]));
            tmps[i->args[0].val] = data[tmps[i->args[1].val]];
            break;
        case OP_STORE:
            assert(isreftmp(i->args[0]));
            assert(isreftmp(i->args[1]));
            data[tmps[i->args[1].val]] = tmps[i->args[0].val];
            break;
        case OP_CJMP:
            if (!tmps[i->args[0].val]) break;
            int k = 0;
            for (; k < blk->inscnt; k++) {
                Ins *target = &blk->ins[k];
                if (target->op != OP_LABEL) continue;
                if (target->args[0].val == i->args[1].val) break;
            }
            ip = k - 1; // compensate for ip++
            break;
        case OP_NOT:
            tmps[i->args[0].val] = !tmps[i->args[0].val];
            break;
        case OP_LABEL:
            break;
        case OP_NOP: break;
        default:
            printf("*** unknown opcode [%i]\n", i->op);
            exit(1);
        }
    }
    free(tmps);
}

static void erase(Block *blk, int pos, int cnt) {
    int tailcnt = blk->inscnt - (pos + cnt);
    memmove(&blk->ins[pos], &blk->ins[pos + cnt], tailcnt * sizeof(Ins));
    blk->inscnt -= cnt;
}

static void insert(Block *blk, int pos, Ins *ins, int cnt) {
    int tailcnt = blk->inscnt - pos;
    blk->inscnt += cnt;
    blk->ins = realloc(blk->ins, blk->inscnt * sizeof(Ins));
    memmove(&blk->ins[pos + cnt], &blk->ins[pos], tailcnt * sizeof(Ins));
    memmove(&blk->ins[pos], ins, cnt * sizeof(Ins));
}

static Ins iload(Ref dst, Ref src) {
    return (Ins){.op = OP_LOAD, .args = {dst, src}};
}

static Ins istore(Ref dst, Ref src) {
    return (Ins){.op = OP_STORE, .args = {dst, src}};
}

static Ins iadd(Ref dst, Ref src) {
    return (Ins){.op = OP_ADD, .args = {dst, src}};
}

static Ins icjmp(Ref src, Ref lbl) {
    return (Ins){.op = OP_CJMP, .args = {src, lbl}};
}

static Ins ilabel(Ref lbl) {
    return (Ins){.op = OP_LABEL, .args = {lbl}};
}

static Ins inot(Ref tmp) {
    return (Ins){.op = OP_NOT, .args = {tmp}};
}

static int opt(Chunk *chunk) {
    int changed = 0;
    Block *blk = &chunk->blocks[0];
    for (int ip = 0; ip < blk->inscnt; ip++) {
        Ins *i = &blk->ins[ip];
        if (i->op == OP_MOVL || i->op == OP_MOVR) {
            int off = i->op == OP_MOVL ? -1 : 1;
            i->op = OP_ADD;
            i->args[0] = reftmp(chunk->dptmpid);
            i->args[1] = refcons(newint(chunk, off));
            changed = 1;
        }
        else if (i->op == OP_INC || i->op == OP_DEC) {
            int off = i->op == OP_INC ? 1 : -1;
            int tmp = newtmp(chunk);
            Ins ins[] = {
                iload(reftmp(tmp), reftmp(chunk->dptmpid)),
                iadd(reftmp(tmp), refcons(newint(chunk, off))),
                istore(reftmp(tmp), reftmp(chunk->dptmpid)),
            };
            i->op = OP_NOP;
            insert(blk, ip, ins, sizeof(ins) / sizeof(Ins));
            changed = 1;
        }
        else if (i->op == OP_JMPFWD) {
            int dstip = ip + 1;
            for (int depth = 0;; dstip++) {
                if (blk->ins[dstip].op == OP_JMPBCK)
                    if (depth == 0) break;
                    else depth--;
                else if (blk->ins[dstip].op == OP_JMPFWD)
                    depth++;
            }
            i->op = OP_NOP;
            blk->ins[dstip].op = OP_NOP;
            int startlbl = newlbl(chunk);
            int endlbl = newlbl(chunk);
            // insert loop end
            int endtmp = newtmp(chunk);
            Ins endins[] = {
                iload(reftmp(endtmp), reftmp(chunk->dptmpid)),
                icjmp(reftmp(endtmp), reflbl(startlbl)),
                ilabel(reflbl(endlbl)),
            };
            insert(blk, dstip, endins, sizeof(endins) / sizeof(Ins));
            // insert loop start
            int starttmp = newtmp(chunk);
            Ins startins[] = {
                iload(reftmp(starttmp), reftmp(chunk->dptmpid)),
                inot(reftmp(starttmp)),
                icjmp(reftmp(starttmp), reflbl(endlbl)),
                ilabel(reflbl(startlbl)),
            };
            insert(blk, ip, startins, sizeof(startins) / sizeof(Ins));
            ip += sizeof(startins) / sizeof(Ins); // jump over fwd
            changed = 1;
        }
        else if (i->op == OP_NOP) {
            int end = ip + 1;
            for (; end < blk->inscnt; end++) {
                if (blk->ins[end].op != OP_NOP) break;
            }
            erase(blk, ip, end - ip);
            changed = 1;
        }
    }
    return changed;
}

static void emitins(Block *blk, Ins i) {
    blk->inscnt++;
    blk->ins = realloc(blk->ins, blk->inscnt * sizeof(Ins));
    blk->ins[blk->inscnt - 1] = i;
}

static void genir(Chunk *chunk, char *src) {
    int blkid = newblk(chunk);
    Block *blk = &chunk->blocks[blkid];
    blk->lbl = newlbl(chunk);
    chunk->dptmpid = newtmp(chunk);
    for (int ip = 0; src[ip]; ip++) {
        switch (src[ip]) {
        case '<': emitins(blk, (Ins){OP_MOVL}); break;
        case '>': emitins(blk, (Ins){OP_MOVR}); break;
        case '+': emitins(blk, (Ins){OP_INC}); break;
        case '-': emitins(blk, (Ins){OP_DEC}); break;
        case '.': emitins(blk, (Ins){OP_WRITE}); break;
        case ',': emitins(blk, (Ins){OP_READ}); break;
        case '[': emitins(blk, (Ins){OP_JMPFWD}); break;
        case ']': emitins(blk, (Ins){OP_JMPBCK}); break;
        }
    }
}

void interpretir(char *src) {
    Chunk chunk = {0};
    genir(&chunk, src);
    while (opt(&chunk));
    // printchunk(&chunk);
    runir(&chunk);
}
