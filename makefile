ECHO	= echo
AR	= ar
CC	= gcc
CFLAGS	= -Wall -Wextra -g -pedantic -std=c99 -O2 
LDFLAGS = 
INCLUDE = libline
TARGET	= forth
RM      = rm -rf
CTAGS  ?= ctags
CP      = cp

MDS     := ${wildcard *.md}
DOCS    := ${MDS:%.md=%.htm}

FORTH_FILE = forth.fth

.PHONY: all shorthelp doc clean test profile unit.test forth.test line small fast static

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
	@${ECHO} "      line            make ${TARGET} with line editor"
	@${ECHO} "      clean           remove generated files"
	@${ECHO} "      dist            create a distribution archive"
	@${ECHO} "      profile         generate lots of profiling information"
	@${ECHO} ""

%.o: %.c *.h
	@echo "cc $< -c -o $@"
	@${CC} ${CFLAGS} $< -c -o $@

lib${TARGET}.a: lib${TARGET}.o
	${AR} rcs $@ $<

${TARGET}: main.o unit.o lib${TARGET}.a
	@echo "cc $^ -o $@"
	@${CC} ${CFLAGS} $^ ${LDFLAGS} -o $@

forth.core: ${TARGET} ${FORTH_FILE} test
	./${TARGET} -s $@ ${FORTH_FILE}

forth.dump: forth.core ${TARGET}
	./${TARGET} -l $< -e "0 here dump" > $@

run: ${TARGET} ${FORTH_FILE}
	./$< -t ${FORTH_FILE}

util/core2c:
	make -C util/ core2c

core.gen.c: forth.core util/core2c
	./util/core2c < $< > $@

lib${TARGET}: main.c unit.o core.gen.c lib${TARGET}.a
	${CC} ${CFLAGS} -I. -DUSE_BUILT_IN_CORE $^ -o $@

# "unit" contains the unit tests against the C API
unit.test: ${TARGET}
	./$< -u

# A side effect of failing the tests in "unit.fth" is the fact that saving to
# "forth.core" will fail, making this test fail.
forth.test: forth unit.test forth.fth unit.fth
	./$< -s forth_test.core forth.fth unit.fth
	@${RM} forth_test.core

test: unit.test forth.test

tags: lib${TARGET}.c lib${TARGET}.h unit.c main.c
	${CTAGS} $^

dist: ${TARGET} ${TARGET}.1 lib${TARGET}.[a3] lib${TARGET}.htm ${DOCS} forth.core
	tar zvcf ${TARGET}.tgz $^

%.htm: %.md
	markdown $< > $@

libforth.md: main.c libforth.c libforth.h
	./util/./convert    libforth.c    >  $@
	echo "## Appendix "               >> $@
	echo "### libforth header"        >> $@
	./util/./convert -H libforth.h    >> $@
	echo "### main.c example program" >> $@
	./util/./convert main.c           >> $@

%.pdf: %.md
	pandoc --toc $< -o $@

%.1: %.md
	pandoc -s -t man $< -o $@

%.md: %.c util/convert
	util/./convert $< > $@

%.md: %.h util/convert
	util/./convert -H $< -o $@

%.3: %.h
	util/./convert -H $< | pandoc -f markdown -s -t man -o $@

${TARGET}.1: readme.1
	${CP} $^ $@

doc: lib${TARGET}.htm ${DOCS}

doxygen: *.c *.h *.md
	doxygen -g
	doxygen 2> doxygen_warnings.log

small: CFLAGS = -m32 -DNDEBUG -std=c99 -Os 
small: ${TARGET}
	strip ${TARGET}

fast: CFLAGS = -DNDEBUG -O3 -std=c99
fast: ${TARGET}

static: CC=musl-gcc
static: ${TARGET}

# CFLAGS: Add "-save-temps" to keep temporary files around
# objdump: Add "-M intel" for a more sensible assembly output
profile: CFLAGS += -pg -g -O2 -DNDEBUG -fprofile-arcs -ftest-coverage 
profile: clean ${TARGET}
	./${TARGET} forth.fth unit.fth > testrun.log
	gprof ${TARGET} gmon.out > profile.log
	gcov lib${TARGET}.c
	objdump -d -S lib${TARGET}.o > libforth.s

clean:
	${RM} ${TARGET} unit *.a *.so *.o
	${RM} *.log *.htm *.tgz *.pdf
	${RM} *.blk *.core *.dump
	${RM} tags
	${RM} *.i *.s *.gcov *.gcda *.gcno *.out
	${RM} html latex Doxyfile *.db *.bak
	${RM} libforth.md

