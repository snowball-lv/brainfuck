
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
	rm -rf out bin

test: all
	# $(BIN)
	$(BIN) -i hello.bf
	printf "hello\0" | $(BIN) -i echo.bf; echo
	printf "hello\0" | $(BIN) -i rev.bf; echo
	$(BIN) hello.bf >prog.asm
	nasm -f elf64 prog.asm -o prog.o
	gcc -static prog.o -o prog
	printf "hello\0" | ./prog