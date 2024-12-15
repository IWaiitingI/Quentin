#define _POSIX_C_SOURCE 199309L
#include "backup_manager.h"
#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Fonction utilitaire pour générer un nom de répertoire avec le format "YYYY-MM-DD-hh:mm:ss.sss"
void generate_backup_name(char *buffer, size_t size) {
    struct timespec ts;
    struct tm *timeinfo;


    clock_gettime(CLOCK_REALTIME, &ts); // Récupère l'heure actuelle
    timeinfo = localtime(&ts.tv_sec);  // Convertit en temps local

    snprintf(buffer, size, "%04d-%02d-%02d-%02d:%02d:%02d.%03ld",
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec,
             ts.tv_nsec / 1000000);
}


// Fonction pour calculer le MD5 d'un fichier
int calculate_md5(const char *file_path, unsigned char *md5_digest) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Erreur lors de l'ouverture du fichier pour calculer le MD5");
        return -1;
    }

    MD5_CTX md5_context;
    MD5_Init(&md5_context);

    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        MD5_Update(&md5_context, buffer, bytes_read);
    }

    fclose(file);

    MD5_Final(md5_digest, &md5_context);
    return 0;
}

// Fonction pour récupérer la date de dernière modification
char *get_modification_date(const char *file_path) {
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Erreur lors de la récupération des informations du fichier");
        return NULL;
    }

    // Convertir le temps en une chaîne lisible
    char *date_str = malloc(20);
    if (!date_str) {
        perror("Erreur d'allocation mémoire pour la date");
        return NULL;
    }

    struct tm *mod_time = localtime(&file_stat.st_mtime);
    strftime(date_str, 20, "%Y-%m-%d %H:%M:%S", mod_time);

    return date_str;
}

// Fonction principale pour créer un log_element à partir d'un fichier
log_element *create_log_element(const char *file_path) {
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Erreur lors de la récupération des informations du fichier");
        return NULL;
    }

    // Vérifier si le chemin correspond bien à un fichier régulier
    if (!S_ISREG(file_stat.st_mode)) {
        fprintf(stderr, "Le chemin fourni n'est pas un fichier régulier : %s\n", file_path);
        return NULL;
    }

    // Allouer et initialiser la structure log_element
    log_element *element = malloc(sizeof(log_element));
    if (!element) {
        perror("Erreur d'allocation mémoire pour log_element");
        return NULL;
    }

    element->path = strdup(file_path);
    if (!element->path) {
        perror("Erreur d'allocation mémoire pour le chemin du fichier");
        free(element);
        return NULL;
    }

    // Récupérer la date de dernière modification
    element->date = get_modification_date(file_path);
    if (!element->date) {
        free((char *)element->path);
        free(element);
        return NULL;
    }

    // Calculer le MD5 du fichier
    if (calculate_md5(file_path, element->md5) == -1) {
        free(element->date);
        free((char *)element->path);
        free(element);
        return NULL;
    }

    element->next = NULL;
    element->prev = NULL;

    return element;
}



const char *find_most_recent_folder(log_t *logs) {
    if (!logs || !logs->head) {
        return NULL; // Liste vide ou invalide
    }

    log_element *current = logs->head;
    log_element *most_recent = current;

    while (current) {
        // Comparer les dates (en supposant un format lexicographiquement ordonné)
        if (strcmp(current->date, most_recent->date) > 0) {
            most_recent = current;
        }
        current = current->next;
    }

    return most_recent->path; // Retourner le chemin du dossier le plus récent
}

// Fonction principale pour créer une sauvegarde
void create_backup(const char *source_dir, const char *backup_dir) {
    // Vérifier si le répertoire de sauvegarde existe
    struct stat backup_stat;
    if (stat(backup_dir, &backup_stat) == -1) {
        perror("Le répertoire de sauvegarde spécifié est inaccessible");
        return;
    }

    if (!S_ISDIR(backup_stat.st_mode)) {
        fprintf(stderr, "Le chemin de destination n'est pas un répertoire.\n");
        return;
    }

    // Créer un nouveau répertoire pour la sauvegarde
    char backup_name[64];
    generate_backup_name(backup_name, sizeof(backup_name));

    char full_backup_path[512];
    snprintf(full_backup_path, sizeof(full_backup_path), "%s/%s", backup_dir, backup_name);

    if (mkdir(full_backup_path, 0755) == -1) {
        perror("Erreur lors de la création du répertoire de sauvegarde");
        return;
    }

    // Vérifier s'il existe une sauvegarde précédente
    char previous_backup_path[512];
    snprintf(previous_backup_path, sizeof(previous_backup_path), "%s/.backup_log", backup_dir);

    if (access(previous_backup_path, F_OK) == -1) {
        // Première sauvegarde : copier tout simplement le répertoire source
            // Ouvrir le fichier de log
        char log_path[512];
        snprintf(log_path, sizeof(log_path), "%s/.backup_log", backup_dir);
        FILE *logfile = fopen(log_path, "w");
        if (!logfile) {
            perror("Erreur lors de la création du fichier .backup_log");
            return;
        }

        // Copier le répertoire source
        copy_file(source_dir, full_backup_path);
        // Fermer le fichier de log
        fclose(logfile);

    } else {
        // Sauvegarde incrémentale
        printf("Sauvegarde précédente détectée. Création d'une sauvegarde incrémentale...\n");
        log_t logs = read_backup_log(previous_backup_path);
        
        printf("Dossier: %s",find_most_recent_folder(&logs));
             

    }

    // Créer un log_element pour ce fichier
        log_element *elt = malloc(sizeof(log_element));
        if (!elt) {
            perror("Erreur d'allocation mémoire pour log_element");
            return;
        }


    printf("Sauvegarde terminée : %s\n", full_backup_path);
}


// Fonction permettant d'enregistrer dans fichier le tableau de chunk dédupliqué
void write_backup_file(const char *output_filename, Chunk *chunks, int chunk_count) {
    /*
    */
}


// Fonction implémentant la logique pour la sauvegarde d'un fichier
void backup_file(const char *filename) {
    /*
    */
}


// Fonction permettant la restauration du fichier backup via le tableau de chunk
void write_restored_file(const char *output_filename, Chunk *chunks, int chunk_count) {
    /*
    */
}

// Fonction pour restaurer une sauvegarde
void restore_backup(const char *backup_id, const char *restore_dir) {
    /* @param: backup_id est le chemin vers le répertoire de la sauvegarde que l'on veut restaurer
    *          restore_dir est le répertoire de destination de la restauration
    */
}



int main() {
    create_backup("/home/qricci/Photos/2018-01-24", "/home/qricci/Documents/Test_backup");
    return 0;
}