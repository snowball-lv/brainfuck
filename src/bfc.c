#include <stdio.h>
#include <brainfuck/common.h>
#include <brainfuck/brainfuck.h>
#include <brainfuck/ir.h>

void interpretc(char *src) {
    int ip = 0;
    char data[30000] = {0};
    int dp = 0;
    for (; src[ip]; ip++) {
        switch (src[ip]) {
        case '>': dp++; break;
        case '<': dp--; break;
        case '+': data[dp]++; break;
        case '-': data[dp]--; break;
        case '.': bfwrite(data[dp]); break;
        case ',': data[dp] = bfread(); break;
        case '[':
            if (data[dp]) break;
            ip++;
            for (int depth = 0;; ip++) {
                if (src[ip] == ']') {
                    if (depth == 0) break;
                    depth--;
                }
                else if (src[ip] == '[') {
                    depth++;
                }
            }
            break;
        case ']':
            if (!data[dp]) break;
            ip--;
            for (int depth = 0;; ip--) {
                if (src[ip] == '[') {
                    if (depth == 0) break;
                    depth--;
                }
                else if (src[ip] == ']') {
                    depth++;
                }
            }
            break;
        default: /* do nothing */ break;
        }
    }
}
