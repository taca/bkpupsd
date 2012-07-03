/*
 * Copyright (C) 2003-2012 Takahiro Kambe
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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#ifdef HAVE_LIBUTIL
#include <libutil.h>
#else
#include <util.h>
#endif /* HAVE_LIBUTLI */
#ifdef HAVE_UPS_CONTROL
#include <sys/com_ups.h>
#endif
#include <termios.h>
#include "str2arg.h"
#include "bits.h"
#include "ups.h"

#ifndef	DEFAULT_INTERVAL
#define	DEFAULT_INTERVAL	1
#endif

static BITS cget_bits __P((char *, char *));
static int set_bits __P((int, BITS));
static int test_bits __P((BITS, int));
static int get_status __P((UPS));
static char *change_bits __P((int prev, int cur));

static void make_waitfile __P((char *path));
static void delete_waitfile __P((char *path));

#ifdef DEBUG
int ups_setbits_debug = 0;
#endif

#ifndef DEFAULT_SHUTDOWN_ARG
#define DEFAULT_SHUTDOWN_ARG		"-h now"
#endif

UPS
ups_new(name, device, timeout, grace)
	char *name;
	char *device;
	int timeout;
	int grace;
{
	static const char *file[] = { UPSTAB,
#ifdef DEBUG
				"upstab",
#endif
				NULL, };
	char *ups_cap;
	UPS ups;
	char *s, *how;

	if (cgetent(&ups_cap, file, name) != 0) {
		perror("ups_new: can't read upstab");
		return NULL;
	}

	ups = calloc(sizeof *ups, 1);
	if (ups == NULL) {
		perror("ups_new");
		return NULL;
	}

	ups->name = strdup(name);
	ups->device = device;
	s = strrchr(device, '/');
	ups->lock = (s == NULL)? device: (s + 1);

	ups->status = NONE;
	ups->interval = DEFAULT_INTERVAL;
	ups->fd = -1;
	ups->tty_status = -1;
	ups->fail_timeout = timeout;
	ups->grace_time = grace;

	ups->init_bits = cget_bits(ups_cap, "init");
	ups->startup_bits = cget_bits(ups_cap, "startup");
	ups->status_bits = cget_bits(ups_cap, "status");
	ups->line_bits = cget_bits(ups_cap, "line");
	ups->low_bits = cget_bits(ups_cap, "low");
	ups->sleep_bits = cget_bits(ups_cap, "sleep");
	if (cgetstr(ups_cap, "shutdown", &s) < 0) {
		s = DEFAULT_SHUTDOWN_ARG;
	}
	how = strdup(s);
	ups->arg = str2arg(how);
	if (ups->arg == NULL) {
		warn("problem on shutdown parameter: \"%s\"", s);
		free(how);
		free(ups->name);
		free(ups);
		ups = NULL;
		goto error;
	}

	if (cgetcap(ups_cap, "noaction", ':') != 0) 
		ups->flag |= UPS_NOACTION;
#ifdef HAVE_UPS_CONTROL
	if (cgetcap(ups_cap, "kernel", ':') != 0) 
		ups->flag |= UPS_KERNEL;
#endif
 error:
	free(ups_cap);
	return ups;
}

int
ups_open(ups, mode)
	UPS ups;
	int mode;
{
	int fd;
	struct termios tio;

	if (ups->fd >= 0) {
		syslog(LOG_ERR, "ups_open: ups_open called twice\n");
		return -1;
	}

	fd = open(ups->device, mode);
	if (fd < 0) {
		syslog(LOG_ERR, "ups_open: open %m");
		return -1;
	}

	if (ioctl(fd, TIOCGETA, &tio) < 0) {
		syslog(LOG_WARNING, "ups_open: TIOCGETA %m");
	} else {
		tio.c_cflag &= ~HUPCL;
		if (ioctl(fd, TIOCSETA, &tio) < 0) {
			syslog(LOG_WARNING, "ups_open: TIOCSETA %m");
		}
	}

	ups->fd = fd;
	ups->status = INITIAL;
	setproctitle("%s opened", ups->name);
	syslog(LOG_DEBUG, "ups_open: UPS is opened");
	return 0;
}

int
ups_init(ups, waitfile)
	UPS ups;
	char *waitfile;
{
	BITS b;
	int value, initial, result;

	value = get_status(ups);
	if (value < 0)
		return -1;
	syslog(LOG_INFO, "ups_init: status = %03o(%s)", value, bit_to_modem(value));
	
	if (set_bits(ups->fd, ups->init_bits) < 0)
		return -1;

	initial = 1;
	b = ups->startup_bits;
	if (b != NULL) {
		while ((value = get_status(ups)) >= 0) {
			result = test_bits(b, value);
			if (result)
				break;
			else {
				if (initial) {
					syslog(LOG_INFO,
					       "ups_init: status = %03o(%s)",
					       value, bit_to_modem(value));
					syslog(LOG_NOTICE,
					       "ups_init: waiting for UPS ready");
					initial = 0;
					setproctitle("%s waiting for ready",
						     ups->name);
					make_waitfile(waitfile);
				}
				mysleep(ups->interval);
			}
		}
		if (value < 0) {
			syslog(LOG_ERR, "ups_init: %m");
			return -1;
		}
	}
	ups->status = READY;
	delete_waitfile(waitfile);
	setproctitle("%s ready", ups->name);
	syslog(LOG_INFO, "ups_init: UPS is ready");
	return 0;
}

int
ups_status(ups)
	UPS ups;
{
	BITS line, low;
	int value, changed;
	char *s;

	line = ups->line_bits;
	low = ups->low_bits;

	mysleep(ups->interval);
	value = get_status(ups);
	time(&ups->when);
	if (value < 0)
		goto error;

	if (ups->tty_status == -1) {
		syslog(LOG_INFO, "ups_status: %03o(%s)", value,
		       bit_to_modem(value));
		changed = 1;
	} else if (ups->tty_status != value) {
		s = change_bits(ups->tty_status, value);
		syslog(LOG_INFO, "status changed: %s", s);
		changed = 1;
	} else
		changed = 0;

	if (changed || ups->status == LINE_LOST) {
		if (test_bits(line, value)) {
			if (ups->status == LINE_LOST && test_bits(low, value)) {
				ups->status = BATTERY_LOW;
				setproctitle("%s low battery", ups->name);
			} else {
				ups->status = LINE_LOST;
				setproctitle("%s, line power lost", ups->name);
			}
		} else {
			ups->status = READY;
			setproctitle("%s ready", ups->name);
		}
		ups->tty_status = value;
	}
	return 0;
 error:
	ups->status = NONE;
	setproctitle("%s error", ups->name);
	return -1;
}

int
ups_sleep(ups)
	UPS ups;
{
	BITS b;
	int status, value;

	if (ups == NULL || ups->fd < 0)
		return 0;
	b = ups->sleep_bits;
	if (ups->flag & UPS_NOACTION) {
		syslog(LOG_INFO, "ups_sleep: no action");
		return 0;
	}

#ifdef HAVE_UPS_CONTROL
	if (ups->flag & UPS_KERNEL) {
		int ovalue;

		value = get_status(ups);
		if (value < 0)
			return -1;
		ovalue = value;
		while (b != NULL) {
			value = bits_data(b, value);
			b = bits_next(b);
		}
		syslog(LOG_INFO, "ups_sleep: set to kernel %03o(%s)", value,
		       bit_to_modem(value));
		if (value != ovalue) {
			if (ioctl(ups->fd, UPSIOC_SET, &value) < 0) {
				syslog(LOG_DEBUG, "UPSIOC_SET %03o(%s): %m",
				       value, bit_to_modem(value));
				return -1;
			}
		}
		return 0;
	}
#endif
	sync(); sync();
	syslog(LOG_DEBUG, "ups_sleep: UPS will sleep");
#ifdef DEBUG
	ups_setbits_debug = 1;
#endif
	status = set_bits(ups->fd, b);
	if (status < 0)
		syslog(LOG_ERR, "ups_sleep: failed to sleep");
	value = get_status(ups);
	if (value < 0)
		return -1;
	syslog(LOG_DEBUG, "ups_sleep: status is now %03o(%s)",
	       value, bit_to_modem(value));
	return status;
}

void
ups_close(ups, keep)
	UPS ups;
	int keep;
{
	if (!keep) {
		if (ups->fd >= 0) {
			(void)close(ups->fd);
			ups->fd = -1;
		}
	}
	free(ups->name);
	free(ups);
	syslog(LOG_DEBUG, "ups_close: finished");
}

static BITS
cget_bits(cap, str)
	char *cap;
	char *str;
{
	BITS b;
	char *s;

	if (cgetstr(cap, str, &s) < 0)
		return NULL;
	b = bits_parse(s);
	free(s);
	return b;
}

static int
set_bits(fd, b)
	int fd;
	BITS b;
{
	int op, result, value;
	unsigned long request;

	result = 0;
	while (result == 0 && b != NULL) {
		request = 0;
		op = bits_op(b);
		value = bits_bits(b);
		switch (op) {
		case BITS_RESET:
			request = TIOCMBIC;
			break;
		case BITS_SET:
			request = TIOCMBIS;
			break;
		default:
			syslog(LOG_ERR,
			       "unknown modem bit operation %d", op);
			break;
		}
#ifdef DEBUG
		if (ups_setbits_debug)
		    syslog(LOG_DEBUG, "%s %03o(%s)",
			   (request == TIOCMBIC)? "clear":
			   (request == TIOCMBIS)? "set": "what?",
			   value, bit_to_modem(value));
#endif
		result = ioctl(fd, request, &value);
		if (result < 0) {
		    syslog(LOG_ERR, "ioctl set (%lx, %o): %m", request, value);
		}
		b = bits_next(b);
	}
	return result;
}

static int
get_status(ups)
	UPS ups;
{
	int fd, result, value;
	BITS s;

	fd = ups->fd;
	s = ups->status_bits;
	if (set_bits(fd, s) < 0)
		return -1;

	result = ioctl(fd, TIOCMGET, &value);
	if (result < 0) {
		syslog(LOG_ERR, "ioctl get (TIOCMGET): %m");
		return result;
	}
	return value;
}

static int
test_bits(b, value)
	BITS b;
	int value;
{
	int result;

	result = 0;
	while (b != NULL) {
		result = bits_test(b, value);
		if (!result)
			break;
		b = bits_next(b);
	}
	return result;
}

void
mysleep(second)
	unsigned int second;
{
	struct timeval timeout;

	timeout.tv_sec = second;
	timeout.tv_usec = 0;

	if (select(0, NULL, NULL, NULL, &timeout) < 0) {
		if (errno != EINTR) {
			syslog(LOG_ERR, "select: %m");
		}
	}
}

static char *
change_bits(prev, cur)
	int prev;
	int cur;
{
	static char buf[30];
	char *s, *u;
	int set, reset, m, n;

	set = ((prev ^ cur) & cur);
	reset = ((prev ^ cur) & prev);

	*buf = '\0';
	n = sizeof buf - 1;
	u = buf;
	if (set) {
		s = bit_to_modem(set);
		if (strchr(s, '|') == NULL) {
			snprintf(u, n, "+%03o(%s)", set, s);
		} else {
			snprintf(u, n, "+%03o(%s)", set, s);
		}
		m = strlen(u);
		n -= m;
		u += m;
	}
	if (reset) {
		if (set) {
			strcat(u, ",");
			u++;
			n--;
		}
		s = bit_to_modem(reset);
		if (strchr(s, '|') == NULL) {
			snprintf(u, n, "-%03o(%s)", reset, s);
		} else {
			snprintf(u, n, "-%03o(%s)", reset, s);
		}
	}
	return buf;
}

static void
make_waitfile(path)
	char *path;
{
	int fd;

	if (path != NULL) {
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd < 0) {
			syslog(LOG_ERR, "ups_init: opening waiting file: %m");
		} else {
			close(fd);
		}
	}
}

static void
delete_waitfile(path)
	char *path;
{
	if (path != NULL) {
		unlink(path);
	}
}
