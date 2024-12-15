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
    printf("  --backup <source_dir> <backup_dir>  Crée une sauvegarde du répertoire source dans le répertoire de sauvegarde.\n");
    printf("  --restore <source_backup> <reception_dir> Restaure une sauvegarde. \n ");
    printf("  --help                              Affiche cette aide.\n");
}

int main(int argc, char *argv[]) {
    int option_index = 0;
    int opt;
    const char *source_dir = NULL;
    const char *backup_dir = NULL;
    const char *backup_id = NULL;
    const char *restore_dir = NULL;

    struct option long_options[] = {
        {"backup", required_argument, NULL, 'b'},
        {"restore", required_argument, NULL, 'r'},
        {"list-backups", required_argument, NULL, 'l'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }


    
    while ((opt = getopt_long(argc, argv, "b:r:l:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'b': // --backup
                if (optind < argc) {
                    source_dir = optarg;
                    backup_dir = argv[optind];
                } else {
                    printf("Erreur : --backup nécessite deux arguments <source_dir> <backup_dir>\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'r': // --restore
                if (optind < argc - 1) {
                    backup_id = optarg;
                    restore_dir = argv[optind];
                } else {
                    printf("Erreur : --restore nécessite deux arguments <backup_id> <restore_dir>\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'l': // --list-backups
                backup_dir = optarg;
                break;
            case 'h': // --help
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    // Implémentation de la logique de sauvegarde et restauration
    if (source_dir && backup_dir) {
        printf("Création d'une sauvegarde depuis '%s' vers '%s'...\n", source_dir, backup_dir);
        create_backup(source_dir, backup_dir);
        printf("Sauvegarde terminée avec succès.\n");
    } else if (backup_id && restore_dir) {
        printf("Restauration de la sauvegarde '%s' vers '%s'...\n", backup_id, restore_dir);
        restore_backup(backup_id, restore_dir);
        printf("Restauration terminée avec succès.\n");
    } else if (backup_dir) {
        printf("Liste des sauvegardes dans le répertoire '%s':\n", backup_dir);
        list_backups(backup_dir);
    } else {
        print("Erreur : Aucune option valide n'a été fournie.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
