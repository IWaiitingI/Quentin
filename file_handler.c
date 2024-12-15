#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include "file_handler.h"
#include "deduplication.h"



// Fonction permettant de lire un élément du fichier .backup_log
log_t read_backup_log(const char *logfile) {
    log_t logs = {NULL, NULL}; // Initialisation de la structure de logs
    FILE *file = fopen(logfile, "r");

    if (!file) {
        perror("Erreur lors de l'ouverture du fichier .backup_log");
        return logs;
    }

    char line[512]; // Buffer pour lire les lignes du fichier
    while (fgets(line, sizeof(line), file)) {
        // Supprimer le saut de ligne final si présent
        line[strcspn(line, "\n")] = 0;

        // Allouer un nouvel élément de log
        log_element *new_element = (log_element *)malloc(sizeof(log_element));
        if (!new_element) {
            perror("Erreur d'allocation mémoire pour un élément de log");
            fclose(file);
            return logs;
        }

        // Analyse de la ligne au format : YYYY-MM-DD-hh:mm:ss.sss/path/to/file;mtime;md5
        char *backup_date = strtok(line, "/");
        char *path_and_metadata = strtok(NULL, "");

        if (!backup_date || !path_and_metadata) {
            fprintf(stderr, "Format invalide dans .backup_log\n");
            free(new_element);
            continue;
        }

        // Extraire le chemin, mtime et MD5
        char *path = strtok(path_and_metadata, ";");
        char *mtime_str = strtok(NULL, ";");
        char *md5_str = strtok(NULL, ";");

        if (!path || !mtime_str || !md5_str) {
            fprintf(stderr, "Données incomplètes dans une ligne du fichier .backup_log\n");
            free(new_element);
            continue;
        }

        // Remplir les champs de la structure log_element
        new_element->path = strdup(path);
        new_element->date = strdup(backup_date);
        new_element->next = NULL;
        new_element->prev = NULL;

        // Convertir md5_str en tableau de MD5_DIGEST_LENGTH octets
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            sscanf(&md5_str[i * 2], "%2hhx", &new_element->md5[i]);
        }

        // Ajouter le nouvel élément à la liste chaînée
        if (!logs.head) {
            logs.head = logs.tail = new_element;
        } else {
            logs.tail->next = new_element;
            new_element->prev = logs.tail;
            logs.tail = new_element;
        }
    }

    fclose(file);
    return logs;
}


// Fonction permettant de mettre à jour une ligne du fichier .backup_log
void write_log_element(log_element *elt, FILE *logfile) {

    // Vérifier que l'élément et le fichier ne sont pas NULL
    if (!elt || !logfile) {
        fprintf(stderr, "Erreur : l'élément ou le fichier est NULL.\n");
        return;
    }

    // Écrire le chemin du fichier
    fprintf(logfile, "%s;", elt->path);

    // Écrire la date de modification
    fprintf(logfile, "%s;", elt->date);

    // Écrire le hash MD5 en format hexadécimal
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        fprintf(logfile, "%02x", elt->md5[i]);
    }

    // Ajouter une nouvelle ligne
    fprintf(logfile, "\n");
}

#include <sys/types.h>
#include <sys/stat.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void list_files(const char *path) {
    /* Implémenter la logique pour lister les fichiers présents dans un répertoire
     * @param: path - le chemin du répertoire à lister
     */

    // Vérifier si le chemin est valide
    if (!path) {
        fprintf(stderr, "Erreur : chemin NULL fourni à list_files.\n");
        return;
    }

    // Ouvrir le répertoire
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Erreur lors de l'ouverture du répertoire");
        return;
    }

    struct dirent *entry;
    struct stat file_stat;
    char full_path[PATH_MAX];

    // Parcourir le contenu du répertoire
    while ((entry = readdir(dir)) != NULL) {
        // Ignorer les répertoires "." et ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construire le chemin complet
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Récupérer les informations sur le fichier ou le répertoire
        if (stat(full_path, &file_stat) == -1) {
            perror("Erreur lors de la récupération des informations sur le fichier");
            continue;
        }

        // Vérifier si c'est un fichier ou un répertoire
        if (S_ISDIR(file_stat.st_mode)) {
            printf("Répertoire : %s\n", full_path);

            // Appeler récursivement pour les sous-répertoires
            list_files(full_path);
        } else if (S_ISREG(file_stat.st_mode)) {
            printf("Fichier : %s\n", full_path);
        }
    }

    // Fermer le répertoire
    closedir(dir);
}


// Fonction pour copier un fichier


void copy_file(const char *src, const char *dest);

// Fonction pour copier un fichier
void copy_single_file(const char *src_file, const char *dest_file) {
    FILE *src_fp = fopen(src_file, "rb");
    if (!src_fp) {
        perror("Error opening source file");
        return;
    }

    FILE *dest_fp = fopen(dest_file, "wb");
    if (!dest_fp) {
        perror("Error opening destination file");
        fclose(src_fp);
        return;
    }
    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_fp)) > 0) {
        fwrite(buffer, 1, bytes, dest_fp);
    }

    fclose(src_fp);
    fclose(dest_fp);
}

// Fonction pour copier un dossier et son contenu
void copy_directory(const char *src, const char *dest) {
    DIR *dir = opendir(src);
    if (!dir) {
        perror("Error opening source directory");
        return;
    }


    struct dirent *entry;
    char src_path[1024], dest_path[1024];

    while ((entry = readdir(dir)) != NULL) {
        // Ignorer les entrées "." et ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

        struct stat statbuf;
        if (stat(src_path, &statbuf) == -1) {
            perror("Error getting file info");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Si c'est un répertoire, appeler récursivement
            copy_directory(src_path, dest_path);
        } else if (S_ISREG(statbuf.st_mode)) {
            // Si c'est un fichier, copier le fichier
            copy_single_file(src_path, dest_path);
        }
    }

    closedir(dir);
}

// Fonction principale pour copier un dossier source dans un dossier destination
void copy_file(const char *src, const char *dest) {
    struct stat statbuf;
    if (stat(src, &statbuf) == -1) {
        perror("Error getting source file/directory info");
        return;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        // Si c'est un répertoire, copier le répertoire
        copy_directory(src, dest);
    } else if (S_ISREG(statbuf.st_mode)) {
        // Si c'est un fichier, copier le fichier
        copy_single_file(src, dest);
    } else {
        fprintf(stderr, "Source is neither a file nor a directory\n");
    }
} 




