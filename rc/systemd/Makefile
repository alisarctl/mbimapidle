RCDIR?=	/etc/systemd/user

conf_install:
.if defined(RC) && ${RC} == "systemd"
	sed -e 's|@BINDIR[@]|${BINDIR}|g' mbimapidle.in > mbimapidle
	install -d -m 0755 ${RCDIR}
	install -m 0755 mbimapidle ${RCDIR}
	rm -f mbimapidle
.endif

install: conf_install

.include <mkc.prog.mk>

