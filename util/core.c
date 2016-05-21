/** @file       core.c
 *  @brief      parse and manipulate libforth core file
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015,2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 *  @todo       deal with endianess correctly
 *  @todo       add options for resizing a core file and making
 *  invalid cores valid (force, attempt to fix, etc) */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IS_BIG_ENDIAN       (!(union { uint16_t u16; unsigned char c; }){ .u16 = 1 }.c)

static void print_hex(FILE *output, const uint8_t *p, size_t len)
{
	size_t i;
	for(i = 0; i < len; i++) {
		fprintf(output, "%01x ", p[i]);
		if(i % 8 == 0 && i)
			fputc('\n', output);
	}
	if(i % 8 != 0 || !i)
		fputc('\n', output);
}

enum header { MAGIC0, MAGIC1, MAGIC2, MAGIC4, CELL_SIZE, VERSION, ENDIAN, MAGIC7 };
static const uint8_t expected_header[MAGIC7+1] = {
	0xFF, '4','T','H',    /* file magic number */
	0,                    /* size field */
	0x02,                 /* version of forth core*/
	-1,                   /* endianess of platform, either zero (big endian) or one (little endian) */
	0xFF                  /* end header */
};

enum endianess { BIG, LITTLE };

struct forth_header {
	int valid;
	int version;
	int cell_size;
	uint64_t vm_size;
	enum endianess endian;
};

static struct forth_header parsed_header = { 0, 0, 0, 0, 0 };

static uint64_t get(void *m, uint64_t offset)
{ /*get a value at an offset into vm memory*/
	assert(parsed_header.valid);
	switch(parsed_header.cell_size)
	{
		case 2:
			return *(((uint16_t*)m) + offset);
		case 4:
			return *(((uint32_t*)m) + offset);
		case 8:
			return *(((uint64_t*)m) + offset);
		default:
			fprintf(stderr, "error: invalid size\n");
			abort();
	}
}

static void prn(void *m, size_t offset)
{/*print a value at an offset into vm memory*/
	assert(parsed_header.valid);
	printf("%"PRIu64, get(m, offset));
}

static void printer(void *m, char *fmt, ...)
{
	uint64_t got;
	char f, *s;
	va_list ap;
	va_start(ap, fmt);
	while(*fmt) {
		if('%' == (f = *fmt++)) {
			switch(f = *fmt++) {
			case '\0':
				goto fmt_fail;
			case 's':
				s = va_arg(ap, char*);
				fputs(s, stdout);
				break;
			case 'd':
				got = va_arg(ap, int);
				prn(m, got);
				break;
			default:
			fmt_fail:
				fprintf(stderr, "error: invalid format\n");
				abort();
			}
		} else {
			putchar(f);
		}
	}
	va_end(ap);
}

int parse_header(FILE *core)
{
	uint8_t actual[sizeof(expected_header)] = {0};
	if(sizeof(actual) != fread(actual, 1, sizeof(actual), core)) {
		fprintf(stderr, "invalid header: too small\n");
		return -1;
	}
	if(memcmp(actual, expected_header, 4)) {
		fprintf(stderr, "invalid header: magic number does not match\n");
		fprintf(stderr, "Expected:\n");
		print_hex(stderr, expected_header, 4);
		fprintf(stderr, "Got:\n");
		print_hex(stderr, actual, 4);
		return -1;
	}
	switch(actual[CELL_SIZE]) {
		case 2:
		case 4:
		case 8:
			parsed_header.cell_size = actual[CELL_SIZE];
			break;
		default:
			fprintf(stderr, "invalid header: expected 2, 4, or 8 for size\nGot: %d\n", actual[CELL_SIZE]);
			return -1;
	}
	switch(actual[ENDIAN]) {
		case BIG:
		case LITTLE:
			parsed_header.endian = actual[ENDIAN];
			break;
		default:
			fprintf(stderr, "invalid header: expected 0 or 1 for endianess\nGot: %d\n", actual[ENDIAN]);
			return -1;
	}
	if(actual[MAGIC7] != expected_header[MAGIC7]) {
		fprintf(stderr, "invalid header: 7th magic number not 0xFF\nGot: %x\n", actual[MAGIC7]);
		return -1;
	}

	if(1 != fread(&parsed_header.vm_size, sizeof(parsed_header.vm_size), 1, core)) {
		fprintf(stderr, "invalid header: file too small, expected vm size\n");
		return -1;
	}

	parsed_header.version = actual[VERSION];
	parsed_header.valid = 1;
	return 0;
}

void print_header_info(const char *name)
{
	if(!parsed_header.valid) {
		printf("invalid header\n");
		return;
	}
	printf("core file:      %s\n", name);
	printf("version:        %d\n", parsed_header.version);
	printf("endian:         %s\n", parsed_header.endian == BIG ? "big" : "little");
	printf("cell size:      %d\n", parsed_header.cell_size);
	printf("vm size:        %"PRIu64"\n", parsed_header.vm_size);
}

/** @note each version of the data structures used must be maintained here */
/****************************** VERSION 2 *************************************/

struct v2header {
	uint8_t header[sizeof(expected_header)]; 
	uint64_t core_size;  
	void *m[];
} __attribute__((packed));

enum v2registers    { /* virtual machine registers */
	DIC    = 6,      /**< dictionary pointer */
	RSTK   = 7,      /**< return stack pointer */
	STATE  = 8,      /**< interpreter state; compile or command mode*/
	BASE   = 9,      /**< base conversion variable */
	PWD    = 10,     /**< pointer to previous word */
	SOURCE_ID = 11,  /**< input source selector */
	SIN    = 12,     /**< string input pointer*/
	SIDX   = 13,     /**< string input index*/
	SLEN   = 14,     /**< string input length*/ 
	START_ADDR = 15, /**< pointer to start of VM */
	FIN    = 16,     /**< file input pointer */
	FOUT   = 17,     /**< file output pointer */
	STDIN  = 18,     /**< file pointer to stdin */
	STDOUT = 19,     /**< file pointer to stdout */
	STDERR = 20,     /**< file pointer to stderr */
	ARGC   = 21,     /**< argument count */
	ARGV   = 22,     /**< arguments */
	DEBUG  = 23,     /**< turn debugging on/off if enabled*/
	INVALID = 24,    /**< if non zero, this interpreter is invalid */
	TOP    = 25,     /**< *stored* version of top of stack */
	INSTRUCTION = 26, /**< *stored* version of instruction pointer*/
	VSTACK_SIZE = 27, /**< size of the variable stack (special treatment compared to RSTK)*/
	START_TIME = 28, /**< start time in milliseconds */
};

int v2_print_registers(FILE *file)
{
	assert(parsed_header.valid);
	rewind(file);
	uint64_t size = sizeof(struct v2header) + parsed_header.vm_size * parsed_header.cell_size;
	struct v2header *h = calloc(size, 1); 
	if(!h) {
		fprintf(stderr, "error: could not allocate enough memory\n");
		return -1;
	}
	if(size != fread(h, 1, size, file)) {
		fprintf(stderr, "error: failed to read in entire file\n");
		return -1;
	}
	printf("Entries marked with a '*' are no longer valid\n");
	printer(h->m, "dictionary:     %d\n", DIC);
	printer(h->m, "x:              %d\n", RSTK);
	printer(h->m, "state:          %d\n", STATE);
	printer(h->m, "base:           %d\n", BASE);
	printer(h->m, "pwd:            %d\n", PWD);
	printer(h->m, "source:         %d\n", SOURCE_ID);
	printer(h->m, "string input:   %d*\n", SIN);
	printer(h->m, "string index:   %d*\n", SIDX);
	printer(h->m, "string length:  %d*\n", SLEN);
	printer(h->m, "vm start addr:  %d*\n", START_ADDR);
	printer(h->m, "file input:     %d*\n", FIN);
	printer(h->m, "file output:    %d*\n", FOUT);
	printer(h->m, "stdin:          %d*\n", STDIN);
	printer(h->m, "stdout:         %d*\n", STDOUT);
	printer(h->m, "stderr:         %d*\n", STDERR);
	printer(h->m, "argc:           %d*\n", ARGC);
	printer(h->m, "argv:           %d*\n", ARGV);
	printer(h->m, "debug:          %d\n", DEBUG);
	printer(h->m, "invalid:        %d\n", INVALID);
	printer(h->m, "top:            %d\n", TOP);
	printer(h->m, "instruction:    %d\n", INSTRUCTION);
	printer(h->m, "v stack size:   %d\n", VSTACK_SIZE);
	printer(h->m, "time:           %d*\n", START_TIME);

	free(h);
	return 0;
}

/****************************** VERSION 2 *************************************/

int main(int argc, char **argv)
{
	FILE *core;
	if(argc != 2) {
		fprintf(stderr, "manipulate libforth core files\nusage: %s forth.core\n", argv[0]);
		return -1;
	}
	if(!(core = fopen(argv[1], "rb"))) {
		fprintf(stderr, "%s:%s", argv[1], strerror(errno));
		return -1;
	}
	if(parse_header(core) < 0)
		goto fail;
	rewind(core);
	print_header_info(argv[1]);

	unsigned our_endian = !IS_BIG_ENDIAN;
	if(our_endian != parsed_header.endian)
	{
		printf("cannot parse different endianess platforms\n");
		goto cleanup;
	}

	switch(parsed_header.version)
	{ /* print more information about known forth cores */
		case 2: 
			v2_print_registers(core);
			break;
		default:
			printf("warning: unsupported forth core version\n");
			break;
	}
cleanup:
	fclose(core);
	return 0;
fail:
	fclose(core);
	return -1;
}

