MDS     := ${wildcard *.md}
DOCS    := ${MDS:%.md=%.htm}

%.htm: %.md
	markdown $< > $@

all: ${DOCS}

clean:
	rm -f ${DOCS}

