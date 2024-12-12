# Compilateur et options
CC = gcc
CFLAGS = -Wall -Wextra -I./
LDFLAGS = -lssl -lcrypto

# Répertoires et fichiers
SRC_DIR = /home/qricci/Documents/Projetlp25
SRC = $(SRC_DIR)/main.c \
      $(SRC_DIR)/file_handler.c \
      $(SRC_DIR)/deduplication.c \
      $(SRC_DIR)/backup_manager.c
OBJ = $(SRC:.c=.o)
TARGET = lp25_borgbackup

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
