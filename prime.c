#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int nr, char * const *args)
{
	if (nr < 3) {
		printf("You need to pass prime card number and then command to run\n");
		return 1;
	}

	int card = atoi(args[1]);

	if (card > 0) {
		system("/sbin/vga_switch -g start");
	}

	pid_t pid = fork();
	
	if (pid < 0) {
		perror("fork:");
		return 2;
	}

	if (pid) {
		/* parent */
		int status;
		while (pid != waitpid(pid, &status, 0) && errno == EINTR)
		{}
	} else {
		/* child */
		setenv("DRI_PRIME", args[1], 1);

		execvp(args[2], &args[2]);
	}
	
	if (card > 0) {
		system("/sbin/vga_switch -g stop");
	}

	return 0;
}
