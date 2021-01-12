SYSCONFDIR?=	/etc

CFLAGS+=	-Wall -Wextra
VPATH+=		../common

all: netwake

netwake: netwake.o wol.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LDLIBS}

clean:
	rm netwake config.h

config.h:
	echo "#define SYSCONFDIR \"${SYSCONFDIR}\"" >config.h

netwake.o:	wol.h config.h
wol.o:		wol.h

.PHONY: all clean
