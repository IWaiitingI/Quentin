#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>

#define CHUNK_SIZE 4096
#define MD5_DIGEST_LENGTH 16

// Structure pour un chunk
typedef struct {
    unsigned char md5[MD5_DIGEST_LENGTH]; // MD5 du chunk
    void *data; // Données du chunk
} Chunk;

// Fonction pour calculer le MD5 d'un chunk
void compute_md5(void *data, size_t len, unsigned char *md5_out) {
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, data, len);
    MD5_Final(md5_out, &md5_ctx);
}

// Fonction pour créer un fichier dédupliqué
void create_test_file(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Erreur lors de l'ouverture du fichier pour écriture");
        return;
    }

    char buffer[CHUNK_SIZE] = "Hello, world! This is a test chunk of data!";
    unsigned char md5[MD5_DIGEST_LENGTH];
    
    // Ajouter plusieurs chunks au fichier
    for (int i = 0; i < 5; i++) {
        compute_md5(buffer, sizeof(buffer), md5);
        
        // Ecrire le MD5 dans le fichier
        fwrite(md5, 1, MD5_DIGEST_LENGTH, file);
        
        // Ecrire la taille des données
        size_t data_size = sizeof(buffer);
        fwrite(&data_size, sizeof(size_t), 1, file);
        
        // Ecrire les données du chunk
        fwrite(buffer, 1, sizeof(buffer), file);
    }

    fclose(file);
}

// Fonction pour restaurer les chunks à partir du fichier dédupliqué
void undeduplicate_file(FILE *file, Chunk **chunks, int *chunk_count) {
    *chunk_count = 0;
    unsigned char buffer[CHUNK_SIZE];
    int chunk_index = 0;
    size_t lecture_chunk;

    // Compter le nombre de chunks dans le fichier
    while ((lecture_chunk = fread(buffer, sizeof(unsigned long), CHUNK_SIZE, file)) > 0) {
        *chunk_count++;
    }
    if (feof(file)) {
        return;
    } else if (ferror(file)) {
        printf("Problème dans la lecture du fichier.\n");
        return;
    }
    printf("Nombre de chunks : %d\n", *chunk_count);

    // Réinitialiser le fichier pour la lecture des chunks
    fseek(file, 0, SEEK_SET);
    *chunks = malloc(*chunk_count * sizeof(Chunk));

    for (int i = 0; i < *chunk_count; i++) {
        fread((*chunks)[i].md5, 1, MD5_DIGEST_LENGTH, file);
        
        size_t data_size;
        fread(&data_size, sizeof(size_t), 1, file);
        
        (*chunks)[i].data = malloc(data_size);
        fread((*chunks)[i].data, 1, data_size, file);
    }
}

// Fonction pour afficher les MD5 des chunks
void print_chunks(Chunk *chunks, int chunk_count) {
    for (int i = 0; i < chunk_count; i++) {
        printf("Chunk %d MD5: ", i);
        for (int j = 0; j < MD5_DIGEST_LENGTH; j++) {
            printf("%02x", chunks[i].md5[j]);
        }
        printf("\n");
    }
}

int main() {
    // Créer un fichier de test contenant des chunks dédupliqués
    create_test_file("deduplicated_file.bin");

    // Ouvrir le fichier dédupliqué pour tester la restauration
    FILE *file = fopen("deduplicated_file.bin", "rb");
    if (file == NULL) {
        printf("Erreur d'ouverture du fichier.\n");
        return -1;
    }

    // Tableau de chunks pour stocker les chunks restaurés
    Chunk *chunks = NULL;
    int chunk_count = 0;

    // Appeler la fonction pour restaurer les chunks
    undeduplicate_file(file, &chunks, &chunk_count);

    // Afficher les MD5 des chunks restaurés
    print_chunks(chunks, chunk_count);

    // Libérer la mémoire allouée pour les chunks
    for (int i = 0; i < chunk_count; i++) {
        free(chunks[i].data);
    }
    free(chunks);

    // Fermer le fichier
    fclose(file);

    return 0;
}
