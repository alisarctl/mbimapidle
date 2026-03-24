MAN=	mbimapidle.1
DOCDIR?=	${DATADIR}/doc/${PROG_NAME}
CONF_EXAMPLE=	${DOCDIR}/mbimapidlerc

TODAY != date "+%B %d, %Y"

mbimapidle.1: mbimapidle.1.in Makefile
	@sed -e 's|@DATE[@]|'"${TODAY}"'|g' \
	     -e 's|@VERSION[@]|'${VERSION}'|g' \
	     -e 's|@CONF_EXAMPLE[@]|'${CONF_EXAMPLE}'|g' \
	mbimapidle.1.in > mbimapidle.1

.include "../inc.mk"
.include <mkc.prog.mk>

clean:
	rm -f mbimapidle.1
