
BFC = ../bin/brainfuck

SRCS = $(wildcard *.bf)
ASMS = $(SRCS:%.bf=%.asm)
OBJS = $(SRCS:%.bf=%.o)
BINS = $(SRCS:%.bf=%)

all: $(BINS)
	@if [ "$(shell ./hello)" != "Hello World!" ]; then echo "./hello failed"; fi
	@if [ "$(shell printf "123\0" | ./echo)" != "123" ]; then echo "./echo failed"; fi
	@if [ "$(shell printf "123\0" | ./rev)" != "321" ]; then echo "./rev failed"; fi

%.asm: %.bf
	$(BFC) -cir $< -o $@

%.o: %.asm
	nasm -f elf64 $< -o $@

%: %.o
	gcc -static $< -o $@

clean:
	rm -f $(BINS) $(OBJS) $(ASMS)