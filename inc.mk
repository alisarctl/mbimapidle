GIT_REV !=	git describe --abbrev=7 --dirty --always
BASE_VERSION=	0.91
PROG_NAME=	mbimapidle

.if defined(RELEASE)
VERSION=	${BASE_VERSION}
.else
VERSION=	${BASE_VERSION}-${GIT_REV}
.endif
DOCDIR?=	${DATADIR}/doc/${PROG_NAME}
