MAN=	mbimapidle.1

TODAY != date "+%B %d, %Y"

mbimapidle.1: mbimapidle.1.in Makefile
	@sed -e 's|@DATE[@]|'"${TODAY}"'|g' \
	     -e 's|@VERSION[@]|'${PROG_VERSION}'|g' \
	mbimapidle.1.in > mbimapidle.1

.include <mkc.prog.mk>

clean:
	rm -f mbimapidle.1
