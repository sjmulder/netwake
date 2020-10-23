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

netwake.exe: winmain.o resource.o wol.o util.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

netwake.fd: netwake.exe
	/sbin/mkfs.msdos -C netwake.fd.tmp 1440 >/dev/null
	mcopy -i netwake.fd.tmp netwake.exe ::/
	mv netwake.fd.tmp netwake.fd

resource.o: resource.rc
	$(RES) $(RESFLAGS) $< $@

winmain.o:	wol.h util.h resource.h
wol.o:		wol.h util.h
resource.o:	resource.h

.PHONY: all clean
