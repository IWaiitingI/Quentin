#include "backup_manager.h"
#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 256

// Fonction pour créer une nouvelle sauvegarde complète puis incrémentale
void create_backup(const char *source_dir, const char *backup_dir) {
    DIR *dir = opendir(source_dir);
    if (!dir) {
        fprintf(stderr, "Erreur : Impossible d'accéder au répertoire source %s\n", source_dir);
        return;
    }

    // Charger les logs existants
    char log_file_path[MAX_PATH_LENGTH];
    snprintf(log_file_path, sizeof(log_file_path), "%s/.backup_log", backup_dir);
    log_t logs = read_backup_log(log_file_path);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", source_dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            fprintf(stderr, "Erreur : Impossible de lire les informations du fichier %s\n", full_path);
            continue;
        }

        if (S_ISREG(st.st_mode)) { // Si c'est un fichier
            backup_file(full_path);
        }
    }

    // Mettre à jour les logs après la sauvegarde
    update_backup_log(log_file_path, &logs);

    closedir(dir);
}

// Fonction permettant d'enregistrer dans un fichier le tableau de chunk dédupliqué
void write_backup_file(const char *output_filename, Chunk *chunks, int chunk_count) {
    FILE *file = fopen(output_filename, "wb");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'écrire dans le fichier de sauvegarde %s\n", output_filename);
        return;
    }

    for (int i = 0; i < chunk_count; i++) {
        size_t chunk_size = strlen(chunks[i].data);
        fwrite(&chunk_size, sizeof(size_t), 1, file); // Écrire la taille du chunk
        fwrite(chunks[i].data, 1, chunk_size, file); // Écrire les données du chunk
    }

    fclose(file);
}

// Fonction implémentant la logique pour la sauvegarde d'un fichier
void backup_file(const char *filename) {
    // Charger les logs existants
    log_t logs = {0};
    log_element *current = logs.head;

    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s\n", filename);
        return;
    }

    Chunk chunks[HASH_TABLE_SIZE] = {0};
    Md5Entry hash_table[HASH_TABLE_SIZE] = {0};

    // Initialiser la table de hachage
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_table[i].index = -1;
    }

    // Dédupliquer les données du fichier
    deduplicate_file(file, chunks, hash_table);

    // Vérifier si des modifications ont eu lieu
    unsigned char md5[MD5_DIGEST_LENGTH];
    fseek(file, 0, SEEK_SET);
    compute_md5(filename, strlen(filename), md5);

    int found = 0;
    while (current) {
        if (memcmp(current->md5, md5, MD5_DIGEST_LENGTH) == 0) {
            found = 1;
            break; // Fichier déjà sauvegardé
        }
        current = current->next;
    }

    fclose(file);

    if (!found) {
        // Si le fichier n'existe pas encore dans les logs, ajouter une nouvelle sauvegarde
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char date[20];
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", t);

        log_element *new_log = malloc(sizeof(log_element));
        new_log->path = strdup(filename);
        memcpy(new_log->md5, md5, MD5_DIGEST_LENGTH);
        new_log->date = strdup(date);
        new_log->next = NULL;
        new_log->prev = logs.tail;

        if (!logs.head) {
            logs.head = new_log;
        } else {
            logs.tail->next = new_log;
        }
        logs.tail = new_log;

        // Sauvegarder les chunks dédupliqués
        char backup_filename[MAX_PATH_LENGTH];
        snprintf(backup_filename, sizeof(backup_filename), "backup_%s", filename);
        write_backup_file(backup_filename, chunks, HASH_TABLE_SIZE);
    }
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