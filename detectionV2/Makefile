# ---- Project ----
APP := detection
SRC := detection2.c detect_letterinword.c detect_lettergrid.c
OBJ := $(SRC:.c=.o)

# ---- Toolchain ----
CC  := gcc
# Récupère les flags GTK+3 via pkg-config
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   := $(shell pkg-config --libs gtk+-3.0)

# ---- Flags ----
CFLAGS  ?= -O2 -Wall -Wextra -std=c11 $(GTK_CFLAGS)
LDFLAGS ?= $(GTK_LIBS) -lm

# ---- Rules ----
all: $(APP)

$(APP): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Lance le binaire (optionnellement avec une image : make run IMG=level_1_image_1_bw.bmp)
run: $(APP)
	./$(APP) $(IMG)

# Build de debug
debug: CFLAGS += -Og -g -DDEBUG
debug: clean $(APP)

# Build avec sanitizers (utile pour traquer les erreurs mémoire)
sanitize: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1
sanitize: LDFLAGS += -fsanitize=address,undefined
sanitize: clean $(APP)

clean:
	rm -f $(OBJ) $(APP)

.PHONY: all run debug sanitize clean

