#include <unistd.h>
#include <stdio.h>

int main()
{
	int n = 0;
	while (1) {
		sleep(1);
		printf("while_loop %d\n", ++n);
		fflush(stdout);
	}
	return 0;
}
