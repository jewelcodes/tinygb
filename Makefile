
CFLAGS=-c -Wall -O2 -I/usr/include/SDL2 -I./src/include
LDFLAGS=-O2 -lSDL2 
CC=gcc
LD=gcc
SRC:=$(shell find ./src -type f -name "*.c")
OBJ:=$(SRC:.c=.o)

all: tinygb

clean:
	@rm -f $(OBJ)
	@rm -f tinygb

%.o: %.c
	@exec echo -e "\x1B[0;1;35m [ CC ]\x1B[0m $@"
	@$(CC) $(CFLAGS) -o $@ $<

tinygb: $(OBJ)
	@exec echo -e "\x1B[0;1;36m [ LD ]\x1B[0m tinygb"
	@$(LD) $(LDFLAGS) $(OBJ) -o tinygb
