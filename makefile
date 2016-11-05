ECHO	= echo
AR	= ar
CC	= gcc
CFLAGS	= -Wall -Wextra -g -pedantic -std=c99 -O2 
TARGET	= forth
RM      = rm -rf
CTAGS  ?= ctags
CP      = cp
COLOR   = 

MDS     := ${wildcard *.md}
DOCS    := ${MDS:%.md=%.htm}

.PHONY: all shorthelp doc clean 

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
	@${ECHO} "      doc             make the project documentation"
	@${ECHO} "      clean           remove generated files"
	@${ECHO} "      dist            create a distribution archive"
	@${ECHO} ""

%.o: %.c *.h
	@echo "cc $< -c -o $@"
	@${CC} ${CFLAGS} $< -c -o $@

${TARGET}: ${TARGET}.o
	@echo "cc $^ -o $@"
	@${CC} ${CFLAGS} $^  -o $@

run: ${TARGET}
	./$< 

dist: ${TARGET} ${DOCS} forth.core
	tar zvcf ${TARGET}.tgz $^

clean:
	${RM} ${TARGET} unit *.a *.so *.o
	${RM} *.log *.htm *.tgz *.pdf
	${RM} *.blk *.core *.dump
	${RM} tags
	${RM} *.i *.s *.gcov *.gcda *.gcno *.out
	${RM} html latex Doxyfile *.db *.bak
	${RM} libforth.md

