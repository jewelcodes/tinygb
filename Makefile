CC=gcc
LD=gcc
ARCH := $(shell $(CC) -dumpmachine | grep -q x86_64 && echo x86_64)

CFLAGS=-c -Wall -Ofast $(shell sdl2-config --cflags) -I./src/include
LDFLAGS=-Ofast $(shell sdl2-config --libs)

ifeq ($(ARCH),x86_64)
	CFLAGS += -msse2
	LDFLAGS += -msse2
endif

SRC:=$(shell find ./src -type f -name "*.c")
OBJ:=$(SRC:.c=.o)

all: tinygb

clean:
	@rm -f $(OBJ)
	@rm -f tinygb

%.o: %.c
	@exec echo -e "\x1B[0;1;35m [ CC ]\x1B[0m $@"
	@$(CC) -o $@ $< ${CFLAGS}

tinygb: $(OBJ)
	@exec echo -e "\x1B[0;1;36m [ LD ]\x1B[0m tinygb"
	@$(LD) $(OBJ) -o tinygb ${LDFLAGS}
