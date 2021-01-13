DESTDIR?=
PREFIX?=	/usr/local
MANPREFIX?=	${PREFIX}/man
SYSCONFDIR?=	/etc

CFLAGS+=	-Wall -Wextra
VPATH+=		../common

all: netwake

netwake: netwake.o wol.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LDLIBS}

clean:
	rm -f netwake *.o config.h

install: all
	install -d ${DESTDIR}${PREFIX}/bin
	install -d ${DESTDIR}${MANPREFIX}/man1
	install -m755 netwake   ${DESTDIR}${PREFIX}/bin/
	install -m644 netwake.1 ${DESTDIR}${MANPREFIX}/man1/

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/netwake
	rm -f ${DESTDIR}${MANPREFIX}/man1/netwake.1

config.h:
	echo "#define SYSCONFDIR \"${SYSCONFDIR}\"" >config.h

netwake.o:	wol.h config.h
wol.o:		wol.h

.PHONY: all clean install uninstall
