ECHO	= echo
AR	= ar
CC	= gcc
CFLAGS	= -Wall -Wextra -g -pedantic -std=c99 -O2 
TARGET	= forth
RM      = rm -rf
CTAGS  ?= ctags
COLOR   = 

FORTH_FILE = forth.fth

.PHONY: all shorthelp doc clean test profile

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
	@${ECHO} "      profile         generate lots of profiling information"
	@${ECHO} ""

%.o: %.c *.h
	@echo "cc $< -c -o $@"
	@${CC} ${CFLAGS} $< -c -o $@

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

%.htm: %.md
	markdown $^ > $@

doc: readme.htm

doxygen: *.c *.h *.md
	doxygen -g
	doxygen 2> doxygen_warnings.log

# CFLAGS: Add "-save-temps" to keep temporary files around
# objdump: Add "-M intel" for a more sensible assembly output
profile: CFLAGS += -pg -g -O2 -DNDEBUG -fprofile-arcs -ftest-coverage 
profile: ${TARGET}
	./$< forth.fth unit.fth
	gprof $< gmon.out > profile.log
	gcov lib${TARGET}.c
	objdump -d -S lib${TARGET}.o > libforth.s

clean:
	${RM} ${TARGET} unit *.a *.so *.o
	${RM} *.log *.htm *.tgz 
	${RM} *.blk *.core
	${RM} tags
	${RM} *.i *.s *.gcov *.gcda *.gcno *.out
	${RM} html latex Doxyfile *.db *.bak

