CC = gcc
CFLAGS = -Wall -Wextra -I/home/qricci/Documents/Projetlp25
LDFLAGS = -lssl -lcrypto

SRC = /home/qricci/Documents/Projetlp25/main.c /home/qricci/Documents/Projetlp25/file_handler.c /home/qricci/Documents/Projetlp25/deduplication.c /home/qricci/Documents/Projetlp25/backup_manager.c
OBJ = $(SRC:.c=.o)

# Règle par défaut (tout)
all: lp25_borgbackup

# Compilation de l'exécutable
lp25_borgbackup: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compilation des fichiers .c en .o
%.o: /home/qricci/Documents/Projetlp25/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage
clean:
	rm -f $(OBJ) lp25_borgbackup

