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
    printf("  --help                              Affiche cette aide.\n");
}

int main(int argc, char *argv[]) {
    int option_index = 0;
    const char *source_dir = NULL;
    const char *backup_dir = NULL;

    static struct option long_options[] = {
        {"backup", required_argument, 0, 'b'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int opt;
    while ((opt = getopt_long(argc, argv, "b:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'b':
                if (optind < argc - 1) {
                    source_dir = optarg;
                    backup_dir = argv[optind];
                } else {
                    fprintf(stderr, "Erreur : Vous devez spécifier un répertoire source et un répertoire de sauvegarde.\n");
                    return EXIT_FAILURE;
                }
                break;

            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;

            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (source_dir && backup_dir) {
        printf("Démarrage de la sauvegarde...\n");
        create_backup(source_dir, backup_dir);
        printf("Sauvegarde terminée avec succès.\n");
    } else {
        fprintf(stderr, "Erreur : Arguments invalides. Utilisez --help pour plus d'informations.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
