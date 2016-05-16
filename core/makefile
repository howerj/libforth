RM=rm -f
CFLAGS=-Wall -Wextra -std=c99 -O2
TARGETS=hexdump core rle
all: hexdump core rle

.PHONY: hexdump.dec rle.dec forth.dec

hexdump.dec:
	./rle -e hexdump.c hexdump.enc
	./rle -d hexdump.enc hexdump.dec
	head hexdump.dec
	diff hexdump.c hexdump.dec

rle.dec:
	./rle -e rle.c rle.enc
	./rle -d rle.enc rle.dec
	head rle.dec
	diff rle.c rle.dec

forth.dec:
	./rle -e ../forth.core forth.enc
	./rle -d forth.enc forth.dec
	wc ../forth.core forth.dec
	cmp -l ../forth.core forth.dec

test: rle hexdump.dec rle.dec forth.dec


clean:
	${RM} ${TARGETS} *.enc *.dec *.o
