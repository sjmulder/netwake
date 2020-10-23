CC=		i686-w64-mingw32-gcc
RES=		i686-w64-mingw32-windres

CFLAGS+=	-Os -Wall -Wextra -fPIE -DNOSTDLIB
LDFLAGS+=	-s -mwindows -nostdlib
# -pie breaks the executable for non-Win32s
LDFLAGS+=	-Wl,--dynamicbase,--export-all-symbols
LDLIBS+=	-lkernel32 -luser32 -lgdi32 -ladvapi32 -lwsock32

VPATH+=		../

all: netwake.exe

clean:
	rm -f netwake.exe netwake.fd *.o *.tmp

netwake.exe: winmain.o resource.o wol.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

netwake.fd: netwake.exe README.md LICENSE.md
	dd if=/dev/zero of=netwake.fd.tmp bs=1440K count=1 status=none
	mformat -i netwake.fd.tmp -f 1440 -v netwake ::
	mcopy -i netwake.fd.tmp $^ ::/
	mv netwake.fd.tmp netwake.fd

resource.o: resource.rc
	$(RES) $(RESFLAGS) $< $@

winmain.o:	wol.h resource.h
wol.o:		wol.h
resource.o:	resource.h

.PHONY: all clean
