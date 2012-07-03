/*
 * $Id: main.c,v 1.24 2006/09/29 07:29:47 taca Exp $
 *
 * Copyright (C) 2003, 2004, 2005, 2006 Takahiro Kambe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the auhor nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* bkpupsd - Simple UPS daemon for APC Back-UPS pro. series
 * Copyright (c) 1997, Yoshifumi Watanabe<mwatts@edu1.tokyo-med.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$Id: main.c,v 1.24 2006/09/29 07:29:47 taca Exp $");
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <syslog.h>
#include <termios.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#ifdef HAVE_UPS_CONTROL
#include <sys/com_ups.h>
#endif
#include "bits.h"
#include "ups.h"

#define LINE_RECOVER_TIME	5

#define	FORK_RETRY		50
#define	FALLBACK_TIMEOUT	300

#ifndef LOG_BKPUPSD
#define LOG_BKPUPSD	LOG_DAEMON
#endif

#ifndef	DEFAULT_DEVICE
#define DEFAULT_DEVICE		"/dev/dty00"
#endif

#ifndef SCRIPT_DIR
#define SCRIPT_DIR		"/usr/pkg/libexec/bkpupsd"
#endif

#ifndef _BKPUPSD_PID_PATH
#define _BKPUPSD_PID_PATH	"/var/run/bkpupsd.pid"
#endif

#ifndef _BKPUPSD_WAITREADY_PATH
#define _BKPUPSD_WAITREADY_PATH	"/var/run/bkpupsd.waitready"
#endif

#ifndef _SHUTDOWN_PATH
#define _SHUTDOWN_PATH		"/sbin/shutdown"
#endif

#ifndef SHUTDOWN_ARG
#define SHUTDOWN_ARG		"-h", "now"
#endif

static char version[] = "2.2.1";

#define UPS_SHUTDOWN		0
#define UPS_LINE_LOST		1
#define UPS_LINE_RECOVERED	2
#define UPS_BATTERY_SHUTDOWN	3

int log_local[] = {
	LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3,
	LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7,
};

char *scripts[] = {
	"line.shutdown",
	"line.lost",
	"line.recover",
	"battery.shutdown",
};

int test_mode		= 0;
int sleep_on_exit	= 0;
int dont_sleep_ups	= 0;
UPS ups;
#ifdef HAVE_UPS_CONTROL
int kernel_mode		= 0;
#endif
char *pid_file		= _BKPUPSD_PID_PATH;
char *waiting_file	= _BKPUPSD_WAITREADY_PATH;

int main __P((int, char **));
void bkpupsd __P((void));
void usage __P((int));
static int check_shutdown_arg __P((char **));
static void execomand __P((UPS, int));
static void makepid __P((void));
static void waitchildren __P((int));
static void intr __P((int));
static void quit __P((int));

int main(argc, argv)
int argc;
char  **argv;
{
	int ch;
	int n;
	int log_fac;
	int debug = 0;
	char *name = NULL, *device, *fmt;
	int fail_timeout = 0;
	int grace_time = LINE_RECOVER_TIME;

	log_fac = LOG_BKPUPSD;
	while ((ch = getopt(argc, argv, "dhnpr:s:t:v")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		case 'n':
			test_mode = 1;
			break;
		case 'p':
			dont_sleep_ups = 1;
			break;
		case 'r':
			n = atoi(optarg);
			if (n > 0)
				grace_time = n;
			break;
		case 's':
			n = atoi(optarg);
			if (n >= 0 && n <= 7)
				log_fac = log_local[n];
			break;
		case 't':
			n = atoi(optarg);
			if (n > 0)
				fail_timeout = n;
			break;
		case 'v':
			printf("bkpupsd version %s\n", version);
			exit(0);
			break;
		default:
			usage(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 1) {
		fputs("ups_spec isn't specified\n", stderr);
		usage(EXIT_FAILURE);
	}
	name = argv[0];

	device = strchr(name, ':');
	if (device == NULL)
		device = DEFAULT_DEVICE;
	else
		*device++ = '\0';
	ups = ups_new(name, device, fail_timeout, grace_time);

	/* Detach and Become to a daemon process */
	if (!debug && daemon(0, 0) < 0) {
		err(1, "daemon");
		exit(1);
	}

	openlog("bkpupsd", LOG_PID, log_fac);

	signal(SIGHUP, intr);
	signal(SIGINT, intr);
	signal(SIGTERM, intr);

	if (!(ups->flag & UPS_NOACTION) && ups->low_bits == NULL && ups->fail_timeout == 0) {
		ups->fail_timeout = FALLBACK_TIMEOUT;
		syslog(LOG_INFO,
		       "No battery low status checking, "
		       "use default timeout %lld seconds",
		       (long long)ups->fail_timeout);
	}

	/* open UPS */
	if (ups_open(ups, O_RDWR | O_NDELAY) != 0) {
		syslog(LOG_ERR, "failed to open UPS.");
		quit(1);
	}

	if (check_shutdown_arg(ups->arg) != 0) {
		syslog(LOG_ERR, "shutdown argument error");
		quit(1);
	}

	/* wait for children */
	signal(SIGCHLD, waitchildren);

	if (test_mode) {
		fmt = "bkpupsd %s started for %s in test mode";
	} else {
		fmt = "bkpupsd %s started for %s";
	}
	syslog(LOG_INFO, fmt, version, name);

	if (!debug)
		makepid();

	if (ups_init(ups, waiting_file) < 0)
		quit(1);

	bkpupsd();
	return 0;
}

void
usage(status)
	int status;
{
	fputs("Usage: bkpupsd [-dhnpv] [-r sec] [-s fac] [-t sec] ups_spec [, ups_spec, ...]\n", stderr);
	if (status == EXIT_SUCCESS) {
		fputs("\t-d: debug mode, don't detach from terminal.\n", stderr);
		fputs("\t-h: Show help, this message.\n", stderr);
		fputs("\t-n: Don't execute external script and set UPS for testing only.\n", stderr);
		fputs("\t-p: Never make ups in sleep mode.\n", stderr);
		fputs("\t-r: Specify the line recovery time.\n", stderr);
		fputs("\t-s: Specify syslog facility, LOCAL_#.\n", stderr);
		fputs("\t-t: Specify UPS shutdown time from line has lost.\n",
		      stderr);
		fputs("\t-v: Show version.\n", stderr);
		fputs("\tups_spec: ups_type:device, specify ups.\n", stderr);
	}
	exit(status);
}

void
bkpupsd()
{
	int status;

	ups->state = NORMAL;
	ups->failed = 0;
	ups->recovered = 0;
	while (1) {
		status = ups_status(ups);
		if (status < 0) {
			syslog(LOG_ERR, "status error");
			quit(1);
		}

		switch (ups->state) {
		case NORMAL:
			switch (ups->status) {
			case READY:
				break;
			case LINE_LOST:
				ups->state = FAIL;
				ups->failed = ups->when;
				syslog(LOG_ALERT, "line power has lost.");
				execomand(ups, UPS_LINE_LOST);
				break;
			default:
				goto error;
			}
			break;
		case FAIL:
			switch (ups->status) {
			case READY:
				ups->state = RECOVER;
				ups->recovered = ups->when;
				break;
			case LINE_LOST:
				if (ups->fail_timeout >0 &&
				    ups->when - ups->failed > ups->fail_timeout) {
					/* power fail timeout */
					syslog(LOG_EMERG, "shutdown for line power failure timeout.");
					if (!test_mode)
						sleep_on_exit = 1;
					execomand(ups, UPS_SHUTDOWN);
					quit(0);
				}
				break;
			case BATTERY_LOW:
				syslog(LOG_ALERT, "battery is low.");
				if (!test_mode)
				    sleep_on_exit = 1;
				execomand(ups, UPS_BATTERY_SHUTDOWN);
				quit(0);
			default:
				goto error;
			}
			break;
		case RECOVER:
			switch (ups->status) {
			case READY:
				if (ups->when - ups->recovered > ups->grace_time) {
					syslog(LOG_ALERT, "line power has recovered.");
					execomand(ups, UPS_LINE_RECOVERED);
					ups->state = NORMAL;
					ups->failed = 0;
					ups->recovered = 0;
				}
				break;
			case  LINE_LOST:
				ups->state = FAIL;
				break;
			default:
				goto error;
			}
			break;
		default:
			goto error;
		}
	}

	/* should not come to here. */
 error:
	syslog(LOG_ERR, "panic: no such power state %d/ups status %d",
	       ups->state, ups->status);
	quit(2);
}

/*
 * check arguemtn for shutdown(8).
 */
static int
check_shutdown_arg(arg)
	char **arg;
{
	char **s;
	int status = 1;

	for (s = arg; *s != NULL; s++) {
		if (strcmp(*s, "-h") == 0 || strcmp(*s, "-p") == 0)
			status = 0;
	}
	return status;
}


/* Subroutine to execute shell script command */

static void
execomand(ups, cmd)
	UPS ups;
	int cmd;
{
	int pid, n;
	struct stat st;
	char command[BUFSIZ];

	if (cmd < 0 || cmd >= (sizeof scripts / sizeof *scripts)) {
		syslog(LOG_ERR, "illegal cmd %d", cmd);
		return;
	}
	sprintf(command, "%s/%s", SCRIPT_DIR, scripts[cmd]);

	if (test_mode) {
		if (stat(command, &st) < 0) {
			syslog(LOG_ERR, "%s: %m", command);
			return;
		}
		syslog(LOG_INFO, "execute %s (testing)", command);
		return;
	}

	n = 0;
	while ((pid = fork()) < 0) {
		syslog(LOG_ERR, "fork failed: %m");
		n++;
		if (errno != EAGAIN && errno != ENOMEM && n > FORK_RETRY) {
			syslog(LOG_EMERG,
			       "bkpupsd exit without executing the command.");
			quit(1);
		}
		mysleep(1);
	}

	if (pid == 0) {
		execl(_PATH_BSHELL, "sh", "-c", command, NULL);
		syslog(LOG_ERR, "execution failed: %m");
		exit(1);
	}
	/* parent */
	syslog(LOG_NOTICE, "execute %s", command);
}

/* Subroutine to make bkpupsd.pid to /var/run/ directory. */
static void
makepid()
{
	int fd;
	char buf[BUFSIZ];

	fd = open(pid_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		syslog(LOG_ERR, "%s: open: %m", pid_file);
		exit(1);
	}
	sprintf(buf, "%d\n", getpid());
	if (write(fd, buf, strlen(buf)) < 0) {
		syslog(LOG_ERR, "%s: write: %m", pid_file);
		quit(1);
	}
	close(fd);
}

/*
 * wait handler.
 */
static void
waitchildren(sig)
	int sig;
{
	int status;

	while (wait3(&status, WNOHANG, NULL) == 0 && WIFEXITED(status))
	       ;
}

/*
 * signal handler.
 */
static void
intr(sig)
	int sig;
{
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	quit(3);
}

/*
 *
 */
static void
quit(sig)
	int sig;
{
	unlink(pid_file);
	if (!sleep_on_exit) {
		ups_close(ups, 0);
		syslog(LOG_INFO, "bkpupsd exit.");
		exit(sig);
	}

	syslog(LOG_NOTICE, "make UPS sleep.");
	if (!dont_sleep_ups)
		ups_sleep(ups);
	ups_close(ups, 1);
	syslog(LOG_NOTICE, "bkpupsd shutdown the system.");
	sync(); sync();
	execv(_SHUTDOWN_PATH, ups->arg);
	syslog(LOG_ERR, "%s: %m: normal shutdown failed", _SHUTDOWN_PATH);
	execl(_SHUTDOWN_PATH, _SHUTDOWN_PATH, SHUTDOWN_ARG, NULL);
	syslog(LOG_ERR, "%s: %m", _SHUTDOWN_PATH);
	_exit(EXIT_FAILURE);
}
