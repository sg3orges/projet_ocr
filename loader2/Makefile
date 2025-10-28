# -------------------------------------------------------------
# loader/Makefile
# Build system for the loader module (with GdkPixbuf support)
# -------------------------------------------------------------

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 `pkg-config --cflags gdk-pixbuf-2.0`
LDFLAGS = -lm `pkg-config --libs gdk-pixbuf-2.0`

OBJ = loader.o main.o
EXEC = loader_test

# -------------------------------------------------------------
# Default target: build everything
# -------------------------------------------------------------
all: $(EXEC)

$(EXEC): $(OBJ)
	@echo "🔧 Linking $(EXEC)..."
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ) $(LDFLAGS)
	@echo "✅ Build complete!"

%.o: %.c
	@echo "📦 Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------------------------------------------
# Clean target: remove objects, executables, and generated images
# -------------------------------------------------------------
clean:
	@echo "🧹 Cleaning loader directory..."
	rm -f *.o $(EXEC)
	rm -f images/*.png images/*.jpg images/*.jpeg images/*.bmp
	rm -f last_image.txt
	@echo "🗑️  Objects, executables, and images deleted from loader/"

.PHONY: all clean
