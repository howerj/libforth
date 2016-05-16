RM=rm -f
CFLAGS=-Wall -Wextra -std=c99 -O2
TARGETS=hexdump core rle
all: hexdump core rle

.PHONY: hexdump.dec rle.dec forth.dec

test: rle
	./unit

clean:
	${RM} ${TARGETS} *.enc *.dec *.bin *.o
