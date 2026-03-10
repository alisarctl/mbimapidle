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

CLEANFILES= ${GENSRCS}

.include <bsd.prog.mk>

