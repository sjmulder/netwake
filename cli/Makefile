CFLAGS+=	-Wall -Wextra
VPATH+=		../common

all: netwake

netwake: netwake.o wol.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LDLIBS}

clean:
	rm netwake

netwake.o:	wol.h
wol.o:		wol.h

.PHONY: all clean
