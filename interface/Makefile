CC = gcc
CFLAGS = -Wall -Wextra `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0` -lm

# We only need main and gui
OBJ = main.o gui.o
EXEC = interface_app

# Default target
all: $(EXEC)

# Link the executable
$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ) $(LIBS)

# Compile main.c
main.o: main.c gui.h
	$(CC) $(CFLAGS) -c main.c -o main.o

# Compile gui.c
gui.o: gui.c gui.h
	$(CC) $(CFLAGS) -c gui.c -o gui.o

clean:
	@echo "Cleaning up..."
	rm -f *.o $(EXEC)
	rm -f images/*

.PHONY: all clean