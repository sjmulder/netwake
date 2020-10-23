CC=	i686-w64-mingw32-gcc
RES=	i686-w64-mingw32-windres

CFLAGS+=	-Os -Wall -Wextra
LDFLAGS+=	-mwindows -s
LDLIBS+=	-lkernel32 -luser32 -lgdi32 -lcomctl32 -lws2_32

all: netwake.exe

clean:
	rm -f netwake.exe *.o

netwake.exe: winmain.o resource.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

resource.o: resource.rc
	$(RES) $(RESFLAGS) $< $@
