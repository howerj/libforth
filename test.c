#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define OUT_LEN (256)

static const char *dev = "/dev/forth";
static const char *prg = "2 2 + . cr";
static char out[OUT_LEN] = { 0 };

static const char *emsg(void)
{
	const char *msg = "unknown error";
	if(errno) {
		const char *m = strerror(errno);
		msg = m ? m : msg;
	}
	return msg;
}

void hex(const char *s, size_t length)
{
	size_t i;
	if(!length) {
		fprintf(stdout, "(empty)\n");
		return;
	}
	for(i = 0; i < length; i++)
		fprintf(stdout, "%03x ", s[i]);
	fputc('\n', stdout);
}

int main(void)
{
	int fd = -1, ret = 0;

	errno = 0;
	printf("opening: %s\n", dev);
	fd = open("/dev/forth", O_RDWR);
	if(fd < 0) {
		fprintf(stderr, "%s: %s\n", dev, emsg());
		return ret;
	}

	printf("writing to %s with '%s'\n", dev, prg);
	errno = 0;
	ret = write(fd, prg, strlen(prg)+1);
	if(ret < 0) {
		fprintf(stderr, "write: %s\n", emsg());
		return ret;
	}

	printf("reading results back\n");
	errno = 0;
	ret = read(fd, out, OUT_LEN);
	if(ret < 0) {
		fprintf(stderr, "read: %s\n", emsg());
		return ret;
	}

	printf("results:\n");
	printf("hex dump\n");
	hex(out, (size_t)ret);
	printf("return string:\n%s\n", out);

	printf("closing: %s\n", dev);
	errno = 0;
	ret = close(fd);
	if(ret < 0) {
		fprintf(stderr, "close: %s\n", emsg());
		return ret;
	}

	return 0;
}

