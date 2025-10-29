# interface/Makefile
CC = gcc
CFLAGS = -Wall -Wextra `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0`
OBJ = main.o gui.o
EXEC = interface_app

all: loader_build $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ) $(LIBS) -lm

loader_build:
	@echo "🔧 Compilation du module loader..."
	$(MAKE) -C ../loader all

clean:
	@echo "🧹 Nettoyage du module interface et loader..."
	rm -f *.o $(EXEC)
	$(MAKE) -C ../loader clean

.PHONY: all clean loader_build
