#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "file_handler.h"
#include "deduplication.h"
#include "backup_manager.h"

// Fonction d'affichage de l'aide
void print_usage() {
    printf("Usage: backup_tool [options]\n");
    printf("Options:\n");
    printf("  --backup <source_dir> <backup_dir>  Crée une nouvelle sauvegarde (complète puis incrémentale)\n");
    printf("  --restore <backup_id> <restore_dir> Restaure une sauvegarde\n");
    printf("  --list-backups <backup_dir>         Liste les sauvegardes disponibles dans le répertoire\n");
    printf("  --help                             Affiche cette aide\n");
}

int main(int argc, char *argv[]) {
    int opt;
    const char *source_dir = NULL;
    const char *backup_dir = NULL;
    const char *backup_id = NULL;
    const char *restore_dir = NULL;

    // Définition des options à parser
    struct option long_options[] = {
        {"backup", no_argument, NULL, 'b'},
        {"restore", no_argument, NULL, 'r'},
        {"list-backups", required_argument, NULL, 'l'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    // Analyse des arguments de ligne de commande
    while ((opt = getopt_long(argc, argv, "b:r:l:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'b': {
                // Sauvegarde : --backup <source_dir> <backup_dir>
                if (optind + 1 < argc) {
                    source_dir = argv[optind++];
                    backup_dir = argv[optind++];
                    printf("Création de la sauvegarde depuis '%s' vers '%s'.\n", source_dir, backup_dir);
                    create_backup(source_dir, backup_dir);
                } else {
                    fprintf(stderr, "Erreur : Vous devez spécifier les répertoires source et de sauvegarde.\n");
                    print_usage();
                    return EXIT_FAILURE;
                }
                break;
            }
            case 'r': {
                // Restauration : --restore <backup_id> <restore_dir>
                if (optind + 1 < argc) {
                    backup_id = argv[optind++];
                    restore_dir = argv[optind++];
                    printf("Restauration de la sauvegarde '%s' vers '%s'.\n", backup_id, restore_dir);
                    restore_backup(backup_id, restore_dir);
                } else {
                    fprintf(stderr, "Erreur : Vous devez spécifier l'ID de sauvegarde et le répertoire de restauration.\n");
                    print_usage();
                    return EXIT_FAILURE;
                }
                break;
            }
            case 'l': {
                // Liste des sauvegardes : --list-backups <backup_dir>
                backup_dir = optarg;
                printf("Liste des sauvegardes dans le répertoire '%s'.\n", backup_dir);
                // Implémentation de la liste des sauvegardes à ajouter ici
                break;
            }
            case 'h': {
                // Affiche l'aide
                print_usage();
                return EXIT_SUCCESS;
            }
            default: {
                print_usage();
                return EXIT_FAILURE;
            }
        }
    }

    // Si aucun argument valide n'a été passé
    if (argc == 1) {
        print_usage();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

