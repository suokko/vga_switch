/*
Copyright (c) 2013 Pauli Nieminen <suokkos@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

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
