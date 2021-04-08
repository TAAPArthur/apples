CFLAGS := -Wall -Wextra
LDFLAGS := -lX11 -lxcb -lxcb-ewmh  -lxcb-screensaver
BIN := apples

SRC := $(BIN).c

all: $(BIN)

$(BIN): $(SRC)
	${CC} $^  -o $@ ${CFLAGS} ${LDFLAGS}

install: $(BIN)
	install -D -m 0755 $^ "${DESTDIR}/usr/bin/$^"

clean:
	rm -f *.o $(BIN)
.PHONY: all clean install
