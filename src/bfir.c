#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/ir.h>
#include <brainfuck/brainfuck.h>

static Block *findblk(Chunk *chunk, int lbl) {
    for (int i = 0; i < chunk->blkcnt; i++)
        if (chunk->blocks[i].lbl == lbl) return &chunk->blocks[i];
    return 0;
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
        case OP_WRITE: bfwrite(data[DP]); break;
        case OP_READ: data[DP] = bfread(); break;
        case OP_ADD:
            assert(isreftmp(i->args[0]));
            assert(isrefint(chunk, i->args[1]));
            tmps[i->args[0].val] += chunk->cons[i->args[1].val].as.int_;
            break;
        case OP_LOAD8:
            assert(isreftmp(i->args[0]));
            assert(isreftmp(i->args[1]));
            tmps[i->args[0].val] = data[tmps[i->args[1].val]];
            break;
        case OP_STORE8:
            assert(isreftmp(i->args[0]));
            assert(isreftmp(i->args[1]));
            data[tmps[i->args[1].val]] = tmps[i->args[0].val];
            break;
        case OP_JMP: {
            blk = findblk(chunk, i->args[0].val);
            ip = -1; // compensate for ip++
            break;
        }
        case OP_CJMP: {
            if (!tmps[i->args[0].val]) break;
            blk = findblk(chunk, i->args[1].val);
            ip = -1; // compensate for ip++
            break;
        }
        case OP_NOT:
            tmps[i->args[0].val] = !tmps[i->args[0].val];
            break;
        case OP_NOP: break;
        default:
            printf("*** unknown opcode [%i]\n", i->op);
            exit(1);
        }
    }
    free(tmps);
}

static void pushblk(Chunk *chunk) {
    int blkid = newblk(chunk);
    chunk->curblk = &chunk->blocks[blkid];
    chunk->curblk->lbl = newlbl(chunk);
}

static void emit(Chunk *chunk, Ins ins) {
    emitins(chunk->curblk, ins);
}

static int _genir(Chunk *chunk, char *src, int ip) {
    for (; src[ip]; ip++) {
        switch (src[ip]) {
        case '<':
            emit(chunk, iadd(
                    reftmp(chunk->dptmpid),
                    refcons(newint(chunk, -1))));
            break;
        case '>':
            emit(chunk, iadd(
                    reftmp(chunk->dptmpid),
                    refcons(newint(chunk, 1))));
            break;
        case '+': {
            int tmp = newtmp(chunk);
            emit(chunk, iload8(reftmp(tmp), reftmp(chunk->dptmpid)));
            emit(chunk, iadd(reftmp(tmp), refcons(newint(chunk, 1))));
            emit(chunk, istore8(reftmp(tmp), reftmp(chunk->dptmpid)));
            break;
        }
        case '-': {
            int tmp = newtmp(chunk);
            emit(chunk, iload8(reftmp(tmp), reftmp(chunk->dptmpid)));
            emit(chunk, iadd(reftmp(tmp), refcons(newint(chunk, -1))));
            emit(chunk, istore8(reftmp(tmp), reftmp(chunk->dptmpid)));
            break;
        }
        case '.': emit(chunk, (Ins){OP_WRITE}); break;
        case ',': emit(chunk, (Ins){OP_READ}); break;
        case '[': {
            int tmp = newtmp(chunk);
            int startlbl = newlbl(chunk);
            int endlbl = newlbl(chunk);
            // emit head
            emit(chunk, iload8(reftmp(tmp), reftmp(chunk->dptmpid)));
            emit(chunk, inot(reftmp(tmp)));
            emit(chunk, icjmp(reftmp(tmp), reflbl(endlbl)));
            // finish the block with a jump to the next one
            emit(chunk, ijmp(reflbl(startlbl)));
            // create new block
            pushblk(chunk);
            chunk->curblk->lbl = startlbl;
            // emit enclosed code
            ip = _genir(chunk, src, ip + 1);
            // emit tail
            emit(chunk, iload8(reftmp(tmp), reftmp(chunk->dptmpid)));
            emit(chunk, icjmp(reftmp(tmp), reflbl(startlbl)));
            // finish the block with a jump to the next one
            emit(chunk, ijmp(reflbl(endlbl)));
            // create new block
            pushblk(chunk);
            chunk->curblk->lbl = endlbl;
            break;
        }
        case ']': return ip;
        }
    }
    return ip;
}

void bftoir(Chunk *chunk, char *src) {
    pushblk(chunk);
    chunk->dptmpid = newtmp(chunk);
    _genir(chunk, src, 0);
}

void interpretir(char *src) {
    Chunk chunk = {0};
    bftoir(&chunk, src);
    // printchunk(&chunk);
    runir(&chunk);
}