CC=gcc
CFLAGS=-Wall -Wextra
all: forth 
libforth.a: libforth.o
	ar rcs $@ $<
libforth.so: libforth.c libforth.h
	$(CC) $(CFLAGS) $< -c -fpic -o $@
	$(CC) -shared $< -o $@
libforth.o: libforth.c libforth.h
	$(CC) $(CFLAGS) $< -c -o $@
forth: main.c libforth.a
	$(CC) $(CFLAGS) $^ -o $@
run: forth
	./forth
clean:
	rm -f forth *.a *.so *.o *.blk *.core
