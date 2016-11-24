CFLAGS= -Wall -Wextra -std=c99 -g
TARGETS= errors.fth mini core2c core pl0
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

core.gen.c: core2c ../forth.core
	./core2c < ../forth.core > $@

core: core.c core.gen.c ../libforth.a
	${CC} ${CFLAGS} -I.. -L.. $^ -lforth -o $@

clean:
	rm -f ${TARGETS}
	make -C ../ clean

