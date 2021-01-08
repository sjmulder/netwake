CFLAGS+=	-Wall -Wextra

SOURCES+=	netwake.c ../common/wol.c
HEADERS+=	../common/wol.h

all: netwake

netwake: ${SOURCES} ${HEADERS}
	${CC} ${CFLAGS} ${LDFLAGS} -o netwake ${SOURCES} ${LDLIBS}

clean:
	rm netwake

.PHONY: all clean
