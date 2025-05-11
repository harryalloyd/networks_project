EXE     = registry
CC      = gcc
CFLAGS  = -Wall
LDLIBS  =

.PHONY: all clean

all: $(EXE)

$(EXE): registry.c
	$(CC) $(CFLAGS) registry.c $(LDLIBS) -o $(EXE)

clean:
	rm -f $(EXE)

