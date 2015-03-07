CC=gcc
CFLAGS=-Wall -Wextra
all: forth
doc: forth.htm
libforth.a: libforth.o
	ar rcs $@ $<
libforth.so: libforth.c libforth.h
	$(CC) $(CFLAGS) $< -c -fpic -o $@
	$(CC) -shared $< -o $@
libforth.o: libforth.c libforth.h
	$(CC) $(CFLAGS) $< -c -o $@
forth: main.c libforth.a
	$(CC) $(CFLAGS) $^ -o $@
forth.htm: forth.md
	rm -f $@
	markdown $^ > $@
run: forth
	./$^
clean:
	rm -f forth *.a *.so *.o *.blk *.core
