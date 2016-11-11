
all: errors.fth mini

errors.fth: errors
	./errors > $@

mini: mini.c
	${CC} ${CFLAGS} -I.. -L.. mini.c -lforth -o $@

clean:
	rm -f errors

