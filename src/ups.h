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
#ifndef _UPS_H_
#define _UPS_H_
/*
 * ups.h: generic UPS serial I/O
 * $Id: ups.h,v 1.7 2006/08/22 04:18:25 taca Exp $
 */
enum ups_status {
	NONE, INITIAL, STARTING, READY, LINE_LOST, BATTERY_LOW
};

enum po_state {
	NORMAL, FAIL, RECOVER
};

struct ups {
	char		*name;		/* UPS model name */
	char		*device;	/* UPS device */
	char		*lock;		/* lock name for uu_lock */
	int		fd;		/* file descriptor for device */
	int		interval;	/* polling interval */
	enum ups_status	status;		/* UPS's status */
	int		tty_status;	/* tty status */
	time_t		when;		/* last polled time */

	enum po_state	state;		/* power state */
	time_t		failed;		/* failed time */
	time_t		recovered;	/* recovered time */
	time_t		fail_timeout;	/* limit of power fail btimeout */
	time_t		grace_time;	/* limit of power fail btimeout */

	char		*error;		/* saved error string */

	int		flag;		/* UPS flag */
#define	UPS_NOACTION	1
#define	UPS_KERNEL	2

	BITS		init_bits;	/* set bits for initilize */
	BITS		startup_bits;	/* check bits for ups startup */
	BITS		status_bits;	/* set bits before getting status */
	BITS		line_bits;	/* check bits for line lost */
	BITS		low_bits;	/* check bits for low battery */
	BITS		sleep_bits;	/* set bits for sleeping UPS */

	char		**arg;		/* shutdown parameter */
	int		timeout;	/* timeout for sleep */
};

typedef struct ups *UPS;

#ifndef TRUE
#define	TRUE	(1)
#endif
#ifndef FALSE
#define	FALSE	(0)
#endif

/* some utitlity functions */
UPS ups_new __P((char *, char *, int, int));
int ups_open __P((UPS, int));
int ups_init __P((UPS, char *path));
int ups_status __P((UPS));
int ups_sleep __P((UPS));
void ups_close __P((UPS, int));
void mysleep __P((unsigned int));

#endif /* _UPS_H_ */
