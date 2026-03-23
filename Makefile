PROG=          mbimapidle
PREFIX?=	/usr/local
LOCALBASE?=	/usr/local
BINDIR=		${PREFIX}/bin
LIBDIR=		${PREFIX}/lib

MKC_REQUIRE_PKGCONFIG=libssl libcrypto
MKC_CACHEDIR=mk-cache

SRCS= main.c mbox.c imap.c base64.c common.c

WARNS = 4

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

.include <mkc.prog.mk>

install: conf_install

clean:
	rm -rf ${MKC_CACHEDIR} ${CLEANFILES}

help:
	@echo "Targets are: all, install, clean, help"
