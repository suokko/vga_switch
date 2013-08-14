#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <stdlib.h>
#include <libgen.h>

static const char *SWITCH_PATH = "/sys/kernel/debug/vgaswitcheroo/switch";
static const char *ON  = "ON";
static const char *OFF = "OFF";

void handler(int sig)
{
	syslog(LOG_USER | LOG_ERR, "signal %d", sig);
}

static void killprevious()
{
	static const char *bin = "vga_switch";
	DIR *r = opendir("/proc");
	pid_t pid = getpid();
	char pidtext[100];
	sprintf(pidtext, "%d", pid);

	if (!r) {
		syslog(LOG_USER | LOG_ERR, "Can't open proc");
		exit(errno);
	}

	struct dirent *ent;
	char path[1024*64];
	while ((ent = readdir(r))) {
		char *n = ent->d_name;
		int only_numbers = 1;
		if (strcmp(ent->d_name, pidtext) == 0)
			continue;
		while (*n != '\0') {
			if (*n > '9' || *n < '0') {
				only_numbers = 0;
				break;
			}
			n++;
		}
		if (only_numbers == 0)
			continue;
		sprintf(path, "/proc/%s/exe", ent->d_name);
		ssize_t s;
		if ((s = readlink(path, path, 1024*64)) == -1)
			continue;
		path[s] = '\0';
		char *base = basename(path);

		if (strcmp(base, bin) == 0) {
			syslog(LOG_USER | LOG_INFO, "killing %s", ent->d_name);
			kill(atoi(ent->d_name), SIGKILL);
		}
	}

	closedir(r);
}

int main(int argc, char **argv)
{
	FILE *f = fopen(SWITCH_PATH, "w");
	int r;
	if (argc < 2 || !(strcmp("post", argv[1]) == 0 ||
				strcmp("pre", argv[1]) == 0)) {
		syslog(LOG_USER | LOG_ERR, "invalid parameters %d", argc);
		fprintf(stderr, "invalid parameters %d\n", argc);
		return -1;
	}
	if (!f) {
		r = errno;
		perror("fopen: ");
		return r;
	}

	r = daemon(0,0);
	if (r < 0) {
		r = errno;
		perror("daemon: ");
		return r;
	}

	for (r = SIGHUP; r < SIGSYS; r++)
		signal(r, handler);

	killprevious();
	
	if (strcmp("pre", argv[1]) == 0) {
		syslog(LOG_USER | LOG_INFO, "switch writing %s", ON);
		if (fwrite(ON, 1, 2, f) != 2)
			syslog(LOG_USER | LOG_ERR, "switch %s failed %d", ON, errno);
		syslog(LOG_USER | LOG_INFO, "switch written %s", ON);
	} else {
		syslog(LOG_USER | LOG_INFO, "switch writing delayed %s", OFF);
		struct timespec tp;
		clock_gettime(CLOCK_MONOTONIC, &tp);
		tp.tv_sec += 5;

		while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tp, NULL) == EINTR) {
		}
		syslog(LOG_USER | LOG_INFO, "switch writing %s", OFF);
		if (fwrite(OFF, 1, 3, f) != 3)
			syslog(LOG_USER | LOG_ERR, "switch %s failed %d", OFF, errno);
		syslog(LOG_USER | LOG_INFO, "switch written %s", OFF);
	}
	fclose(f);
	syslog(LOG_USER | LOG_INFO, "switch done");
	return 0;
}
