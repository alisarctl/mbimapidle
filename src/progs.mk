# Add dependency on Makefile
.if !empty(PROGS)
. for p in ${PROGS}
ALLOBJS = ${SRCS.${p}:.c=.o}
.  for o in ${ALLOBJS}
${o}: Makefile
.  endfor
. endfor
.include <progs.mk>
.elif !empty(PROG)
ALLOBJS = ${SRCS:.c=.o}
.  for o in ${ALLOBJS}
${o}: Makefile
.  endfor
.include <bsd.prog.mk>
.endif

clean_progs:
	rm -rf *.d *.o ${PROGS}

clean: ${clean} clean_progs

help:
	@echo "Targets are: all, install, clean, help"

