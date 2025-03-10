EXE = peer
CC = gcc
CFLAGS = -Wall
LDLIBS =

.PHONY: all clean

all: $(EXE)

$(EXE): peer_to_peer.c
	$(CC) $(CFLAGS) peer_to_peer.c $(LDLIBS) -o $(EXE)

clean:
	rm -f $(EXE)

