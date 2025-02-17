EXE = h1-counter
CFLAGS = -Wall
CXXFLAGS = -Wall
LDLIBS =
CC = gcc
CXX = g++

.PHONY: all
all: $(EXE)

# Compile h1-counter from h1-counter.c
$(EXE): h1-counter.c
	$(CC) $(CFLAGS) h1-counter.c $(LDLIBS) -o $(EXE)

.PHONY: clean
clean:
	rm -f $(EXE)
