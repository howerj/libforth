ECHO	= echo
AR	= ar
CC	= gcc
CFLAGS	= -Wall -Wextra -g -pedantic -std=c99 -O2
TARGET	= forth
RM      = rm -rf
CTAGS  ?= ctags
COLOR   = 

FORTH_FILE = forth.fth

.PHONY: all shorthelp doc clean test 

all: shorthelp ${TARGET}

shorthelp:
	@${ECHO} "Use 'make help' for a list of all options"
help:
	@${ECHO} ""
	@${ECHO} "project:      lib${TARGET}"
	@${ECHO} "description: A small ${TARGET} interpreter and library"
	@${ECHO} ""
	@${ECHO} "make {option}*"
	@${ECHO} ""
	@${ECHO} "      all             create the ${TARGET} libraries and executables"
	@${ECHO} "      ${TARGET}           create the ${TARGET} executable"
	@${ECHO} "      unit            create the unit test executable"
	@${ECHO} "      test            execute the unit tests"
	@${ECHO} "      doc             make the project documentation"
	@${ECHO} "      lib${TARGET}.a      make a static ${TARGET} library"
	@${ECHO} "      clean           remove generated files"
	@${ECHO} "      dist            create a distribution archive"
	@${ECHO} ""

%.o: %.c *.h
	@echo "cc $< -c -o $@"
	@${CC} ${CFLAGS} $< -c -o $@

doc: readme.htm

%.htm: %.md
	markdown $^ > $@

lib${TARGET}.a: lib${TARGET}.o
	${AR} rcs $@ $<

unit: unit.o lib${TARGET}.a

${TARGET}: main.o lib${TARGET}.a
	@echo "cc $^ -o $@"
	@${CC} ${CFLAGS} $^ -o $@

forth.core: ${TARGET} ${FORTH_FILE} test
	./${TARGET} -s $@ ${FORTH_FILE}

run: ${TARGET} ${FORTH_FILE}
	./$< -t ${FORTH_FILE}

test: unit
	./$^ ${COLOR}

tags: lib${TARGET}.c lib${TARGET}.h unit.c main.c
	${CTAGS} $^

dist: ${TARGET} ${TARGET}.1 lib${TARGET}.[a3] readme.htm forth.core
	tar zvcf ${TARGET}.tgz $^

clean:
	${RM} ${TARGET} unit *.blk *.core *.a *.so *.o *.log *.htm *.tgz tags

