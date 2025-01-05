#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "file_handler.h"

#define PORT 8080
#define BUFFER_SIZE 1024

// Fonction qui calcule la taille totale d'un dossier
off_t calculate_folder_size(const char *path);

// Fonction principale pour gérer un client
void handle_client(int client_socket) {
    char directory[BUFFER_SIZE] = {0};
    int valread = read(client_socket, directory, sizeof(directory) - 1);

    if (valread <= 0) {
        perror("Erreur lors de la lecture des données du client");
        return;
    }

    directory[valread] = '\0'; // Terminer la chaîne reçue
    printf("Chemin reçu du client : %s\n", directory);

    // Appeler la fonction find_backup_logs pour le chemin reçu
    int count = 0;
    BackupInfo *results = find_backup_logs(directory, &count);
    
    print_backup_info_remote( results,count, client_socket);

    if (results == NULL) {
        perror("Aucun résultat trouvé ou erreur dans find_backup_logs");
        write(client_socket, "ERREUR", strlen("ERREUR"));
        return;
    }

    // Côté serveur : envoyer les données sous forme de chaîne formatée
    char buffer[BUFFER_SIZE];
    for (int i = 0; i < count; i++) {
        snprintf(buffer, sizeof(buffer), "%s|%ld|%ld\n",
                results[i].folder_name,
                results[i].creation_time,
                results[i].folder_size);
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("Erreur lors de l'envoi des données au client");
            break;
        }
    }


    // Envoyer un signal de fin
    send(client_socket, "FIN", strlen("FIN"), 0);

    // Libérer la mémoire allouée
    free(results);
    printf("Résultats envoyés au client.\n");
}



void print_backup_info_remote(BackupInfo *infos, int count, int client_socket) {
    if (!infos || count == 0) {
        const char *message = "Aucune sauvegarde trouvée.\n";
        send(client_socket, message, strlen(message), 0);
        return;
    }

    char buffer[BUFFER_SIZE];
    for (int i = 0; i < count; i++) {
        char creation_time_str[64];
        struct tm *tm_info = localtime(&infos[i].creation_time);
        strftime(creation_time_str, sizeof(creation_time_str), "%Y-%m-%d %H:%M:%S", tm_info);

        snprintf(buffer, sizeof(buffer),
                 "Sauvegarde %d :\n  Nom du dossier : %s\n  Date de création : %s\n  Taille du log : %ld octets\n\n",
                 i + 1, infos[i].folder_name, creation_time_str, infos[i].folder_size);

        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("Erreur lors de l'envoi des données au client");
            return;
        }
    }

    // Envoyer un signal de fin
    const char *fin = "FIN";
    send(client_socket, fin, strlen(fin), 0);
}


// Fonction principale du serveur
int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Créer le socket serveur
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // Configurer le socket pour réutiliser l'adresse
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Erreur lors de la configuration du socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Définir l'adresse et le port du serveur
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Lier le socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Erreur lors du bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Mettre le serveur en écoute
    if (listen(server_fd, 3) < 0) {
        perror("Erreur lors du listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur en écoute sur le port %d...\n", PORT);

    // Boucle principale pour accepter et gérer les clients
    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Erreur lors de l'acceptation d'une connexion");
            continue;
        }

        printf("Nouvelle connexion acceptée.\n");
        handle_client(client_socket);
        close(client_socket);
        printf("Connexion fermée.\n");
    }

    close(server_fd);
    return 0;
}


