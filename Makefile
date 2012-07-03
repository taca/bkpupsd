#
# Copyright (C) 2003-2012 Takahiro Kambe
# All rights reserved.
#
OPSYS!=	uname -s
SUBDIR=	src scripts
PKG=	pkg.${OPSYS}

config:
	@rm -f Makefile.inc; ln -s Makefile.inc.${OPSYS} Makefile.inc

depend: config
	@cd src; make $@

all: depend
	@cd src; make $@
	@cd scripts; make $@

clean:
	@for d in ${SUBDIR}; do \
		(cd ${.CURDIR}/$$d; $(MAKE) clean); \
	done

package deinstall:
	cd ${.CURDIR}/${PKG}; $(MAKE) $@

realclean: cleandir
#	cd ${.CURDIR}/${PKG}; $(MAKE) cleandir
	@rm -f Makefile.inc

.include <bsd.subdir.mk>
