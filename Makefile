obj-m    += forth.o
#ccflags-y  = -std=gnu99

all: forth.ko test

test: test.c
	${CC} -Wall -Wextra -o $@ $<

forth.ko: forth.c
	make -C /lib/modules/${shell uname -r}/build/ M=${PWD} modules

clean:
	make -C /lib/modules/${shell uname -r}/build/ M=${PWD} clean
	rm -f test
