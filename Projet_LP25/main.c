#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "file_handler.h"
#include "deduplication.h"
#include "backup_manager.h"

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  --backup <source_dir> <backup_dir>      Crée une sauvegarde du répertoire source dans le répertoire de sauvegarde.\n");
    printf("  --restore <source_backup> <restore_dir> Restaure une sauvegarde.\n");
    printf("  --list-backups <backup_dir> [--s-serveur <adresse> --s-port <port>] Liste les sauvegardes locales ou distantes.\n");
    printf("  --help                                  Affiche cette aide.\n");
}

int main(int argc, char *argv[]) {
    int option_index = 0;
    int opt;
    const char *source_dir = NULL;
    const char *backup_directory = NULL;
    const char *backup_dir = NULL;
    const char *backup_id = NULL;
    const char *restore_dir = NULL;
    const char *server_address = NULL;
    int server_port = -1;

    struct option long_options[] = {
        {"backup", required_argument, NULL, 'b'},
        {"restore", required_argument, NULL, 'r'},
        {"list-backups", required_argument, NULL, 'l'},
        {"s-serveur", required_argument, NULL, 's'},
        {"s-port", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    while ((opt = getopt_long(argc, argv, "b:r:l:s:p:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'b': // --backup
                if (optind < argc) {
                    source_dir = optarg;
                    backup_directory = argv[optind];
                    create_backup(source_dir, backup_directory);
                } else {
                    printf("Erreur : --backup nécessite deux arguments <source_dir> <backup_dir>\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'r': // --restore
                if (optind < argc) {
                    backup_id = optarg;
                    restore_dir = argv[optind];
                    restore_backup(backup_id, restore_dir);
                } else {
                    printf("Erreur : --restore nécessite deux arguments <backup_id> <restore_dir>\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'l': // --list-backups
                backup_dir = optarg;
                break;
            case 's': // --s-serveur
                server_address = optarg;
                break;
            case 'p': // --s-port
                server_port = atoi(optarg);
                break;
            case 'h': // --help
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Gestion de l'option --list-backups après la boucle
    if (backup_dir) {
        if (server_address && server_port > 0) {
            int count = 0;
            BackupInfo *infos = find_backup_logs_remote(server_address, server_port, backup_dir, &count);

        } else {
            // Liste les sauvegardes locales
            const char *directory = backup_dir;
            int count = 0;
            BackupInfo *infos = find_backup_logs(directory, &count);
            printf("Nombre de dossiers de backup: %d\n\n", count);
            print_backup_info(infos, count);
        }
    }  
    return EXIT_SUCCESS;
}
