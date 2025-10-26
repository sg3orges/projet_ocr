CC = gcc
CFLAGS = -Wall -Wextra -std=c99
OBJ = main.o loader.o

all: loader_test

loader_test: $(OBJ)
	$(CC) $(CFLAGS) -o loader_test $(OBJ) -lm

main.o: main.c loader.h
loader.o: loader.c loader.h stb_image.h stb_image_write.h

clean:
	rm -f *.o loader_test
