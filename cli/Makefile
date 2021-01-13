DESTDIR?=
PREFIX?=	/usr/local
MANPREFIX?=	${PREFIX}/man
SYSCONFDIR?=	/etc

CFLAGS+=	-Wall -Wextra
VPATH+=		../common
OBJS+=		netwake.o wol.o

all: netwake netwake.1

netwake: ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJS} ${LDLIBS}

clean:
	rm -f netwake *.o config.h netwake.1

install: all netwake.1
	install -d ${DESTDIR}${PREFIX}/bin
	install -d ${DESTDIR}${MANPREFIX}/man1
	install -m755 netwake   ${DESTDIR}${PREFIX}/bin/
	install -m644 netwake.1 ${DESTDIR}${MANPREFIX}/man1/

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/netwake
	rm -f ${DESTDIR}${MANPREFIX}/man1/netwake.1

config.h:
	echo "#define SYSCONFDIR \"${SYSCONFDIR}\"" >config.h

netwake.1: netwake.1.in
	sed -e 's,/etc,${SYSCONFDIR},' <netwake.1.in >netwake.1

netwake.o:	wol.h config.h
wol.o:		wol.h

.PHONY: all clean install uninstall
