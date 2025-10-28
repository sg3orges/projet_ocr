# loader/Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
OBJ = loader.o main.o
EXEC = loader_test

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "🧹 Nettoyage du dossier loader..."
	rm -f *.o $(EXEC)
	rm -f images/*.png images/*.jpg images/*.jpeg images/*.bmp
	rm -f last_image.txt
	@echo "🗑️  Fichiers et images supprimés dans loader/"

.PHONY: all clean
