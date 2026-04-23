# Copyright (c) 2026, Ali Abdallah <ali.abdallah@suse.com>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the <organization> nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Updated 13-April 2026

all: ${PROGS}

BINMODE?= 555
STRIP_FLAG?= -s
COPY?= -c

.if defined(SRCS)
depend: .depend
OBJS=${SRCS:.c=.o}
DEP=${SRCS:.c=.d}
.depend: ${SRCS}
	rm -f ./.depend
	${CC} ${CFLAGS} -MM ${SRCS} > ./.depend;
-include .depend
OBJS: ${SRCS}
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
DEPS+=.depend

.for o in ${OBJS}
${o}: Makefile .depend
.endfor
.endif

.for p in ${PROGS}
depend.${p}: .depend.${p}
OBJS.${p}=${SRCS.${p}:.c=.o}
DEP.${p}=${SRCS.${p}:.c=.d}

.depend.${p}: ${SRCS.${p}}
	${CC} ${CFLAGS} -MM ${SRCS.${p}} > ./.depend.${p};
-include .depend.${p}
DEPS+=.depend.${p}

{OBJS.${p}}: ${SRCS.${p}} ${OBJS}
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.for o in ${OBJS.${p}}
${o}: Makefile .depend.${p}
.endfor

${p}: ${OBJS.${p}} ${OBJS}
	${CC} -o ${.TARGET} ${OBJS.${p}} ${OBJS} ${LDADD}

.endfor # ${PROGS}

.for p in ${PROGS}
install.${p}: .PHONY
	install -d -m 775 ${BINDIR}
	install ${COPY} ${STRIP_FLAG} ${p} -m ${BINMODE} ${BINDIR}/${PROG_NAME}
installall+=install.${p}
.endfor

install: ${installall}

clean: .PHONY
	rm -f ${PROGS} *.o ${DEPS}

