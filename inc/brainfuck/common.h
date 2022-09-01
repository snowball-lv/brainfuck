#pragma once

typedef struct Mod Mod;

typedef struct {
    char *infile;
    char *outfile;
    char *src;
    FILE *out;
    Mod *m;
} Task;
