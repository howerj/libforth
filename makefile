CC=gcc
CFLAGS=-Wall -Wextra
all: forth
forth.o: forth.c forth.h
	$(CC) $(CFLAGS) $< -c -o $@
forth: main.c forth.o
	$(CC) $(CFLAGS) $^ -o $@
run: forth forth.4th
	./forth
clean:
	rm -f forth *.o *.blk
