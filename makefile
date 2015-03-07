CC=gcc
CFLAGS=-Wall -Wextra
all: forth
doc: forth.htm doxygen
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
doxygen: doxygen.conf *.c *.h
	rm -rf doxygen
	mkdir doxygen
	doxygen doxygen.conf
run: forth
	./$^
clean:
	rm -rf forth *.a *.so *.o *.blk *.core *.log doc/htm
