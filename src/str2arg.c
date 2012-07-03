/*
 * $Id: str2arg.c,v 1.4 2006/08/22 06:13:47 taca Exp $
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
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$Id: str2arg.c,v 1.4 2006/08/22 06:13:47 taca Exp $");
#endif
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include "str2arg.h"

TAILQ_HEAD(argq, argument);

struct argument {
	char *string;
	TAILQ_ENTRY(argument) ags;
};

static char *skip_space(char *s);
static void addarg(struct argq *, char *string, int *n);

static char *
skip_space(s)
	char *s;
{
	if (s == NULL)
		return s;
	while (*s != '\0' && (*s == ' ' || *s == '\t' || *s == '\n'))
		s++;
	return s;
}

static void
addarg(aq, string, n)
	struct argq *aq;
	char *string;
	int *n;
{
	struct argument *arg;
	arg = malloc(sizeof *arg);
	if (arg == NULL)
		err(1, "addarg(malloc): ");
	arg->string = string;
	TAILQ_INSERT_TAIL(aq, arg, ags);
	(void)*n++;
}

char **
str2arg(char *str)
{
	char *s, *t;
	int n, status;
	struct argq aq;
	struct argument *ag;
	char **argument, **a;
	char sep[2];

	if (str == NULL)
		return NULL;
	TAILQ_INIT(&aq);
	status = n = 0;
	a = argument = NULL;
	while (*(str = skip_space(str))) {
		s = strpbrk(str, " \t\n\"'");
		if (s == NULL) {
			addarg(&aq, str, &n);
			break;
		}
		switch (*s) {
		case ' ':	/* fall through */
		case '\t':
		case '\n':
			*s++ = '\0';
			addarg(&aq, str, &n);
			str = s;
			break;
		case '\'':	/* fall through */
		case '\"':
			sep[0] = *s++;
			sep[1] = '\0';
			t = strpbrk(s, sep);
			if (t == NULL) {
				status = 1;
				goto error;
			}
			else {
				*t++ = '\0';
				addarg(&aq, s, &n);
				str = t;
			}
			break;
		default:
			/* should not happen */
			status = 1;
			goto error;
		}
	}

	a = argument = calloc(sizeof(*argument), n + 1);
 error:
	while (!TAILQ_EMPTY(&aq)) {
		ag = TAILQ_FIRST(&aq);
		if (status == 0)
			*a++ = ag->string;
		TAILQ_REMOVE(&aq, aq.tqh_first, ags);
		free(ag);
	}
	if (status)
		return NULL;
	*a = NULL;
	return argument;
}

#if 0
#include <stdio.h>

main()
{
	char buf[BUFSIZ];
	char **a, **s;
	int n;

	while (fgets(buf, sizeof buf, stdin) != NULL) {
		a = str2arg(buf);
		n = 0;
		for (s = a; s != NULL && *s != NULL; s++) {
			printf("%d: \"%s\"\n", n++, *s);
		}
		putchar('\n');
	}
}
#endif
