#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define TenMB (10 * 1024 * 1024)

int main()
{
	void *p = NULL;
	int n = 0;
	while (1) {
		p = malloc(TenMB);
		if (p == NULL) {
			printf("malloc failed\n");
			return -1;
		}
		memset(p, 1, TenMB);
		printf("Total malloc %d TenMB success. %p \n", ++n, p);
		fflush(stdout);
		sleep(1);
	}
}
