#
# Copyright (C) 2003-2012 Takahiro Kambe
# All rights reserved.
#
# bkpupsd - Simple UPS daemon for APC Back-UPS Pro. series
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the author nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
OPSYS!=uname -s
PROG=	bkpupsd
SRCS=	bits.c main.c ups.c str2arg.c
UPSTAB=	${PREFIX}/etc/upstab

.if exists(/usr/include/sys/com_ups.h)
CFLAGS+=	 -DHAVE_UPS_CONTROL -DDEBUG
.endif

.if (${OPSYS} == "FreeBSD")
CFLAGS+=	-DHAVE_LIBUTIL
CFLAGS+=	-Wall -Wuninitialized -Wunused -Wpointer-arith
LDADD+=		-lutil
MAN5=		upstab.5
MAN8=		bkpupsd.8
.else
WARNS=		1
MAN=		upstab.5 bkpupsd.8
.endif

OPTIM=-g -O

CFLAGS+=	-DDEFAULT_DEVICE=\"${DEFAULT_DEVICE}\"
CFLAGS+=	-DUPSTAB=\"${UPSTAB}\"
CFLAGS+=	-DSCRIPT_DIR=\"${PREFIX}/libexec/bkpupsd\"
CFLAGS+=	${OPTIM}
CLEANFILES+=	upstab.5 bkpupsd.8

all:	${PROG} ${MAN8}

upstab.5: upstab.5.in
	@${SED} -e 's#%%prefix%%#${PREFIX}#g' \
		${.CURDIR}/${.ALLSRC} > ${.TARGET}

bkpupsd.8: bkpupsd.8.in
	@${SED} -e 's#%%prefix%%#${PREFIX}#g' \
		${.CURDIR}/${.ALLSRC} > ${.TARGET}

afterinstall:
	${INSTALL} -c -m 0644 -o ${BINOWN} -g ${BINGRP} ${.CURDIR}/upstab \
		${UPSTAB}

.include <bsd.prog.mk>
