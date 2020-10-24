CFLAGS+=	-Wall -Wextra `pkg-config --cflags gtk+-3.0`
LDLIBS+=	`pkg-config --libs gtk+-3.0`

VPATH+=		../

all: netwake

clean:
	rm -f netwake

netwake: main.o wol.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: all clean
