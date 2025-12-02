EXEC = ocr_ai
CC = gcc
CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image)
LDFLAGS = -lm $(shell pkg-config --libs sdl2 SDL2_image)
SRC = main.c networks.c
OBJ = $(SRC:.c=.o)

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)
