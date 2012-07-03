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
#include <string.h>
#include <unistd.h>
#include <sys/ttycom.h>
#include "bits.h"

#define SBUF	80

struct modem_bit {
	char *name;			/* name of the bit */
	unsigned int bit;		/* bit value */
};

struct bits {
	struct bits *next;		/* link to next one */
	int operation;			/* operation to perform */
	unsigned char bits;		/* set or reset bits */
};

struct modem_bit bit_table[] = {
	{"DTR", TIOCM_DTR},		/* DTR (DR)*/
	{"RTS", TIOCM_RTS},		/* RTS (RS) */
	{"CTS", TIOCM_CTS},		/* CTS (CS) */
	{"CAR", TIOCM_CAR},		/* CAR (CD or DCD) */
	{"RNG", TIOCM_RNG},		/* RNG (RI) */
	{"DSR", TIOCM_DSR},		/* DSR (DR) */
					/* Those are only for show status. */
	{"LE", TIOCM_LE},		/* line enable */
	{"ST", TIOCM_ST},		/* secondary transmit */
	{"SR", TIOCM_SR},		/* secondary receive */
	{NULL, 0},
};

static unsigned int modem_to_bit __P((const char *));
static BITS new_bits __P((void));
static void free_bits __P((BITS));

BITS
bits_parse(string)
	char *string;
{
	BITS b, cur;
	char *s, *t, **line;
	int op;
	unsigned int value = 0;

	b = cur = NULL;
	op = 0;
	s = strdup(string);
	if (s != NULL) {
		line = &s;
		while ((t = strsep(line, ",")) != NULL) {
			if (*t == '+')
				op = BITS_SET;
			else if (*t == '-')
				op = BITS_RESET;
			else {
				op = BITS_UNKNOWN;
				break;
			}

			t++;
			value = modem_to_bit(t);
			if (value == 0)
				break;

			if (b == NULL) {
				b = cur = new_bits();
			} else {
				cur->next = new_bits();
				cur = cur->next;
			}
			if (cur == NULL)
				break;

			cur->next = NULL;
			cur->operation = op;
			cur->bits = value;
		}
		if (op == BITS_UNKNOWN || value == 0 || cur == NULL) {
			free_bits(b);
		}
	}
	return b;
}

BITS
bits_next(b)
	BITS b;
{
	return b->next;
}

int
bits_op(b)
	BITS b;
{
	return b->operation;
}

int
bits_bits(b)
	BITS b;
{
	return b->bits;
}

unsigned int
bits_data(b, data)
	BITS b;
	unsigned int data;
{
	unsigned char n = data;

	if (b->operation == BITS_RESET)
		n &= ~(b->bits);
	else
		n |= b->bits;
	return n;
}

int
bits_test(b, data)
	BITS b;
	unsigned int data;
{
	int on;
	unsigned char n = data;

	on = ((n & b->bits) == b->bits);
	return (b->operation == BITS_RESET)? !on: on;
}

static unsigned int
modem_to_bit(name)
	const char *name;
{
	struct modem_bit *m;

	if (name != NULL) {
		for (m = bit_table; m != NULL; m++) {
			if (strcmp(name, m->name) == 0)
				return m->bit;
		}
	}
	return 0;
}

char *
bit_to_modem(bits)
	unsigned int bits;
{
	struct modem_bit *m;
	static char *buf;
	int n;

	if (buf == NULL)
		buf = malloc(SBUF + 1);
	if (buf == NULL)
		return NULL;
	*buf = '\0';
	for (m = bit_table; m != NULL && m->name != NULL; m++) {
		if ((bits & m->bit) == m->bit) {
			if (*buf) {
				n = SBUF - strlen(buf);
				if (n < 0)
					return NULL;
				strncat(buf, "|", n--);
				strncat(buf, m->name, n);
			} else {
				strcpy(buf, m->name);
			}
		}
	}
	return buf;
}

static BITS
new_bits()
{
	BITS p;

	p = calloc(1, sizeof *p);
	return p;
}

static void
free_bits(b)
	BITS b;
{
	BITS p;

	while (b != NULL) {
		p = b;
		b = b->next;
		free(p);
	}
}

#if 0
char *
bits_print(b)
	BITS b;
{
	char *s, *p, *q, *l;
	int length, n, first = 1;

	p = malloc(SBUF);
	if (p == NULL)
	    return NULL;
	length = SBUF;
	while (b != NULL && length > 0) {
		s = bit_to_modem(b->bits);
		if (s != NULL) {
			if (first) {
				first = 0;
				*q = '\0';
			} else {
				strncat(q, ", ", length);
				length -= 2;
				q += 2;
			}
			snprintf(q, length, "%s%s",
				 (b->operation)? "+": "-", s);
			n = strlen(q);
			length -= n;
			q += n;
		}
		b = bits_next(b);
	}
	if (length <= 0) {
		l = NULL;
	} else {
		l = strdup(p);
	}
	free(p);
	return l;
}
#endif
