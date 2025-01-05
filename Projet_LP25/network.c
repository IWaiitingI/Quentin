#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "file_handler.h"

#define BUFFER_SIZE 1024

// Fonction pour récupérer les informations sur les sauvegardes depuis un serveur
void find_backup_logs_remote(const char *server_address, int server_port, const char *backup_dir) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Création du socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erreur lors de la création du socket");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        perror("Adresse du serveur invalide");
        close(sockfd);
        return;
    }

    // Connexion au serveur
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur lors de la connexion au serveur");
        close(sockfd);
        return;
    }

    // Envoyer le chemin du répertoire au serveur
    send(sockfd, backup_dir, strlen(backup_dir), 0);

    // Recevoir et afficher les données ligne par ligne
    printf("Logs des sauvegardes reçus du serveur :\n");
    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Terminer la chaîne reçue
        if (strcmp(buffer, "FIN") == 0) break; // Fin de transmission
        printf("%s", buffer);
    }

    if (bytes_received < 0) {
        perror("Erreur lors de la réception des données");
    }

    close(sockfd);
}






