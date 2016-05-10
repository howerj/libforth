/** @file       core.c
 *  @brief      parse and manipulate libforth core file
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015,2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 *  @todo       Do something useful and implement this here*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

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
		
	fclose(core);
	return 0;
}
