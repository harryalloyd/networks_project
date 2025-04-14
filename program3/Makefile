EXE = peer
CC = gcc
CFLAGS = -Wall
LDLIBS =

.PHONY: all clean

all: $(EXE)

$(EXE): peer.c
	$(CC) $(CFLAGS) peer.c $(LDLIBS) -o $(EXE)

clean:
	rm -f $(EXE)

