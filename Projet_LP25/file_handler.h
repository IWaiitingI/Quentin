#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stdio.h>
#include <openssl/md5.h>
#include <time.h>        // Pour time_t
#include <sys/types.h>   // Pour off_t

// Structure pour une ligne du fichier log
typedef struct log_element {
    const char *path; // Chemin du fichier/dossier
    unsigned char md5[MD5_DIGEST_LENGTH]; // MD5 du fichier dédupliqué
    char *date; // Date de dernière modification
    struct log_element *next;
    struct log_element *prev;
} log_element;

// Structure pour une liste de log représentant le contenu du fichier backup_log
typedef struct {
    log_element *head; // Début de la liste de log 
    log_element *tail; // Fin de la liste de log
} log_t;

typedef struct {
    char folder_name[256];
    time_t creation_time;
    off_t folder_size;
} BackupInfo;

log_t read_backup_log(const char *logfile);
void update_backup_log(const char *logfile, log_t *logs);
void write_log_element(log_element *elt, FILE *logfile, const char *backup_log);
void list_files(const char *path);
void copy_single_file(const char *src_file, const char *dest_file);
void copy_directory(const char *src, const char *dest);
void copy_file(const char *src, const char *dest);
BackupInfo *find_backup_logs(const char *directory, int *count);
void print_backup_info(BackupInfo *infos, int count);

#endif // FILE_HANDLER_H
