#pragma once

void bfwrite(int c);
int bfread();

void interpretc(char *src);
void interpretasm(char *src);

void gennasm(Task *t);
void genirnasm(Task *t);

void bftoir(Task *t);

