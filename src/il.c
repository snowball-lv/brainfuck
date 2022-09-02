#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <brainfuck/common.h>
#include <brainfuck/il.h>
#include <brainfuck/ir.h>
#include <brainfuck/amd64.h>

#define TOKS(T) T(NONE) T(EOF) T(ID) T(LBRACE) T(RBRACE)

enum {
#define T(name) T_ ## name,
    TOKS(T)
#undef T
};

typedef struct {
    int type;
    char *_start;
    int len;
    char *str;
} Tok;

typedef struct {
    Task *t;
    Tok prev;
    Tok next;
    char *src;
    int line;
    int column;
} Parser;

static const char *tname(int type) {
    switch (type) {
#define T(name) case T_ ## name: return #name;
    TOKS(T)
#undef T
    }
    printf("*** unknown token type %i\n", type);
    exit(1);
}

static Tok nexttok(Parser *p) {
    char *start;
    while (isspace(*p->src))
        p->src++;
    start = p->src;
    switch (*start) {
    case 0: return (Tok){T_EOF};
    case '{': p->src++; return (Tok){T_LBRACE, start, 1};
    case '}': p->src++; return (Tok){T_RBRACE, start, 1};
    }
    if (isalpha(*start) || *start == '_') goto id;
    printf("*** unexpected char %c\n", *start);
    exit(1);
id:
    while (isalnum(*p->src) || *p->src == '_')
        p->src++;
    return (Tok){T_ID, start, p->src - start};
}

static void intern(Parser *p, Tok *t) {
    if (!t->len) return;
    t->str = malloc(t->len + 1);
    memcpy(t->str, t->_start, t->len);
    t->str[t->len] = 0;
}

static void advance(Parser *p) {
    p->prev = p->next;
    p->next = nexttok(p);
    intern(p, &p->next);
}

static int match(Parser *p, int type) {
    if (p->next.type != type) return 0;
    advance(p);
    return 1;
}

static int matchid(Parser *p, char *str) {
    if (p->next.type != T_ID) return 0;
    if (strcmp(p->next.str, str) != 0) return 0;
    advance(p);
    return 1;
}

static void expect(Parser *p, int type) {
    if (match(p, type)) return;
    printf("*** expected %s got %s:%s\n",
            tname(type), tname(p->next.type), p->next.str);
    exit(1);
}

static void parsefn(Parser *p) {
    Func *fn = newfn(p->t->m);
    expect(p, T_ID);
    fn->name = p->prev.str;
    expect(p, T_LBRACE);
    while (!match(p, T_RBRACE)) {
        advance(p);
    }
}

static void parsefile(Parser *p) {
    advance(p);
    while (!match(p, T_EOF)) {
        if (matchid(p, "function")) parsefn(p);
        else {
            printf("*** unexpected token %s:%s\n",
                    tname(p->next.type), p->next.str);
            exit(1);
        }
    }
}

void ilc(Task *t) {
    Parser p = {0};
    p.t = t;
    p.src = t->src;
    t->m = newmod();
    parsefile(&p);
    // fprintf(t->out, "bits 64\n");
    // fprintf(t->out, "global main\n");
    // fprintf(t->out, "main:\n");
    // fprintf(t->out, "mov rax, 0\n");
    // fprintf(t->out, "ret\n");
    amd64gen(t);
}
