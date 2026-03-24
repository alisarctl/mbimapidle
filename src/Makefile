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
CFLAGS   +=     -DPROG='"${PROG}"'
CFLAGS   +=     -DVERSION='"${PROG_VERSION}"'

.if defined(DEBUG)
CFLAGS   +=     -g -ggdb
.endif

.include <mkc.prog.mk>

# Add dependency on Makefile
ALLOBJS = ${SRCS:.c=.o}
.for o in ${ALLOBJS}
${o}: Makefile
.endfor

clean:
	echo "${OBJS}"
	rm -rf ${MKC_CACHEDIR} .depend

help:
	@echo "Targets are: all, install, clean, help"
