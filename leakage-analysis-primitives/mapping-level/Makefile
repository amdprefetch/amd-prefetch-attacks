CFLAGS ?= -Os

all: profile

profile: main.c
	@gcc ${CFLAGS} main.c -o profile -lm

clean:
	@rm -rf profile
