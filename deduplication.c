#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>


#include "deduplication.h"
#include "file_handler.h"
#include <dirent.h>


#define CHUNK_SIZE 4096
#define HASH_TABLE_SIZE 1000
#define MD5_DIGEST_LENGTH 16

// Structure pour un chunk
typedef struct {
    unsigned char md5[MD5_DIGEST_LENGTH]; // MD5 du chunk
    void *data; // Données du chunk
} Chunk;

// Table de hachage pour stocker les MD5 et leurs index
typedef struct {
    unsigned char md5[MD5_DIGEST_LENGTH];
    int index;
} Md5Entry;

// Fonction de hachage MD5 pour l'indexation dans la table de hachage
unsigned int hash_md5(unsigned char *md5) {
    unsigned int hash = 0;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        hash = (hash << 5) + hash + md5[i];
    }
    return hash % HASH_TABLE_SIZE;
}

// Fonction pour calculer le MD5 d'un chunk
void compute_md5(void *data, size_t len, unsigned char *md5_out) {
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, data, len);
    MD5_Final(md5_out, &md5_ctx);
}

// Fonction pour ajouter un MD5 dans la table de hachage
void add_md5(Md5Entry *hash_table, unsigned char *md5, int index) {
    unsigned int hash = hash_md5(md5);
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        unsigned int probe = (hash + i) % HASH_TABLE_SIZE;
        if (hash_table[probe].index == -1) {
            memcpy(hash_table[probe].md5, md5, MD5_DIGEST_LENGTH);
            hash_table[probe].index = index;
            return;
        }
    }
}

// Fonction pour chercher un MD5 dans la table de hachage
int find_md5(Md5Entry *hash_table, unsigned char *md5) {
    unsigned int index = hash_md5(md5);
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        unsigned int probe = (index + i) % HASH_TABLE_SIZE;
        if (hash_table[probe].index == -1) {
            continue;
        }
        if (memcmp(hash_table[probe].md5, md5, MD5_DIGEST_LENGTH) == 0) {
            return hash_table[probe].index;
        }
    }
    return -1;
}

// Fonction pour dédupliquer un fichier et l'ajouter en chunks
void deduplicate_file(FILE *file, Chunk *chunks, Md5Entry *hash_table) {
    size_t bytes_read;
    unsigned char buffer[CHUNK_SIZE];
    unsigned char md5[MD5_DIGEST_LENGTH];
    int chunk_index = 0;
    
    // Lecture du fichier en chunks
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        // Calculer le MD5 du chunk
        compute_md5(buffer, bytes_read, md5);
        
        // Chercher si ce MD5 est déjà dans la table de hachage
        int index_in_hash = find_md5(hash_table, md5);
        
        if (index_in_hash == -1) {
            // Si le MD5 n'existe pas encore, l'ajouter à la table de hachage et au tableau de chunks
            add_md5(hash_table, md5, chunk_index);
            
            // Allouer et stocker les données du chunk
            chunks[chunk_index].data = malloc(bytes_read);
            memcpy(chunks[chunk_index].data, buffer, bytes_read);
            memcpy(chunks[chunk_index].md5, md5, MD5_DIGEST_LENGTH);
            
            // Incrémenter l'index du chunk
            chunk_index++;
        } else {
            // Si le MD5 existe déjà, ignorer le chunk et ne pas ajouter de doublons
            printf("Chunk duplicata trouvé : index %d, MD5 déjà présent.\n", index_in_hash);
        }
    }
    
    // Terminer la lecture du fichier
    printf("Déduplication terminée, %d chunks traités.\n", chunk_index);
}

void undeduplicate_file(FILE *file, Chunk **chunks, int *chunk_count) {
    *chunk_count = 0;
    unsigned char buffer[CHUNK_SIZE];
    int chunk_index = 0;
    size_t lecture_chunk;


    //compte du nombre de chunks
    while ((lecture_chunk = fread(buffer, sizeof(unsigned long), CHUNK_SIZE, file)) > 0) {
        *chunk_count++;
    }
    if (feof(file)){
        return;
    }
    else if (ferror(file)){
        printf("probleme dans la lecture du fichier");
        return;
    }
    printf("nombre de chunks : %d\n",*chunk_count);


    *chunks = malloc(*chunk_count * sizeof(Chunk));
    for (int i = 0; i < *chunk_count; i++) {
        fread((*chunks)[i].md5, 1, MD5_DIGEST_LENGTH, file);


        size_t data_size;
        fread(&data_size, sizeof(size_t), 1, file);


        (*chunks)[i].data = malloc(data_size);
        fread((*chunks)[i].data, 1, data_size, file);
    }
}

// Fonction pour afficher la table de hachage
void print_hash_table(Md5Entry *hash_table) {
    printf("\nTable de hachage des MD5 et leurs indices:\n");
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (hash_table[i].index != -1) {
            printf("Index: %d, MD5: ", hash_table[i].index);
            for (int j = 0; j < MD5_DIGEST_LENGTH; j++) {
                printf("%02x", hash_table[i].md5[j]);
            }
            printf("\n");
        }
    }
}

int main() {
    // Ouvrir le fichier à dédupliquer
    FILE *file = fopen("Hello.txt", "rb");
    if (file == NULL) {
        printf("Erreur d'ouverture du fichier.\n");
        return -1;
    }
    
    // Initialiser la table de hachage avec des index invalides (-1)
    Md5Entry hash_table[HASH_TABLE_SIZE];
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_table[i].index = -1;
    }
    
    // Tableau pour stocker les chunks
    Chunk chunks[100]; // Limité à 100 chunks pour ce test
    
    // Appeler la fonction pour dédupliquer le fichier
    deduplicate_file(file, chunks, hash_table);
    
    // Afficher la table de hachage après déduplication
    print_hash_table(hash_table);
    
    // Fermer le fichier
    fclose(file);
    
    return 0;
}
