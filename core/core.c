/** @file       core.c
 *  @brief      parse and manipulate libforth core file
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015,2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 *  @todo       Do something useful and implement this here*/
#include <errno.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
	0x01,                 /* version of forth core*/
	-1,                   /* endianess of platform, either zero (big endian) or one (little endian) */
	0xFF                  /* end header */
};

enum endianess { BIG, LITTLE };

struct forth_header {
	int valid;
	int version;
	int size;
	enum endianess endian;
};

static struct forth_header parsed_header = { 0 };

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
			parsed_header.size = actual[CELL_SIZE];
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
	printf("core file:   %s\n", name);
	printf("version:     %d\n", parsed_header.version);
	printf("endian:      %s\n", parsed_header.endian == BIG ? "big" : "little");
	printf("cell size:   %d\n", parsed_header.size);
}

/** @note each version of the data structures used must be maintained here */
/****************************** VERSION 1 *************************************/
struct v1forth;
typedef struct v1forth v1forth_t;
typedef uintptr_t v1forth_cell_t;

struct v1forth {          /**< The FORTH environment is contained in here*/
	uint8_t header[sizeof(expected_header)]; /**< header for when writing object to disk */
	size_t core_size;   /**< size of VM */
	jmp_buf *error;      /**< place to jump to on error */
	uint8_t *s;         /**< convenience pointer for string input buffer */
	clock_t start_time; /**< used for "clock", start of initialization */
	v1forth_cell_t *S;    /**< stack pointer */
	v1forth_cell_t m[];   /**< Forth Virtual Machine memory */
} /*__attribute__((packed)); // not always portable */;

enum v1registers    { /* virtual machine registers */
	v1DIC    = 6,      /**< dictionary pointer */
	v1RSTK   = 7,      /**< return stack pointer */
	v1STATE  = 8,      /**< interpreter state; compile or command mode*/
	v1BASE   = 9,      /**< base conversion variable */
	v1PWD    = 10,     /**< pointer to previous word */
	v1SOURCE_ID = 11,  /**< input source selector */
	v1SIN    = 12,     /**< string input pointer*/
	v1SIDX   = 13,     /**< string input index*/
	v1SLEN   = 14,     /**< string input length*/ 
	v1START_ADDR = 15, /**< pointer to start of VM */
	v1FIN    = 16,     /**< file input pointer */
	v1FOUT   = 17,     /**< file output pointer */
	v1STDIN  = 18,     /**< file pointer to stdin */
	v1STDOUT = 19,     /**< file pointer to stdout */
	v1STDERR = 20,     /**< file pointer to stderr */
	v1ARGC   = 21,     /**< argument count */
	v1ARGV   = 22,     /**< arguments */
	v1DEBUG  = 23,     /**< turn debugging on/off if enabled*/
	v1INVALID = 24,    /**< if non zero, this interpreter is invalid */
	v1TOP    = 25,     /**< *stored* version of top of stack */
	v1INSTRUCTION = 26, /**< *stored* version of instruction pointer*/
	v1VSTACK_SIZE = 27  /**< size of the variable stack (special treatment compared to RSTK)*/
};

/****************************** VERSION 1 *************************************/

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
	parse_header(core);
	print_header_info(argv[1]);
		
	fclose(core);
	return 0;
}
