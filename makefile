
BIN = bin/brainfuck
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=out/%.o)
DEPS = $(SRCS:src/%.c=out/%.d)

CFLAGS = -g -c -MMD -I inc -Wall

ASRCS = $(wildcard src/*.asm)
AOBJS = $(ASRCS:src/%.asm=out/%.o)

all: $(BIN)

-include $(DEPS)

out:
	mkdir out

out/%.o: src/%.c | out
	$(CC) $(CFLAGS) $< -o $@

out/%.o: src/%.asm | out
	nasm -g -f elf64 $< -o $@

bin:
	mkdir bin

$(BIN): $(OBJS) $(AOBJS) | bin
	$(CC) $^ -o $@

clean:
	$(MAKE) -s -C bftest clean
	rm -rf out bin

.PHONY: bftest
bftest:
	$(MAKE) -s -C bftest

test: all bftest
	# $(BIN) -cil main.il -o prog.asm