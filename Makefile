BIN := fand

CFLAGS := -Wall -pipe -O2
LDLIBS := -lgpiod

all: $(BIN)

clean::
	rm -f $(BIN) *.o
