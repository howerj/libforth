CFLAGS= -Wall -Wextra -std=c99 -g
TARGETS= errors.fth mini core2c core core2

all: ${TARGETS}

errors.fth: errors
	./errors > $@

mini: mini.c
	${CC} ${CFLAGS} -I.. -L.. $^ -lforth -o $@

core2c: core2c.c
	${CC} ${CFLAGS} -I.. $^ -o $@

../forth.core:
	make -C .. forth.core

core.gen.c: core2c ../forth.core
	./core2c < ../forth.core > $@

core: core.c core.gen.c
	${CC} ${CFLAGS} -I.. -L.. $^ -lforth -o $@

# use -O elf32-i386 and -B i386 for 32 bit targets
# objcopy -i <-- lists possible targets
REDEFINES =  --redefine-sym _binary____forth_core_start=forth_core_data
REDEFINES += --redefine-sym _binary____forth_core_size=forth_core_size
forthcore.o: ../forth.core
	objcopy -I binary -O elf64-x86-64 -B i386 ${REDEFINES} $^ $@
	objdump -t $@

core2: core.c forthcore.o
	${CC} ${CFLAGS} -DUSE_OBJDUMP -I.. -L.. $^ -lforth -o $@

clean:
	rm -f ${TARGETS}

