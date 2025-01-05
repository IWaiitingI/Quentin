# Compilateur et options
CC = gcc
CFLAGS = -Wall -Wextra -I./
LDFLAGS = -lssl -lcrypto

# Fichiers source explicitement listés
SRC = main.c \
      file_handler.c \
      deduplication.c \
      backup_manager.c \
	  network.c
OBJ = $(SRC:.c=.o)
TARGET = backup

# Règle par défaut
all: $(TARGET)

# Compilation de l'exécutable
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compilation des fichiers .c en .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage
clean:
	rm -f $(OBJ) $(TARGET)

# Nettoyage complet
distclean: clean
	rm -f *~ *.bak
