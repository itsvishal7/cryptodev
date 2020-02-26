#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


int main() {
	int fd = open("/dev/cryptocard_mod",O_RDWR);
	char *name = "AAAA";
	printf(" %c %lx\n", name[0], (unsigned long) &name);
	printf("%d\n", fd);

	write(fd, name, 4);	

	printf("uuuu: %s, %d\n", name, close(fd));
	return 0;
}
