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

void process_directory(const char *directory_path, FILE *logs, const char *backup_dir) {
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

        // Construit le chemin complet du fichier/dossier
        char file_to_process[1024];
        snprintf(file_to_process, sizeof(file_to_process), "%s/%s", directory_path, entry->d_name);

        // Vérifie si l'entrée est un répertoire
        struct stat statbuf;
        if (stat(file_to_process, &statbuf) == -1) {
            perror("Erreur lors de la récupération des informations du fichier");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Si c'est un répertoire, appel récursif sur le sous-dossier
            process_directory(file_to_process, logs, backup_dir);
        } else if (S_ISREG(statbuf.st_mode)) {
            // Si c'est un fichier, traitement du fichier
            log_element test_element;
            create_log_element_from_file(file_to_process, &test_element);

            // Écriture dans le fichier de log
            FILE *logfile = fopen(logs, "a");
            if (!logfile) {
                perror("Erreur lors de l'ouverture du fichier de log");
                free(test_element.path);
                free(test_element.date);
                continue;
            }
            write_log_element(&test_element, logfile, backup_dir);
            fclose(logfile);

            // Libère la mémoire allouée pour les éléments de log
            free(test_element.path);
            free(test_element.date);
        }
    }

    // Ferme le dossier
    closedir(dir);
}





// Fonction pour supprimer un fichier
int remove(const char *filepath) {
    // Utilise la fonction standard de C pour supprimer un fichier
    if (unlink(filepath) == 0) {
        printf("Fichier supprimé : %s\n", filepath);
        return 0;
    } else {
        perror("Erreur lors de la suppression du fichier");
        return -1;
    }
}

// Fonction pour vérifier si un fichier existe dans un répertoire
int file_exists_in_dir(const char *directory, const char *filename) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", directory, filename);
    struct stat buffer;
    return (stat(filepath, &buffer) == 0);
}




int files_are_different(const char *file1, const char *file2) {
    struct stat stat1, stat2;

    // Obtenir les informations des fichiers
    if (stat(file1, &stat1) == -1 || stat(file2, &stat2) == -1) {
        perror("Erreur lors de l'accès aux fichiers pour comparaison");
        return 1; // Considérer les fichiers comme différents si les stat échouent
    }

    // Comparer les tailles des fichiers
    if (stat1.st_size != stat2.st_size) {
        return 1; // Les fichiers ont des tailles différentes
    }

    // Ouvrir les fichiers
    FILE *f1 = fopen(file1, "rb");
    FILE *f2 = fopen(file2, "rb");

    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return 1; // Considérer les fichiers comme différents si l'un des deux ne peut être ouvert
    }

    // Lire et comparer le contenu des fichiers par blocs
    char buffer1[4096], buffer2[4096];
    size_t size1, size2;

    do {
        size1 = fread(buffer1, 1, sizeof(buffer1), f1);
        size2 = fread(buffer2, 1, sizeof(buffer2), f2);

        if (size1 != size2 || memcmp(buffer1, buffer2, size1) != 0) {
            fclose(f1);
            fclose(f2);
            return 1; // Les fichiers sont différents
        }
    } while (size1 > 0 && size2 > 0);

    fclose(f1);
    fclose(f2);
    return 0; // Les fichiers sont identiques
}

#include <unistd.h>

void sync_directories(const char *dir1, const char *dir2) {
    DIR *dir;
    struct dirent *entry;

    // Vérifier les fichiers et sous-dossiers du premier répertoire (dir1)
    dir = opendir(dir1);
    if (dir == NULL) {
        perror("Erreur lors de l'ouverture du premier répertoire");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char filepath1[1024];
        snprintf(filepath1, sizeof(filepath1), "%s/%s", dir1, entry->d_name);

        char filepath2[1024];
        snprintf(filepath2, sizeof(filepath2), "%s/%s", dir2, entry->d_name);

        struct stat statbuf1, statbuf2;

        if (stat(filepath1, &statbuf1) == -1) {
            perror("Erreur lors de l'obtention des informations du fichier source");
            continue;
        }

        if (S_ISDIR(statbuf1.st_mode)) {
            // Si c'est un répertoire
            if (stat(filepath2, &statbuf2) == -1) {
                // Répertoire inexistant dans dir2 -> le supprimer
                printf("Suppression du répertoire : %s\n", filepath1);
                sync_directories(filepath1, filepath2);
                rmdir(filepath1); // Supprime le répertoire une fois vide
            } else {
                sync_directories(filepath1, filepath2);
            }
        } else if (S_ISREG(statbuf1.st_mode)) {
            // Si c'est un fichier
            if (stat(filepath2, &statbuf2) == -1) {
                // Fichier inexistant dans dir2 -> le supprimer
                printf("Suppression du fichier : %s\n", filepath1);
                unlink(filepath1); // Supprime le fichier
            } else if (files_are_different(filepath1, filepath2)) {
                // Vérifier si le fichier est un lien dur
                if (statbuf1.st_nlink > 1) {
                    // Le fichier a des liens durs, créer une copie unique
                    printf("Le fichier %s a des liens durs, création d'une copie unique\n", filepath1);

                    char temp_filepath[1024];
                    snprintf(temp_filepath, sizeof(temp_filepath), "%s.tmp", filepath1);

                    copy_file(filepath1, temp_filepath);  // Copie temporaire
                    rename(temp_filepath, filepath1);     // Remplacement du fichier original
                }

                // Si le fichier est différent, le mettre à jour
                printf("Mise à jour du fichier : %s -> %s\n", filepath2, filepath1);
                copy_file(filepath2, filepath1);
            }
        }
    }
    closedir(dir);

    // Vérifier les fichiers et sous-dossiers du deuxième répertoire (dir2)
    dir = opendir(dir2);
    if (dir == NULL) {
        perror("Erreur lors de l'ouverture du deuxième répertoire");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char filepath1[1024];
        snprintf(filepath1, sizeof(filepath1), "%s/%s", dir1, entry->d_name);

        char filepath2[1024];
        snprintf(filepath2, sizeof(filepath2), "%s/%s", dir2, entry->d_name);

        struct stat statbuf1;

        if (stat(filepath2, &statbuf1) == -1) {
            perror("Erreur lors de l'obtention des informations du fichier");
            continue;
        }

        if (S_ISDIR(statbuf1.st_mode)) {
            // Si c'est un répertoire
            if (stat(filepath1, &statbuf1) == -1) {
                // Le répertoire n'existe pas dans dir1, le copier
                printf("Ajout du répertoire : %s -> %s\n", filepath2, filepath1);
                mkdir(filepath1, 0755);
                sync_directories(filepath1, filepath2);
            }
        } else if (S_ISREG(statbuf1.st_mode)) {
            // Si c'est un fichier
            if (stat(filepath1, &statbuf1) == -1) {
                // Le fichier n'existe pas dans dir1, le copier
                printf("Ajout du fichier : %s -> %s\n", filepath2, filepath1);
                copy_file(filepath2, filepath1);
            } else if (files_are_different(filepath1, filepath2)) {
                // Vérifier si le fichier est un lien dur
                if (statbuf1.st_nlink > 1) {

                    char temp_filepath[1024];
                    snprintf(temp_filepath, sizeof(temp_filepath), "%s.tmp", filepath1);

                    copy_file(filepath1, temp_filepath);  // Copie temporaire
                    rename(temp_filepath, filepath1);     // Remplacement du fichier original
                }

                // Si le fichier est différent, le mettre à jour
                printf("Mise à jour du fichier : %s -> %s\n", filepath2, filepath1);
                copy_file(filepath2, filepath1);
            }
        }
    }
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
        FILE *logfile = fopen(log_path, "a");
        if (!logfile) {
            perror("Erreur lors de la création du fichier .backup_log");
            return;
        }

        // Copier le répertoire source
        copy_file(source_dir, full_backup_path);

        process_directory(full_backup_path, log_path, backup_dir);
        

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

            // Copier les fichiers avec des liens durs
            copy_with_hard_links(most_recent_folder_path, full_backup_path);

            char log_path[512];
            snprintf(log_path, sizeof(log_path), "%s/.backup_log", backup_dir);

            sync_directories(full_backup_path, source_dir);

            FILE *log1 = fopen(log_path, "w");
            fclose(log1);

            process_directory(full_backup_path, log_path, backup_dir );

            
        } else {
            printf("Aucun dossier précédent trouvé.\n");
        }
    }

    char log_dest_path[512];
    snprintf(log_dest_path, sizeof(log_dest_path), "%s/.backup_log", full_backup_path);
    char back[500]; // Déclare une chaîne suffisamment grande pour contenir le chemin complet.
    snprintf(back, sizeof(back), "%s/.backup_log", backup_dir); // Formate la chaîne avec le répertoire de sauvegarde.
    copy_single_file(back, log_dest_path); // Appelle la fonction pour copier le fichier.


    printf("Sauvegarde terminée : %s\n", full_backup_path);
}






// Function to join paths safely
void join_paths(const char *base, const char *name, char *result, size_t size) {
    snprintf(result, size, "%s/%s", base, name);
}

#include <errno.h>
#include <limits.h> // Inclure pour PATH_MAX si disponible

#ifndef PATH_MAX
#define PATH_MAX 4096 // Définit PATH_MAX si non défini par le système
#endif


// Recursive function to copy source to destination using hard links
int copy_with_hard_links(const char *source, const char *destination) {
    DIR *dir;
    struct dirent *entry;
    struct stat entry_stat;
    char source_path[PATH_MAX];
    char destination_path[PATH_MAX];

    // Open the source directory
    dir = opendir(source);
    if (dir == NULL) {
        perror("Failed to open source directory");
        return -1;
    }

    // Ensure the destination directory exists
    if (mkdir(destination, 0755) == -1 && errno != EEXIST) {
        perror("Failed to create destination directory");
        closedir(dir);
        return -1;
    }

    // Iterate over all entries in the source directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." and files that start with a "."
        if (entry->d_name[0] == '.' || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full paths for source and destination
        join_paths(source, entry->d_name, source_path, sizeof(source_path));
        join_paths(destination, entry->d_name, destination_path, sizeof(destination_path));

        // Get the entry's metadata
        if (lstat(source_path, &entry_stat) == -1) {
            perror("Failed to stat source entry");
            closedir(dir);
            return -1;
        }

        // Handle directories recursively
        if (S_ISDIR(entry_stat.st_mode)) {
            if (copy_with_hard_links(source_path, destination_path) == -1) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(entry_stat.st_mode)) {
            // Create a hard link for regular files
            if (link(source_path, destination_path) == -1) {
                perror("Failed to create hard link");
                closedir(dir);
                return -1;
            }
        } else {
            fprintf(stderr, "Skipping unsupported file type: %s\n", source_path);
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
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h> // Pour dirname

void restore_backup(const char *backup_id, const char *restore_dir) {
    // Vérifier si le répertoire de destination est valide
    struct stat restore_stat;

    if (stat(restore_dir, &restore_stat) == -1) {
        perror("Le répertoire de restauration spécifié est inaccessible");
        return;
    }

    if (!S_ISDIR(restore_stat.st_mode)) {
        fprintf(stderr, "Le chemin de destination n'est pas un répertoire.\n");
        return;
    }

    // Construire le chemin du fichier `.backup_log` pour la sauvegarde spécifiée
    char backup_log_path[512];
    snprintf(backup_log_path, sizeof(backup_log_path), "%s/.backup_log", backup_id);

    // Lire le fichier .backup_log
    log_t logs = read_backup_log(backup_log_path);
    if (!logs.head) {
        fprintf(stderr, "Aucun fichier à restaurer trouvé dans .backup_log\n");
        return;
    }

    // Parcourir les éléments du journal pour restaurer les fichiers
    log_element *current = logs.head;
    while (current) {
        // Construire le chemin source (dans le répertoire de sauvegarde)
        char source_path[512];
        snprintf(source_path, sizeof(source_path), "%s/%s", backup_id, current->path);

        // Construire le chemin de destination (dans le répertoire de restauration)
        char dest_path[512];
        snprintf(dest_path, sizeof(dest_path), "%s/%s", restore_dir, current->path);

        // Extraire le répertoire parent du fichier destination
        char dest_dir[512];
        snprintf(dest_dir, sizeof(dest_dir), "%s", dest_path);
        dirname(dest_dir);

        // Vérifier si le répertoire parent existe, sinon le créer
        struct stat dir_stat;
        if (stat(dest_dir, &dir_stat) == -1) {
            printf("Création du répertoire : %s\n", dest_dir);
            if (mkdir(dest_dir, 0755) == -1) {
                perror("Erreur lors de la création du répertoire");
                continue; // Passer au fichier suivant en cas d'erreur
            }
        }

        // Vérifier si le fichier destination existe déjà
        if (access(dest_path, F_OK) == 0) {  // Le fichier existe
            // Comparer les fichiers uniquement si le fichier destination existe
            if (files_are_different(source_path, dest_path)) {
                printf("Mise à jour de %s -> %s\n", source_path, dest_path);
                copy_single_file(source_path, dest_path);
            } else {
                printf("Le fichier %s est déjà à jour.\n", dest_path);
            }
        } else {
            // Le fichier destination n'existe pas, copier directement
            printf("Copie initiale de %s -> %s\n", source_path, dest_path);
            copy_single_file(source_path, dest_path);
        }

        current = current->next;
    }

    // Libérer la mémoire allouée pour le journal
    free_backup_log(&logs);
}




void free_backup_log(log_t *logs) {
    log_element *current = logs->head;
    while (current) {
        log_element *next = current->next;
        free(current->path);
        free(current->date);
        free(current);
        current = next;
    }
    logs->head = logs->tail = NULL;
}




