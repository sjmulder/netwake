CC=		i686-w64-mingw32-gcc
RES=		i686-w64-mingw32-windres

CFLAGS+=	-Os -Wall -Wextra
LDFLAGS+=	-mwindows -s
LDLIBS+=	-lkernel32 -luser32 -lgdi32 -lcomctl32 -lws2_32

#CFLAGS+=	-DNOSTDLIB
#LDFLAGS+=	-nostdlib -ffreestanding

VPATH+=		../

all: netwake.exe

clean:
	rm -f netwake.exe *.o

netwake.exe: winmain.o resource.o wol.o util.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

resource.o: resource.rc
	$(RES) $(RESFLAGS) $< $@

winmain.o:	wol.h util.h resource.h
wol.o:		wol.h util.h
resource.o:	resource.h

.PHONY: all clean
