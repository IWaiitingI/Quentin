#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <openssl/md5.h>
#include "file_handler.h"

// Fonction utilitaire pour convertir une chaîne hexadécimale MD5 en tableau d'octets
void hex_to_md5(const char *hex_str, unsigned char *md5_out) {
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sscanf(hex_str + 2 * i, "%2hhx", &md5_out[i]);
    }
}

// Fonction pour lire un fichier .backup_log et retourner une structure de log_t
log_t read_backup_log(const char *logfile) {
    log_t logs = {NULL, NULL};  // Initialiser la liste vide
    FILE *file = fopen(logfile, "r");
    if (file == NULL) {
        perror("Erreur d'ouverture du fichier .backup_log");
        return logs; // Retourner une liste vide si le fichier ne peut pas être ouvert
    }

    char line[1024];  // Buffer pour lire chaque ligne du fichier
    while (fgets(line, sizeof(line), file)) {
        // Parse each line (assuming the format: path|md5|date)
        char *path = strtok(line, "|");
        char *md5_str = strtok(NULL, "|");
        char *date = strtok(NULL, "|");

        if (path && md5_str && date) {
            // Créer un nouvel élément de log
            log_element *new_log = (log_element *)malloc(sizeof(log_element));
            if (new_log == NULL) {
                perror("Erreur d'allocation mémoire pour un nouvel élément de log");
                fclose(file);
                return logs;
            }

            new_log->path = strdup(path);  // Copier le chemin du fichier
            new_log->date = strdup(date);  // Copier la date
            hex_to_md5(md5_str, new_log->md5);  // Convertir le MD5 hexadécimal en tableau d'octets
            new_log->next = NULL;
            new_log->prev = logs.tail;

            // Ajouter le nouvel élément à la fin de la liste
            if (logs.tail) {
                logs.tail->next = new_log;
            } else {
                logs.head = new_log;  // Si la liste est vide, ce sera aussi le premier élément
            }
            logs.tail = new_log;
        }
    }

    fclose(file);  // Fermer le fichier après la lecture
    return logs;
}


// Fonction pour mettre à jour une ligne dans le fichier .backup_log
void update_backup_log(const char *logfile, log_t *logs) {
    // Ouvrir le fichier .backup_log en mode écriture
    FILE *file = fopen(logfile, "w");
    if (!file) {
        perror("Erreur d'ouverture du fichier .backup_log");
        return;
    }

    // Parcourir la liste des logs et écrire chaque élément dans le fichier
    log_element *current = logs->head;
    while (current) {
        write_log_element(current, file);  // Utilisation de la fonction write_log_element
        current = current->next;
    }

    // Fermer le fichier après avoir écrit toutes les lignes
    fclose(file);
}

// Fonction pour écrire un élément de log dans le fichier
void write_log_element(log_element *elt, FILE *logfile) {
    // Format de sortie : "chemin|MD5|date"
    fprintf(logfile, "%s|", elt->path);

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        fprintf(logfile, "%02x", elt->md5[i]);  // Afficher le MD5 en hexadécimal
    }

    fprintf(logfile, "|%s\n", elt->date);  // Afficher la date
}


void list_files(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("Erreur d'ouverture du répertoire");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignorer les répertoires '.' et '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Afficher le nom du fichier ou du répertoire
        printf("%s\n", entry->d_name);
    }

    // Fermer le répertoire
    closedir(dir);
}


int main() {
    // Nom du fichier .backup_log pour le test
    const char *logfile = "backup_log.txt";

    // Lire le fichier .backup_log et obtenir la liste de logs
    log_t logs = read_backup_log(logfile);

    // Afficher les logs avant la mise à jour
    printf("Logs avant mise à jour:\n");
    log_element *current = logs.head;
    while (current) {
        printf("Path: %s\n", current->path);
        printf("MD5: ");
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            printf("%02x", current->md5[i]);
        }
        printf("\nDate: %s\n\n", current->date);
        current = current->next;
    }

    // Mise à jour du log pour un fichier spécifique (ici "/path/to/file1")
    log_element *target = logs.head;
    while (target) {
        if (strcmp(target->path, "/path/to/file1") == 0) {
            // Mettre à jour le MD5 et la date
            const char *new_md5_hex = "e4b8b3db8accc91e2a8b1e13f734be76";
            hex_to_md5(new_md5_hex, target->md5);
            free(target->date);
            target->date = strdup("2024-12-13 15:00:00");
            break;
        }
        target = target->next;
    }

    // Afficher les logs après la mise à jour
    printf("\nLogs après mise à jour:\n");
    current = logs.head;
    while (current) {
        printf("Path: %s\n", current->path);
        printf("MD5: ");
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            printf("%02x", current->md5[i]);
        }
        printf("\nDate: %s\n\n", current->date);
        current = current->next;
    }

    // Mettre à jour le fichier .backup_log avec les nouvelles données
    update_backup_log(logfile, &logs);

    // Libérer la mémoire allouée pour les éléments de log
    current = logs.head;
    while (current) {
        log_element *temp = current;
        current = current->next;
        free(temp->path);
        free(temp->date);
        free(temp);
    }

    return 0;
}