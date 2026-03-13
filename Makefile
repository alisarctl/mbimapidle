MAKEOBJDIR=    obj

PROG=          mbimapidle
MAN=
PREFIX?=	/usr/local
LOCALBASE?=	/usr/local
BINDIR=		${PREFIX}/bin
MANDIR=		${PREFIX}/share/man/man
LIBDIR=		${PREFIX}/lib

SRCS= main.c mbox.c imap.c base64.c common.c

CFLAGS+=       -I${.CURDIR} -Wall
LDFLAGS+=      -lssl -lcrypto

.if defined(RC) && ${RC} == "openrc"
RCDIR?=	/etc/user/init.d/
conf_install:
	sed -e 's|@BINDIR[@]|${BINDIR}|g' openrc/mbimapidle.in > openrc/mbimapidle
	install -d -m 0755 ${RCDIR}
	install -m 0755 openrc/mbimapidle ${RCDIR}
	rm -f openrc/mbimapidle
.endif

.if defined(RC) && ${RC} == "systemd"
RCDIR?=	/etc/systemd/user
conf_install:
	sed -e 's|@BINDIR[@]|${BINDIR}|g' systemd/mbimapidle.in > systemd/mbimapidle
	install -d -m 0755 ${RCDIR}
	install -m 0755 systemd/mbimapidle ${RCDIR}
	rm -f systemd/mbimapidle
.endif

CLEANFILES= ${GENSRCS}

.include <bsd.prog.mk>

install: maninstall conf_install install_links _SUBDIRUSE

help:
	@echo "Targets are: all, install, clean, help"
