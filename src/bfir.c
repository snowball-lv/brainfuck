#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/ir.h>
#include <brainfuck/brainfuck.h>

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
        case '>': {
            int val = src[ip] == '<' ? -1 : 1;
            emit(chunk, iadd(
                    reftmp(chunk->dptmpid),
                    refcons(newint(chunk, val))));
            break;
        }
        case '-':
        case '+': {
            int val = src[ip] == '-' ? -1 : 1;
            int tmp = newtmp(chunk);
            emit(chunk, iload8(reftmp(tmp), reftmp(chunk->dptmpid)));
            emit(chunk, iadd(reftmp(tmp), refcons(newint(chunk, val))));
            emit(chunk, istore8(reftmp(chunk->dptmpid), reftmp(tmp)));
            break;
        }
        case '.':
            emit(chunk, iscratch());
            emit(chunk, icall(refcons(newstr(chunk, "putchar"))));
            break;
        case ',':
            emit(chunk, iscratch());
            emit(chunk, icall(refcons(newstr(chunk, "getchar"))));
            break;
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
            chunk->curblk->outlbls[0] = endlbl;
            chunk->curblk->outlbls[1] = startlbl;
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
            chunk->curblk->outlbls[0] = startlbl;
            chunk->curblk->outlbls[1] = endlbl;
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

// reserve and zero bf data area
static void zerodata(Chunk *chunk) {
    int counter = newtmp(chunk);
    int loop = newlbl(chunk);
    int end = newlbl(chunk);
    int ptr = newtmp(chunk);
    int zero = newtmp(chunk);
    emit(chunk, ialloc(reftmp(chunk->dptmpid), refcons(newint(chunk, 30000))));
    emit(chunk, imov(reftmp(counter), refcons(newint(chunk, 30000))));
    emit(chunk, ijmp(reflbl(loop)));
    chunk->curblk->outlbls[0] = loop;
    // loop body
    pushblk(chunk);
    chunk->curblk->lbl = loop;
    emit(chunk, iadd(reftmp(counter), refcons(newint(chunk, -1))));
    emit(chunk, imov(reftmp(ptr), reftmp(chunk->dptmpid)));
    emit(chunk, iadd(reftmp(ptr), reftmp(counter)));
    emit(chunk, imov(reftmp(zero), refcons(newint(chunk, 0))));
    emit(chunk, istore8(reftmp(ptr), reftmp(zero)));
    emit(chunk, icjmp(reftmp(counter), reflbl(loop)));
    emit(chunk, ijmp(reflbl(end)));
    chunk->curblk->outlbls[0] = loop;
    chunk->curblk->outlbls[1] = end;
    // loop end
    pushblk(chunk);
    chunk->curblk->lbl = end;
}

void bftoir(Chunk *chunk, char *src) {
    pushblk(chunk);
    chunk->dptmpid = newtmp(chunk);
    zerodata(chunk);
    _genir(chunk, src, 0);
}
