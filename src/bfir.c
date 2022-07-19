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
