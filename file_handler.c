#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include "file_handler.h"
#include "deduplication.h"

// Fonction permettant de lire un élément du fichier .backup_log
log_t read_backup_log(const char *logfile) {
    log_t log_list = {0};
    FILE *file = fopen(logfile, "r");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s\n", logfile);
        return log_list; // Retourne une liste vide
    }

    log_element *prev = NULL;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        log_element *new_elt = malloc(sizeof(log_element));
        if (!new_elt) {
            fprintf(stderr, "Erreur : Allocation mémoire\n");
            break;
        }

        char md5_hex[MD5_DIGEST_LENGTH * 2 + 1];
        new_elt->path = malloc(strlen(line));
        new_elt->date = malloc(20);

        sscanf(line, "%s %s %s", new_elt->path, md5_hex, new_elt->date);

        // Convertir le MD5 hexadécimal en binaire
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            sscanf(&md5_hex[i * 2], "%2hhx", &new_elt->md5[i]);
        }

        new_elt->next = NULL;
        new_elt->prev = prev;

        if (!log_list.head) {
            log_list.head = new_elt;
        } else {
            prev->next = new_elt;
        }
        log_list.tail = new_elt;
        prev = new_elt;
    }

    fclose(file);
    return log_list;
}


// Fonction permettant de mettre à jour une ligne du fichier .backup_log
void update_backup_log(const char *logfile, log_t *logs) {
    FILE *file = fopen(logfile, "w");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s\n", logfile);
        return;
    }

    log_element *current = logs->head;
    while (current) {
        // Convertir le MD5 en hexadécimal
        char md5_hex[MD5_DIGEST_LENGTH * 2 + 1] = {0};
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            sprintf(&md5_hex[i * 2], "%02x", current->md5[i]);
        }

        fprintf(file, "%s %s %s\n", current->path, md5_hex, current->date);
        current = current->next;
    }

    fclose(file);
}


void write_log_element(log_element *elt, FILE *logfile) {
    if (!elt || !logfile) {
        fprintf(stderr, "Erreur : Paramètres invalides pour écrire dans le fichier .backup_log\n");
        return;
    }

    char md5_hex[MD5_DIGEST_LENGTH * 2 + 1] = {0};
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&md5_hex[i * 2], "%02x", elt->md5[i]);
    }

    fprintf(logfile, "%s %s %s\n", elt->path, md5_hex, elt->date);
}


void list_files(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Erreur : Impossible d'accéder au répertoire %s\n", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
}
