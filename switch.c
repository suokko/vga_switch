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
#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
static const char *SWITCH_PATH = "/sys/kernel/debug/vgaswitcheroo/switch";

struct options {
	unsigned sleep : 3;
	unsigned switch_dis : 1;
	unsigned switch_igd : 1;
	unsigned wakeup_lvds : 1;
	unsigned prime : 2;
};

static struct options goptions = {0};

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

static void usage(const char *prog, const char*msg, int status)
{
	printf( "%s"
		"%s [OPTIONS]\n"
		"\n"
		"-h\tThis help\n"
		"-s pre|post|off|on\tpre sleep and post sleep operation\n"
		"-d\tSwitch to discrete card\n"
		"-i\tSwitch to integrated card\n"
		"-w\tWakeup lvds\n"
		, msg, prog
		);
	exit(status);
};

static int print(void)
{
	char buffer[4096];
	ssize_t s;
	FILE *f = fopen(SWITCH_PATH, "r");
	while ((s = fread(buffer, 1, sizeof(buffer) - 1, f)) > 0) {
		buffer[s] = '\0';
		puts(buffer);
	}
	fclose(f);
	return 0;
}

static int handle_sleep(void)
{
	static const char *ON  = "ON";
	static const char *OFF = "OFF";
	FILE *f = fopen(SWITCH_PATH, "w");
	int r;
	if (!f) {
		r = errno;
		perror("fopen: ");
		return r;
	}

	if ((goptions.sleep & 4) == 4) {
		r = daemon(0,0);
		if (r < 0) {
			r = errno;
			perror("daemon: ");
			fclose(f);
			return r;
		}

		for (r = SIGHUP; r < SIGSYS; r++)
			signal(r, handler);

		killprevious();
	}
	
	if ((goptions.sleep & 1) == 1) {
		syslog(LOG_USER | LOG_INFO, "switch writing %s", ON);
		if (fwrite(ON, 1, 2, f) != 2)
			syslog(LOG_USER | LOG_ERR, "switch %s failed %d", ON, errno);
		syslog(LOG_USER | LOG_INFO, "switch written %s", ON);
	} else {
		assert((goptions.sleep & 2) == 2);
		if ((goptions.sleep & 4) == 4) {
			syslog(LOG_USER | LOG_INFO, "switch writing delayed %s", OFF);
			struct timespec tp;
			clock_gettime(CLOCK_MONOTONIC, &tp);
			tp.tv_sec += 4;

			while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tp, NULL) == EINTR) {
			}
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

static void wakeup_display(int fd)
{
	drmModeRes *res = drmModeGetResources(fd);
	int i;
	for (i = 0; i < res->count_connectors; i++) {
		drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
		if (!conn) {
			syslog(LOG_USER | LOG_ERR, "Error fetching connector %d", i);
			continue;
		}

		if (conn->connector_type != DRM_MODE_CONNECTOR_LVDS) {
			drmFree(conn);
			continue;
		}

		//drmModeSetCrtc(drmModeSetCrtc(fd, res->crtc[0], bufferId, 0, 0, &res->connectors[i], 1, mode);
		int j;
		for (j = 0; j < conn->count_props; j++) {
			drmModePropertyPtr props = drmModeGetProperty(fd, conn->props[j]);
			if (!props) {
				syslog(LOG_USER | LOG_ERR, "Error fetching property %d", j);
				continue;
			}

			if (strcmp(props->name, "DPMS") == 0) {
				printf("%s = %lu\n", props->name, conn->prop_values[j]);
				syslog(LOG_USER | LOG_INFO, "Current dpms %lu", conn->prop_values[j]);
				if (conn->prop_values[j] != 0) {
					drmModeObjectSetProperty(fd, conn->connector_id, DRM_MODE_OBJECT_CONNECTOR, props->prop_id, DRM_MODE_DPMS_ON);
					syslog(LOG_USER | LOG_INFO, "HACK: Wokeup intel lvds");
				}
			}
	
			drmModeFreeProperty(props);
		}

		drmFree(conn);
	}
}

static int wakeup_intel_lvds()
{
	char buffer[4096];
	char *buf = buffer;
	FILE *f = fopen(SWITCH_PATH, "r");
	ssize_t s;
	while ((s = fread(buf, 1, sizeof(buffer) - 1, f)) > 0) {
		buf += s;
	}
	buf[s] = '\0';
	buf = buffer - 1;

	do {
		buf++;
		int id;
		int r;
		char type[4], def[2], power[4], pci[17];
		strcpy(pci, "pci:");
		r = sscanf(buf, "%d:%3s:%c:%3s:%12s\n",
				&id, type, def, power, pci + 4);
		if (r < 5)
			continue;

		if (strcmp(type, "IGD") != 0)
			continue;

		if (def[0] == '+') {
			syslog(LOG_USER | LOG_INFO, "Using IGD as primary no need to apply HACK");
			//return 0;
		}

		syslog(LOG_USER | LOG_INFO, "%d:%3s:%1c:%3s:%12s",
				id, type, def[0], power, pci);

		int fd = drmOpen("", pci);

		if (fd < 0) {
			syslog(LOG_USER | LOG_ERR, "drmOpen: %s", strerror(errno));
			continue;
		}

		wakeup_display(fd);

		drmClose(fd);
	} while ((buf = strchr(buf, '\n')));

	fclose(f);
	syslog(LOG_USER | LOG_INFO, "switch done");
	return 0;
}

static int handle_switch()
{
	static const char *DIS = "DIS";
	static const char *IGD = "IGD";
	char buffer[1024*16];
	char *buf = buffer;
	size_t s;
	const char *switch_to = goptions.switch_dis ? DIS : IGD;
	FILE *f = fopen(SWITCH_PATH, "w");
	syslog(LOG_USER | LOG_INFO, "switch writing %s", switch_to);
	if (fwrite(switch_to, 1, 3, f) != 3)
		syslog(LOG_USER | LOG_ERR, "switch %s failed %d", switch_to, errno);

	fclose(f);

	sleep(1);

	if (goptions.switch_dis) {
		f = fopen(SWITCH_PATH, "w");
		syslog(LOG_USER | LOG_INFO, "Enable integrated");
		if (fwrite("ON", 1, 2, f) != 3)
			syslog(LOG_USER | LOG_ERR, "Enabling IGD failed (%d)", errno);

		fclose(f);
	}

	f = fopen(SWITCH_PATH, "r");
	while ((s = fread(buf, 1, sizeof(buffer) - 1, f)) > 0) {
		buf += s;
	}
	buf[s] = '\0';
	buf = buffer - 1;

	do {
		buf++;
		int id;
		int r;
		char type[4], def[2], power[4], pci[17];
		strcpy(pci, "pci:");
		r = sscanf(buf, "%d:%3s:%c:%3s:%12s\n",
				&id, type, def, power, pci + 4);
		if (r < 5)
			continue;

		if (strcmp(type, switch_to) != 0)
			continue;

		if (def[0] != '+' || strcmp("Pwr", power) != 0)
			syslog(LOG_USER | LOG_INFO, "Switch not completed yet");

		syslog(LOG_USER | LOG_INFO, "%d:%3s:%1c:%3s:%12s",
				id, type, def[0], power, pci);
	} while ((buf = strchr(buf, '\n')));

	fclose(f);
	syslog(LOG_USER | LOG_INFO, "switch done");
	return 0;
}

static int prime_start_stop(void)
{
	int fd = open("/run/vga_switch_gl_count", O_CREAT | O_RDWR, 00777);
	if (fd < 0) {
		perror("Failed to open refcount fail");
		return -1;
	}
	fchmod(fd, 00777);

	ftruncate(fd, getpagesize());

	int *refcnt = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (refcnt == MAP_FAILED) {
		perror("mmap");
		return -2;
	}

	if (goptions.prime == 1) {
		/* start */
		if (__sync_fetch_and_add(refcnt, 1) == 0) {
			goptions.sleep = 1;
			handle_sleep();
		}
	} else {
		/* stop */
		if (__sync_add_and_fetch(refcnt, -1) == 0) {
			goptions.sleep = 2;
			handle_sleep();
		}
	}
	return 0;
}

static void parse(int argc, char * const * argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "s:dihwg:")) != -1) {
		switch (opt) {
		case 's':
			if (strcmp("post", optarg) == 0)
				goptions.sleep = 6;
			else if (strcmp("pre", optarg) == 0)
				goptions.sleep = 5;
			else if (strcmp("off", optarg) == 0)
				goptions.sleep = 2;
			else if (strcmp("on", optarg) == 0)
				goptions.sleep = 1;
			else
				usage(argv[0], "Invalid parameter to sleep switching\n\n", -1);
			break;
		case 'g':
			if (strcmp("start", optarg) == 0)
				goptions.prime = 1;
			else if (strcmp("stop", optarg) == 0)
				goptions.prime = 2;
			else
				usage(argv[0], "Invalid parameter to gl start parameter\n\n", -1);
			break;
		case 'd':
			goptions.switch_dis = 1;
			break;
		case 'i':
			goptions.switch_igd = 1;
			break;
		case 'w':
			goptions.wakeup_lvds = 1;
			break;
		case 'h':
			usage(argv[0], "", 0);
			break;
		default:
			usage(argv[0], "Invalid parameter.\n\n", -1);
		}
	}

	if (!!goptions.sleep + goptions.switch_dis + goptions.switch_igd > 1)
		usage(argv[0], "Only one of parameters can be specified\n", -1);
}

int main(int argc, char * const * argv)
{
	parse(argc, argv);

	if (goptions.sleep)
		return handle_sleep();
	else if (goptions.switch_dis || goptions.switch_igd)
		return handle_switch();
	else if (goptions.wakeup_lvds)
		return wakeup_intel_lvds();
	else if (goptions.prime)
		return prime_start_stop();
	return print();
}
