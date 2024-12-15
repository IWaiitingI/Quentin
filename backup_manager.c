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
char *calculate_md5(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Erreur lors de l'ouverture du fichier");
        return NULL;
    }

    MD5_CTX md5_ctx;
    unsigned char data[1024];
    unsigned char hash[MD5_DIGEST_LENGTH];
    char *md5_string = malloc(MD5_DIGEST_LENGTH * 2 + 1); // 2 caractères par octet + 1 pour '\0'

    if (!md5_string) {
        fprintf(stderr, "Erreur d'allocation mémoire\n");
        fclose(file);
        return NULL;
    }

    MD5_Init(&md5_ctx);

    // Lecture du fichier par blocs et mise à jour du calcul MD5
    size_t bytes_read;
    while ((bytes_read = fread(data, 1, sizeof(data), file)) > 0) {
        MD5_Update(&md5_ctx, data, bytes_read);
    }

    if (ferror(file)) {
        perror("Erreur lors de la lecture du fichier");
        free(md5_string);
        fclose(file);
        return NULL;
    }

    MD5_Final(hash, &md5_ctx);
    fclose(file);

    // Convertir le hash en une chaîne hexadécimale
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&md5_string[i * 2], "%02x", hash[i]);
    }

    return md5_string;
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

    // Retourner le nom du dossier correspondant à la date la plus récente
    // Extraire la première partie de la date (avant le premier point-virgule)
    char *folder_name = strdup(most_recent->date);
    if (!folder_name) {
        return NULL; // Erreur d'allocation mémoire
    }

    // Trouver le premier point-virgule et ajouter un terminateur
    char *semicolon_pos = strchr(folder_name, ';');
    if (semicolon_pos) {
        *semicolon_pos = '\0'; // Terminer la chaîne avant le point-virgule
    }

    return folder_name; // Retourner le nom du dossier (ex: "2024-12-15-16:37:24.967")
}

void process_directory(const char *directory_path) {
    DIR *dir;
    struct dirent *entry;

    // Ouvre le dossier
    dir = opendir(directory_path);
    if (dir == NULL) {
        perror("Erreur lors de l'ouverture du dossier");
        return;
    }

    // Parcours les fichiers du dossier
    while ((entry = readdir(dir)) != NULL) {
        // Ignore les fichiers "." et ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construit le chemin complet du fichier
        char file_to_process[1024];
        snprintf(file_to_process, sizeof(file_to_process), "%s/%s", directory_path, entry->d_name);

        // Appelle la fonction create_log_element_from_file pour chaque fichier
        log_element test_element;
        create_log_element_from_file(file_to_process, &test_element);
        FILE *logfile = fopen("/home/qricci/Documents/Test_backup/.backup_log", "a");
        if (!logfile) {
            perror("Erreur lors de l'ouverture du fichier de log");
            free(test_element.path);
            free(test_element.date);
        return 1;
    }
        write_log_element(&test_element, logfile);
    }

    // Ferme le dossier
    closedir(dir);
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

        process_directory(full_backup_path);
        // Fermer le fichier de log
        fclose(logfile);

    } else {
        // Sauvegarde incrémentale
        printf("Sauvegarde précédente détectée. Création d'une sauvegarde incrémentale...\n");
        log_t logs = read_backup_log(previous_backup_path);
        
        // Trouver le dossier le plus récent
        const char *most_recent_folder = find_most_recent_folder(&logs);
        if (most_recent_folder) {
            // Créer le chemin complet du dossier le plus récent
            char most_recent_folder_path[512];
            snprintf(most_recent_folder_path, sizeof(most_recent_folder_path), "%s/%s", backup_dir, most_recent_folder);
            
            printf("Dossier le plus récent : %s\n", most_recent_folder_path);

            copy_with_hard_links(most_recent_folder_path, full_backup_path);
        } else {
            printf("Aucun dossier précédent trouvé.\n");
        }
    }

    printf("Sauvegarde terminée : %s\n", full_backup_path);
}




char* construct_folder_path(const char *base_path, const char *folder_name) {
    // Allouer de la mémoire pour le chemin complet (base_path + '/' + folder_name + '\0')
    size_t path_length = strlen(base_path) + strlen(folder_name) + 2; // 2 pour le séparateur '/' et '\0'
    char *full_path = (char*)malloc(path_length);
    
    if (!full_path) {
        perror("Erreur d'allocation mémoire");
        return NULL;
    }

    // Construire le chemin complet
    snprintf(full_path, path_length, "%s/%s", base_path, folder_name);
    
    return full_path; // Retourner le chemin complet
}


// Fonction créer chemins complets
void join_paths(const char *base, const char *name, char *result, size_t size) {
    snprintf(result, size, "%s/%s", base, name);
}

#include <errno.h>
#include <limits.h> // Inclure PATH_MAX si disponible

#ifndef PATH_MAX
#define PATH_MAX 4096 // Définit PATH_MAX si non défini par le système
#endif


// Fonction recursive creant une copie vers la destianation liens durs
int copy_with_hard_links(const char *source, const char *destination) {
    DIR *dir;
    struct dirent *entry;
    struct stat entry_stat;
    char source_path[PATH_MAX];
    char destination_path[PATH_MAX];

    // Ouvre le repertoire source
    dir = opendir(source);
    if (dir == NULL) {
        perror("Erreur d'ouverture du repertoire source");
        return -1;
    }

    // Verifier la sortie du repertoire source
    if (mkdir(destination, 0755) == -1 && errno != EEXIST) {
        perror("Erreur de creation du repertoire de destination");
        closedir(dir);
        return -1;
    }

    // Iteration sur toutes les entrees du repertoire source
    while ((entry = readdir(dir)) != NULL) {
        // Suivant "." et ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construction les chemins complets pour source et destination
        join_paths(source, entry->d_name, source_path, sizeof(source_path));
        join_paths(destination, entry->d_name, destination_path, sizeof(destination_path));

        // Obtenir les metadata de l'entree
        if (lstat(source_path, &entry_stat) == -1) {
            perror("Failed to stat source entry");
            closedir(dir);
            return -1;
        }

        // Gestion des repertoires en recursif
        if (S_ISDIR(entry_stat.st_mode)) {
            if (copy_with_hard_links(source_path, destination_path) == -1) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(entry_stat.st_mode)) {
            // Cree un lien dur pour les fichiers
            if (link(source_path, destination_path) == -1) {
                perror("Erreur de creation de lien dur");
                closedir(dir);
                return -1;
            }
        } else {
            fprintf(stderr, "Ignoration du type de fichier non pris en charge %s\n", source_path);
        }
    }

    closedir(dir);
    return 0;
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

