RM=rm -f
CFLAGS=-Wall -Wextra -std=c99 -O2
TARGETS=hexdump core rle mkblk
all: ${TARGETS}

.PHONY: hexdump.dec rle.dec forth.dec

%: %.c
	@echo "$(CC) $< -o $@"
	@$(CC) $(CFLAGS) $< -o $@

test: rle rle.unit
	./rle.unit

clean:
	${RM} ${TARGETS} *.enc *.dec *.bin *.o *.blk
