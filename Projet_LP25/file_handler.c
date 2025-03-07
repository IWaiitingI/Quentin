#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
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





int file_md5(const char *file_path, unsigned char *md5_result) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Erreur lors de l'ouverture du fichier pour MD5");
        return 1;
    }

    MD5_CTX md5_context;
    MD5_Init(&md5_context);

    unsigned char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        MD5_Update(&md5_context, buffer, bytes_read);
    }

    MD5_Final(md5_result, &md5_context);
    fclose(file);
    return 0;
}

// Fonction pour récupérer les informations d'un fichier et remplir un log_element
int create_log_element_from_file(const char *file_path, log_element *element) {
    struct stat file_stat;

    // Récupérer les métadonnées du fichier
    if (stat(file_path, &file_stat) != 0) {
        perror("Erreur lors de la récupération des métadonnées du fichier");
        return 1;
    }

    // Remplir le champ path
    element->path = strdup(file_path);

    // Formater la date de modification
    char date_buffer[64];
    struct tm *mod_time = localtime(&file_stat.st_mtime);
    if (!mod_time) {
        perror("Erreur lors de la conversion de la date");
        free(element->path);
        return 1;
    }
    strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d-%H:%M:%S", mod_time);
    element->date = strdup(date_buffer);

    // Calculer le MD5
    if (file_md5(file_path, element->md5) != 0) {
        fprintf(stderr, "Erreur lors du calcul du MD5\n");
        free(element->path);
        free(element->date);
        return 1;
    }

    element->next = NULL;
    element->prev = NULL;

    return 0;
}



void get_file_mtime(const char *file_path, char *mtime_buffer, size_t buffer_size) {
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Erreur lors de la récupération du mtime");
        strncpy(mtime_buffer, "unknown", buffer_size);
        return;
    }

    struct tm *timeinfo = localtime(&file_stat.st_mtime);
    strftime(mtime_buffer, buffer_size, "%Y-%m-%d-%H:%M:%S", timeinfo);
}



const char *get_relative_path(const char *full_path, const char *logfile) {
    const char *log_dir = strstr(logfile, ".backup_log");
    if (!log_dir) {
        fprintf(stderr, "Erreur : .backup_log introuvable dans le chemin fourni\n");
        return full_path; // Si le chemin de .backup_log n'est pas trouvé, retourner le chemin complet
    }

    size_t log_dir_len = log_dir - logfile; // Longueur du chemin jusqu'à .backup_log
    const char *base_path = full_path + log_dir_len; // Construire le chemin relatif

    // Vérifier si le chemin relatif commence par un "/"
    if (base_path[0] == '/')
        base_path++;

    return base_path;
}




// Fonction permettant de mettre à jour une ligne du fichier .backup_log
void write_log_element(log_element *elt, FILE *logfile, const char *backup_log) {
    // Extraire le chemin relatif du fichier
    char back[500];
    snprintf(back, sizeof(back), "%s/.backup_log", backup_log);
    const char *relative_path = get_relative_path(elt->path, back);

    // Écrire le chemin relatif dans le fichier de log
    fprintf(logfile, "%s;", relative_path);

    // Écrire la date de dernière modification (mtime)
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
#include <errno.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif


// Fonction pour calculer récursivement la taille d'un dossier
off_t calculate_folder_size(const char *folder_path) {
    struct dirent *entry;
    struct stat file_stat;
    DIR *dp = opendir(folder_path);

    if (!dp) {
        perror("Erreur lors de l'ouverture du répertoire");
        return 0;
    }

    off_t total_size = 0;

    while ((entry = readdir(dp)) != NULL) {
        // Ignorer les entrées spéciales "." et ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construire le chemin complet pour chaque élément
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, entry->d_name);

        // Récupérer les informations sur l'élément
        if (stat(full_path, &file_stat) == 0) {
            if (S_ISDIR(file_stat.st_mode)) {
                // Si c'est un répertoire, appeler récursivement la fonction
                total_size += calculate_folder_size(full_path);
            } else if (S_ISREG(file_stat.st_mode)) {
                // Si c'est un fichier, ajouter sa taille
                total_size += file_stat.st_size;
            }
        }
    }

    closedir(dp);
    return total_size;
}

BackupInfo *find_backup_logs(const char *directory, int *count) {
    struct dirent *entry;
    struct stat file_stat, log_stat;
    DIR *dp = opendir(directory);

    if (!dp) {
        perror("Erreur lors de l'ouverture du répertoire");
        return NULL;
    }

    BackupInfo *results = NULL;
    *count = 0;

    while ((entry = readdir(dp)) != NULL) {
        // Ignorer les entrées spéciales "." et ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construire le chemin complet pour chaque élément
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);

        // Vérifier si c'est un répertoire
        if (stat(full_path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
            // Construire le chemin vers le fichier `.backup_log`
            char log_path[512];
            snprintf(log_path, sizeof(log_path), "%s/.backup_log", full_path);

            // Vérifier si le fichier `.backup_log` existe
            if (stat(log_path, &log_stat) == 0 && S_ISREG(log_stat.st_mode)) {
                // Allouer de la mémoire pour stocker les résultats
                results = realloc(results, (*count + 1) * sizeof(BackupInfo));

                // Remplir les informations sur le dossier et le fichier
                strncpy(results[*count].folder_name, entry->d_name, sizeof(results[*count].folder_name) - 1);
                results[*count].creation_time = log_stat.st_ctime;

                // Calculer la taille totale du dossier
                results[*count].folder_size = calculate_folder_size(full_path);

                (*count)++;
            }
        }
    }

    closedir(dp);
    return results;
}


void print_backup_info(BackupInfo *infos, int count) {
    for (int i = 0; i < count; i++) {
        char creation_time[64];
        struct tm *tm_info = localtime(&infos[i].creation_time);
        strftime(creation_time, sizeof(creation_time), "%Y-%m-%d %H:%M:%S", tm_info);

        printf("Dossier: %s\n", infos[i].folder_name);
        printf("Date de création: %s\n", creation_time);
        printf("Taille de la sauvegarde: %ld octets\n\n", infos[i].folder_size);
    }
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

    // Créer le répertoire de destination si nécessaire
    if (mkdir(dest, 0755) == -1 && errno != EEXIST) {
        perror("Error creating destination directory");
        closedir(dir);
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






