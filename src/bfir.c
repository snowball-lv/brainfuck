#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <brainfuck/common.h>
#include <brainfuck/brainfuck.h>
#include <brainfuck/ir.h>

typedef struct {
    Task *t;
    Func *fn;
    int dptmp;
    Block *curblk;
} Context;

static void pushblk(Context *ctx, Func *fn) {
    ctx->curblk = newblk(fn);
    ctx->curblk->lbl = newlbl(fn);
}

static void emit(Context *ctx, Ins ins) {
    emitins(ctx->curblk, ins);
}

static int _genir(Context *ctx, Func *fn, char *src, int ip) {
    for (; src[ip]; ip++) {
        switch (src[ip]) {
        case '<':
        case '>': {
            int val = src[ip] == '<' ? -1 : 1;
            emit(ctx, iadd(
                    reftmp(ctx->dptmp),
                    refcons(newint(fn, val))));
            break;
        }
        case '-':
        case '+': {
            int val = src[ip] == '-' ? -1 : 1;
            int tmp = newtmp(fn);
            emit(ctx, iload8(reftmp(tmp), reftmp(ctx->dptmp)));
            emit(ctx, iadd(reftmp(tmp), refcons(newint(fn, val))));
            emit(ctx, istore8(reftmp(ctx->dptmp), reftmp(tmp)));
            break;
        }
        case '.': {
            int tmp = newtmp(fn);
            emit(ctx, iload8(reftmp(tmp), reftmp(ctx->dptmp)));
            emit(ctx, iscratch());
            emit(ctx, iarg(0, reftmp(tmp)));
            emit(ctx, icall(reftmp(newtmp(fn)), refcons(newstr(fn, "putchar"))));
            break;
        }
        case ',': {
            emit(ctx, iscratch());
            int tmp = newtmp(fn);
            emit(ctx, icall(reftmp(tmp), refcons(newstr(fn, "getchar"))));
            emit(ctx, istore8(reftmp(ctx->dptmp), reftmp(tmp)));
            break;
        }
        case '[': {
            int tmp = newtmp(fn);
            int startlbl = newlbl(fn);
            int endlbl = newlbl(fn);
            // emit head
            emit(ctx, iload8(reftmp(tmp), reftmp(ctx->dptmp)));
            emit(ctx, inot(reftmp(tmp)));
            emit(ctx, icjmp(reftmp(tmp), reflbl(endlbl)));
            // finish the block with a jump to the next one
            emit(ctx, ijmp(reflbl(startlbl)));
            ctx->curblk->outlbls[0] = endlbl;
            ctx->curblk->outlbls[1] = startlbl;
            // create new block
            pushblk(ctx, fn);
            ctx->curblk->lbl = startlbl;
            // emit enclosed code
            ip = _genir(ctx, fn, src, ip + 1);
            // emit tail
            emit(ctx, iload8(reftmp(tmp), reftmp(ctx->dptmp)));
            emit(ctx, icjmp(reftmp(tmp), reflbl(startlbl)));
            // finish the block with a jump to the next one
            emit(ctx, ijmp(reflbl(endlbl)));
            ctx->curblk->outlbls[0] = startlbl;
            ctx->curblk->outlbls[1] = endlbl;
            // create new block
            pushblk(ctx, fn);
            ctx->curblk->lbl = endlbl;
            break;
        }
        case ']': return ip;
        }
    }
    return ip;
}

// reserve and zero bf data area
static void zerodata(Context *ctx, Func *fn) {
    int counter = newtmp(fn);
    int loop = newlbl(fn);
    int end = newlbl(fn);
    int ptr = newtmp(fn);
    int zero = newtmp(fn);
    emit(ctx, ialloc(reftmp(ctx->dptmp), refcons(newint(fn, 30000))));
    emit(ctx, imov(reftmp(counter), refcons(newint(fn, 30000))));
    emit(ctx, ijmp(reflbl(loop)));
    ctx->curblk->outlbls[0] = loop;
    // loop body
    pushblk(ctx, fn);
    ctx->curblk->lbl = loop;
    emit(ctx, iadd(reftmp(counter), refcons(newint(fn, -1))));
    emit(ctx, imov(reftmp(ptr), reftmp(ctx->dptmp)));
    emit(ctx, iadd(reftmp(ptr), reftmp(counter)));
    emit(ctx, imov(reftmp(zero), refcons(newint(fn, 0))));
    emit(ctx, istore8(reftmp(ptr), reftmp(zero)));
    emit(ctx, icjmp(reftmp(counter), reflbl(loop)));
    emit(ctx, ijmp(reflbl(end)));
    ctx->curblk->outlbls[0] = loop;
    ctx->curblk->outlbls[1] = end;
    // loop end
    pushblk(ctx, fn);
    ctx->curblk->lbl = end;
}

void bftoir(Task *t) {
    Context ctx;
    ctx.t = t;
    ctx.fn = newfn(t->m);
    ctx.dptmp = newtmp(ctx.fn);
    pushblk(&ctx, ctx.fn);
    zerodata(&ctx, ctx.fn);
    _genir(&ctx, ctx.fn, t->src, 0);
}
