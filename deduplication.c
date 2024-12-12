#include "deduplication.h"
#include "file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <dirent.h>

// Fonction de hachage MD5 pour l'indexation
unsigned int hash_md5(unsigned char *md5) {
    unsigned int hash = 0;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        hash = (hash << 5) + hash + md5[i];
    }
    return hash % HASH_TABLE_SIZE;
}

// Fonction pour calculer le MD5 d'un chunk
void compute_md5(void *data, size_t len, unsigned char *md5_out) {
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, data, len);
    MD5_Final(md5_out, &ctx);
}

// Fonction permettant de chercher un MD5 dans la table de hachage
int find_md5(Md5Entry *hash_table, unsigned char *md5) {
    unsigned int index = hash_md5(md5);
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        int pos = (index + i) % HASH_TABLE_SIZE;
        if (hash_table[pos].index == -1) {
            break; // Case vide
        }
        
        if (memcmp(hash_table[pos].md5, md5, MD5_DIGEST_LENGTH) == 0) {
            return hash_table[pos].index; // MD5 trouvé
        }
    }
    return -1; // MD5 non trouvé
}

// Ajouter un MD5 dans la table de hachage
void add_md5(Md5Entry *hash_table, unsigned char *md5, int index) {
    unsigned int pos = hash_md5(md5);
    while (hash_table[pos].index != -1) {
        pos = (pos + 1) % HASH_TABLE_SIZE;
    }
    memcpy(hash_table[pos].md5, md5, MD5_DIGEST_LENGTH);
    hash_table[pos].index = index;
}

// Fonction pour convertir un fichier non dédupliqué en tableau de chunks
void deduplicate_file(FILE *file, Chunk *chunks, Md5Entry *hash_table) {
    unsigned char buffer[CHUNK_SIZE];
    int chunk_index = 0;

    while (!feof(file)) {
        size_t bytes_read = fread(buffer, 1, CHUNK_SIZE, file);
        if (bytes_read == 0) break;

        unsigned char md5[MD5_DIGEST_LENGTH];
        compute_md5(buffer, bytes_read, md5);

        int existing_index = find_md5(hash_table, md5);
        if (existing_index == -1) {
            // Nouveau chunk, ajout dans la table
            chunks[chunk_index].data = malloc(bytes_read);
            memcpy(chunks[chunk_index].data, buffer, bytes_read);
            memcpy(chunks[chunk_index].md5, md5, MD5_DIGEST_LENGTH);
            add_md5(hash_table, md5, chunk_index);
            chunk_index++;
        }
    }
}

// Fonction permettant de charger un fichier dédupliqué en table de chunks
void undeduplicate_file(FILE *file, Chunk **chunks, int *chunk_count) {
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    int total_chunks = file_size / CHUNK_SIZE + (file_size % CHUNK_SIZE != 0);
    *chunks = (Chunk *)malloc(total_chunks * sizeof(Chunk));
    *chunk_count = 0;

    unsigned char buffer[CHUNK_SIZE];
    while (!feof(file)) {
        size_t bytes_read = fread(buffer, 1, CHUNK_SIZE, file);
        if (bytes_read == 0) break;

        (*chunks)[*chunk_count].data = malloc(bytes_read);
        memcpy((*chunks)[*chunk_count].data, buffer, bytes_read);

        compute_md5(buffer, bytes_read, (*chunks)[*chunk_count].md5);
        (*chunk_count)++;
    }
}
