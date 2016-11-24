#include "libforth.h"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#endif

int main(int argc, char **argv)
{
#ifdef _WIN32
	/* unmess up Windows file descriptors */
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);
#endif
	return main_forth(argc, argv);
}

