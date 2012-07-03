# $Id: Makefile,v 1.18 2006/08/22 04:15:58 taca Exp $
#
# Copyright (C) 2003, 2004, 2005, 2006 Takahiro Kambe
# All rights reserved.
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
SCRIPTS=	battery.shutdown line.lost line.recover line.shutdown
FILESDIR?=	${PREFIX}/libexec/bkpupsd
RCDIR?=		${PREFIX}/etc/rc.d
DOCDIR?=	${PREFIX}/share/doc/bkpupsd
CLEANFILES+=	${RCFILE} ${RCDFILE}
OPSYS!=uname -s
.if (${OPSYS} == "FreeBSD")
RCFILE=		bkpupsd.sh
RCTARGET=	bkpupsd.sh.sample
RCDFILE=	# empty
.else
RCFILE=		bkpupsd
RCTARGET=	bkpupsd
RCDFILE=	rc.conf.add
RCDTARGET=	rc.conf.add.sample
.endif

all: ${RCFILE} ${RCDFILE}

lint:

tags:

bkpupsd.sh: bkpupsd.sh.in
	sed	-e 's|%%prefix%%|${PREFIX}|g' \
		-e 's|%%device%%|${DEFAULT_DEVICE}|g' \
		${.CURDIR}/${.ALLSRC} > ${.TARGET}

bkpupsd: bkpupsd.in
	sed	-e 's|%%prefix%%|${PREFIX}|g' \
		-e 's|%%device%%|${DEFAULT_DEVICE}|g' \
		${.CURDIR}/${.ALLSRC} > ${.TARGET}

rc.conf.add: rc.conf.add.in
	sed	-e 's|%%prefix%%|${PREFIX}|g' \
		-e 's|%%device%%|${DEFAULT_DEVICE}|g' \
		${.CURDIR}/${.ALLSRC} > ${.TARGET}

scriptsinstall:

install:
	@test -d ${FILESDIR} || mkdir -p ${FILESDIR}
	@test -d ${RCDIR} || mkdir -p ${RCDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPTS} ${FILESDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${RCFILE} ${RCDIR}/${RCTARGET}
.if !empty(RCDFILE)
	@test -d ${DOCDIR} || mkdir -p ${DOCDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${RCDFILE} ${DOCDIR}/${RCDTARGET}
.endif

.include <bsd.prog.mk>
