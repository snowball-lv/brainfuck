#pragma once

void bfwrite(int c);
int bfread();

void interpretc(char *src);
void interpretasm(char *src);
void gennasm(char *src);
void genirnasm(char *src);

void bftoir(Chunk *chunk, char *src);

