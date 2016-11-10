#include <stdio.h>
#include <string.h>

static const char *emsg(int error)
{
	const char *e = strerror(error);
	if(!e)
		e = "unknown error";
	return e;
}

int main(void)
{
	int i;
	puts(": 2table create allot does> swap 2* + ; \n"
		"512 2table errors\n"
		": decode errors dup 1+ @ swap @ swap ;\n");
	for(i = 0; i < 256; i++)
		printf("c\" %s\" "
			"%d errors 1+ ! "
			"%d errors !\n", 
			emsg(i),
			i, 
			i);
	return 0;
}

