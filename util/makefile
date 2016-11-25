CFLAGS= -Wall -Wextra -std=c99 -g
TARGETS= errors.fth core2c pl0
.PHONY: ../forth.core ../libforth.a
all: ${TARGETS}

errors.fth: errors
	./errors > $@

mini: mini.c ../libforth.a
	${CC} ${CFLAGS} -I.. -L.. $^ -lforth -o $@

core2c: core2c.c ../libforth.a
	${CC} ${CFLAGS} -I.. $^ -o $@

../forth.core:
	make -C .. forth.core

../libforth.a:
	make -C .. libforth.a

clean:
	rm -f ${TARGETS}
	make -C ../ clean

