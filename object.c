#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/evp.h>

// ---------------- PROVIDED ----------------

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    unsigned int hash_len;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, id_out->hash, &hash_len);
    EVP_MD_CTX_free(ctx);
}

void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ---------------- FIXED IMPLEMENTATION ----------------

// ensure directories exist
static void ensure_dirs() {
    mkdir(PES_DIR, 0755);
    mkdir(OBJECTS_DIR, 0755);
}

// map enum to string
static const char* type_to_str(ObjectType t) {
    if (t == OBJ_BLOB) return "blob";
    if (t == OBJ_TREE) return "tree";
    if (t == OBJ_COMMIT) return "commit";
    return NULL;
}

// WRITE
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {

    const char *type_str = type_to_str(type);
    if (!type_str) return -1;

    ensure_dirs();

    // build header safely
    char header[64];
    int hdr_len = snprintf(header, sizeof(header), "%s %zu", type_str, len);
    header[hdr_len] = '\0';
    hdr_len += 1;

    // build full object buffer
    size_t total_len = hdr_len + len;
    unsigned char *buffer = malloc(total_len);
    if (!buffer) return -1;

    memcpy(buffer, header, hdr_len);
    memcpy(buffer + hdr_len, data, len);

    // compute hash
    compute_hash(buffer, total_len, id_out);

    // skip if exists
    if (object_exists(id_out)) {
        free(buffer);
        return 0;
    }

    // prepare path
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id_out, hex);

    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "%s/%.2s", OBJECTS_DIR, hex);
    mkdir(dir_path, 0755);

    char final_path[512];
    object_path(id_out, final_path, sizeof(final_path));

    char temp_path[520];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", final_path);

    // write temp file
    int fd = open(temp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        free(buffer);
        return -1;
    }

    ssize_t written = write(fd, buffer, total_len);
    free(buffer);

    if (written != (ssize_t)total_len) {
        close(fd);
        unlink(temp_path);
        return -1;
    }

    fsync(fd);
    close(fd);

    // atomic rename
    if (rename(temp_path, final_path) != 0) {
        unlink(temp_path);
        return -1;
    }

    return 0;
}

// READ
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {

    char path[512];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0) {
        fclose(f);
        return -1;
    }

    unsigned char *buffer = malloc(size);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    if (fread(buffer, 1, size, f) != (size_t)size) {
        free(buffer);
        fclose(f);
        return -1;
    }
    fclose(f);

    // verify hash
    ObjectID check;
    compute_hash(buffer, size, &check);
    if (memcmp(check.hash, id->hash, HASH_SIZE) != 0) {
        free(buffer);
        return -1;
    }

    // split header and data
    unsigned char *null_pos = memchr(buffer, '\0', size);
    if (!null_pos) {
        free(buffer);
        return -1;
    }

    // detect type
    if (strncmp((char*)buffer, "blob ", 5) == 0)
        *type_out = OBJ_BLOB;
    else if (strncmp((char*)buffer, "tree ", 5) == 0)
        *type_out = OBJ_TREE;
    else if (strncmp((char*)buffer, "commit ", 7) == 0)
        *type_out = OBJ_COMMIT;
    else {
        free(buffer);
        return -1;
    }

    // extract data
    unsigned char *data_start = null_pos + 1;
    size_t data_len = size - (data_start - buffer);

    *data_out = malloc(data_len);
    if (!*data_out) {
        free(buffer);
        return -1;
    }

    memcpy(*data_out, data_start, data_len);
    *len_out = data_len;

    free(buffer);
    return 0;
}
