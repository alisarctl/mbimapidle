PROG=           mbimapidle
PROG_VERSION=   0.90
PREFIX?=	/usr/local
LOCALBASE?=	/usr/local
BINDIR=		${PREFIX}/bin
LIBDIR=		${PREFIX}/lib

MKC_REQUIRE_PKGCONFIG=libssl libcrypto
MKC_CACHEDIR=mk-cache

SRCS= main.c mbox.c imap.c base64.c common.c

WARNS = 4

.include <mkc.prog.mk>

clean:
	rm -rf ${MKC_CACHEDIR}

help:
	@echo "Targets are: all, install, clean, help"
