#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------- LOAD ----------------

int index_load(Index *idx) {
    idx->count = 0;

    FILE *f = fopen(INDEX_FILE, "rb");
    if (!f) return 0;

    while (idx->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &idx->entries[idx->count];

        uint16_t path_len;

        if (fread(&path_len, sizeof(uint16_t), 1, f) != 1)
            break;

        if (fread(e->path, 1, path_len, f) != path_len)
            break;

        e->path[path_len] = '\0';

        if (fread(e->hash.hash, 1, HASH_SIZE, f) != HASH_SIZE)
            break;

        idx->count++;
    }

    fclose(f);
    return 0;
}

// ---------------- SAVE ----------------

int index_save(const Index *idx) {
    FILE *f = fopen(INDEX_FILE, "wb");
    if (!f) return -1;

    for (int i = 0; i < idx->count; i++) {
        const IndexEntry *e = &idx->entries[i];

        uint16_t path_len = strlen(e->path);

        fwrite(&path_len, sizeof(uint16_t), 1, f);
        fwrite(e->path, 1, path_len, f);
        fwrite(e->hash.hash, 1, HASH_SIZE, f);
    }

    fclose(f);
    return 0;
}

// ---------------- ADD ----------------

int index_add(Index *idx, const char *path) {

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size < 0) {
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

    ObjectID oid;
    if (object_write(OBJ_BLOB, buffer, size, &oid) != 0) {
        free(buffer);
        return -1;
    }

    free(buffer);

    // update existing
    for (int i = 0; i < idx->count; i++) {
        if (strcmp(idx->entries[i].path, path) == 0) {
            idx->entries[i].hash = oid;
            return index_save(idx);   // ✅ FIXED
        }
    }

    // add new
    if (idx->count >= MAX_INDEX_ENTRIES)
        return -1;

    strcpy(idx->entries[idx->count].path, path);
    idx->entries[idx->count].hash = oid;
    idx->count++;

    return index_save(idx);   // ✅ FIXED
}

// ---------------- REMOVE ----------------

int index_remove(Index *idx, const char *path) {

    int pos = -1;

    for (int i = 0; i < idx->count; i++) {
        if (strcmp(idx->entries[i].path, path) == 0) {
            pos = i;
            break;
        }
    }

    if (pos == -1)
        return -1;

    for (int i = pos; i < idx->count - 1; i++) {
        idx->entries[i] = idx->entries[i + 1];
    }

    idx->count--;

    return index_save(idx);   // ✅ FIXED
}

// ---------------- STATUS ----------------

int index_status(const Index *idx) {
    for (int i = 0; i < idx->count; i++) {
        printf("%s\n", idx->entries[i].path);
    }
    return 0;
}
