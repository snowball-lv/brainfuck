#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/brainfuck.h>
#include <brainfuck/ir.h>

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

static int _genir(Chunk *chunk, Block *blk, char *src, int ip) {
    for (; src[ip]; ip++) {
        switch (src[ip]) {
        case '<':
            emitins(blk, iadd(
                    reftmp(chunk->dptmpid),
                    refcons(newint(chunk, -1))));
            break;
        case '>':
            emitins(blk, iadd(
                    reftmp(chunk->dptmpid),
                    refcons(newint(chunk, 1))));
            break;
        case '+': {
            int tmp = newtmp(chunk);
            emitins(blk, iload(reftmp(tmp), reftmp(chunk->dptmpid)));
            emitins(blk, iadd(reftmp(tmp), refcons(newint(chunk, 1))));
            emitins(blk, istore(reftmp(tmp), reftmp(chunk->dptmpid)));
            break;
        }
        case '-': {
            int tmp = newtmp(chunk);
            emitins(blk, iload(reftmp(tmp), reftmp(chunk->dptmpid)));
            emitins(blk, iadd(reftmp(tmp), refcons(newint(chunk, -1))));
            emitins(blk, istore(reftmp(tmp), reftmp(chunk->dptmpid)));
            break;
        }
        case '.': emitins(blk, (Ins){OP_WRITE}); break;
        case ',': emitins(blk, (Ins){OP_READ}); break;
        case '[': {
            int tmp = newtmp(chunk);
            int startlbl = newlbl(chunk);
            int endlbl = newlbl(chunk);
            // emit head
            emitins(blk, iload(reftmp(tmp), reftmp(chunk->dptmpid)));
            emitins(blk, inot(reftmp(tmp)));
            emitins(blk, icjmp(reftmp(tmp), reflbl(endlbl)));
            emitins(blk, ilabel(reflbl(startlbl)));
            // emit enclosed code
            ip = _genir(chunk, blk, src, ip + 1);
            // emit tail
            emitins(blk, iload(reftmp(tmp), reftmp(chunk->dptmpid)));
            emitins(blk, icjmp(reftmp(tmp), reflbl(startlbl)));
            emitins(blk, ilabel(reflbl(endlbl)));
            break;
        }
        case ']': return ip;
        }
    }
    return ip;
}

static void genir(Chunk *chunk, char *src) {
    int blkid = newblk(chunk);
    Block *blk = &chunk->blocks[blkid];
    blk->lbl = newlbl(chunk);
    chunk->dptmpid = newtmp(chunk);
    _genir(chunk, blk, src, 0);
}

void interpretir(char *src) {
    Chunk chunk = {0};
    genir(&chunk, src);
    printchunk(&chunk);
    runir(&chunk);
}
