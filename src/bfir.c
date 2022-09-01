#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/common.h>
#include <brainfuck/brainfuck.h>
#include <brainfuck/ir.h>

static void pushblk(Func *fn) {
    int blkid = newblk(fn);
    fn->curblk = &fn->blocks[blkid];
    fn->curblk->lbl = newlbl(fn);
}

static void emit(Func *fn, Ins ins) {
    emitins(fn->curblk, ins);
}

static int _genir(Func *fn, char *src, int ip) {
    for (; src[ip]; ip++) {
        switch (src[ip]) {
        case '<':
        case '>': {
            int val = src[ip] == '<' ? -1 : 1;
            emit(fn, iadd(
                    reftmp(fn->dptmpid),
                    refcons(newint(fn, val))));
            break;
        }
        case '-':
        case '+': {
            int val = src[ip] == '-' ? -1 : 1;
            int tmp = newtmp(fn);
            emit(fn, iload8(reftmp(tmp), reftmp(fn->dptmpid)));
            emit(fn, iadd(reftmp(tmp), refcons(newint(fn, val))));
            emit(fn, istore8(reftmp(fn->dptmpid), reftmp(tmp)));
            break;
        }
        case '.': {
            int tmp = newtmp(fn);
            emit(fn, iload8(reftmp(tmp), reftmp(fn->dptmpid)));
            emit(fn, iscratch());
            emit(fn, iarg(0, reftmp(tmp)));
            emit(fn, icall(reftmp(newtmp(fn)), refcons(newstr(fn, "putchar"))));
            break;
        }
        case ',': {
            emit(fn, iscratch());
            int tmp = newtmp(fn);
            emit(fn, icall(reftmp(tmp), refcons(newstr(fn, "getchar"))));
            emit(fn, istore8(reftmp(fn->dptmpid), reftmp(tmp)));
            break;
        }
        case '[': {
            int tmp = newtmp(fn);
            int startlbl = newlbl(fn);
            int endlbl = newlbl(fn);
            // emit head
            emit(fn, iload8(reftmp(tmp), reftmp(fn->dptmpid)));
            emit(fn, inot(reftmp(tmp)));
            emit(fn, icjmp(reftmp(tmp), reflbl(endlbl)));
            // finish the block with a jump to the next one
            emit(fn, ijmp(reflbl(startlbl)));
            fn->curblk->outlbls[0] = endlbl;
            fn->curblk->outlbls[1] = startlbl;
            // create new block
            pushblk(fn);
            fn->curblk->lbl = startlbl;
            // emit enclosed code
            ip = _genir(fn, src, ip + 1);
            // emit tail
            emit(fn, iload8(reftmp(tmp), reftmp(fn->dptmpid)));
            emit(fn, icjmp(reftmp(tmp), reflbl(startlbl)));
            // finish the block with a jump to the next one
            emit(fn, ijmp(reflbl(endlbl)));
            fn->curblk->outlbls[0] = startlbl;
            fn->curblk->outlbls[1] = endlbl;
            // create new block
            pushblk(fn);
            fn->curblk->lbl = endlbl;
            break;
        }
        case ']': return ip;
        }
    }
    return ip;
}

// reserve and zero bf data area
static void zerodata(Func *fn) {
    int counter = newtmp(fn);
    int loop = newlbl(fn);
    int end = newlbl(fn);
    int ptr = newtmp(fn);
    int zero = newtmp(fn);
    emit(fn, ialloc(reftmp(fn->dptmpid), refcons(newint(fn, 30000))));
    emit(fn, imov(reftmp(counter), refcons(newint(fn, 30000))));
    emit(fn, ijmp(reflbl(loop)));
    fn->curblk->outlbls[0] = loop;
    // loop body
    pushblk(fn);
    fn->curblk->lbl = loop;
    emit(fn, iadd(reftmp(counter), refcons(newint(fn, -1))));
    emit(fn, imov(reftmp(ptr), reftmp(fn->dptmpid)));
    emit(fn, iadd(reftmp(ptr), reftmp(counter)));
    emit(fn, imov(reftmp(zero), refcons(newint(fn, 0))));
    emit(fn, istore8(reftmp(ptr), reftmp(zero)));
    emit(fn, icjmp(reftmp(counter), reflbl(loop)));
    emit(fn, ijmp(reflbl(end)));
    fn->curblk->outlbls[0] = loop;
    fn->curblk->outlbls[1] = end;
    // loop end
    pushblk(fn);
    fn->curblk->lbl = end;
}

void bftoir(Task *t) {
    Func *fn = newfn(t->m);
    pushblk(fn);
    fn->dptmpid = newtmp(fn);
    zerodata(fn);
    _genir(fn, t->src, 0);
}
