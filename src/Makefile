PROG=		${PROG_NAME}
PREFIX?=	/usr/local
BINDIR=		${PREFIX}/bin
LIBDIR=		${PREFIX}/lib

MKC_REQUIRE_PKGCONFIG=libssl libcrypto
MKC_REQUIRE_FUNCS3=vasprintf:stdio.h
MKC_CACHEDIR=mk-cache
MKDEPEND=yes

SRCS= main.c mbox.c imap.c base64.c common.c

WARNS = 4
CFLAGS   +=     -DPROG='"${PROG}"'
CFLAGS   +=     -DVERSION='"${VERSION}"'

.if defined(DEBUG)
CFLAGS   +=     -g -ggdb
.endif

.include "../inc.mk"
.include <mkc.prog.mk>

# Add dependency on Makefile
ALLOBJS = ${SRCS:.c=.o}
.for o in ${ALLOBJS}
${o}: Makefile
.endfor

clean:
	rm -rf ${MKC_CACHEDIR}

help:
	@echo "Targets are: all, install, clean, help"
