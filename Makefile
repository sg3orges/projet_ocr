### ===== PROJECT =====

EXEC = ocr_project

# Dossiers contenant les sources
SRC_DIRS = rotations interface detectionV2 neuronne solver xnor

# Récupère tous les .c sauf ceux nommés main.c dans les sous-dossiers
SUB_SRC = $(foreach dir,$(SRC_DIRS),$(filter-out $(dir)/main.c,$(wildcard $(dir)/*.c)))

# Le main principal (dans la racine du projet)
MAIN_SRC = main.c

# Liste complète des sources
SRC = $(MAIN_SRC) $(SUB_SRC)

# Objets générés à côté des .c
OBJ = $(SRC:.c=.o)



### ===== COMPILATEUR =====

CC = gcc

# GTK3
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   = $(shell pkg-config --libs   gtk+-3.0)

# SDL2 + SDL2_image
SDL_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image)
SDL_LIBS   = $(shell pkg-config --libs   sdl2 SDL2_image)

# Flags
CFLAGS  = -Wall -Wextra -O2 -std=c11 $(GTK_CFLAGS) $(SDL_CFLAGS) -Irotations -Iinterface -IdetectionV2 -Ineuronne -Isolver -Ixnor
LDFLAGS = -lm $(GTK_LIBS) $(SDL_LIBS)



### ===== RULES =====

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compilation d'un .c → .o dans le même dossier
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@



### ===== MODES SPÉCIAUX =====

debug: CFLAGS += -Og -g -DDEBUG
debug: clean $(EXEC)

sanitize: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1
sanitize: LDFLAGS += -fsanitize=address,undefined
sanitize: clean $(EXEC)



### ===== CLEAN =====

clean:
	@echo "Cleaning object files and temporary folders..."
	rm -f $(OBJ)
	rm -f $(EXEC)
	rm -rf cells letterInWord images GRIDL GRIDWO CELLPOS

fclean: clean
	@echo "Cleaning executable..."
	rm -f $(EXEC)

re: fclean all



### ===== PHONY =====

.PHONY: all clean fclean re debug sanitize
