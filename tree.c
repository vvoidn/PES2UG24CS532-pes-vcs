#include "tree.h"
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ─── Mode Constants ─────────────────────────────────────────

#define MODE_FILE 0100644
#define MODE_EXEC 0100755
#define MODE_DIR  0040000

// ─── PROVIDED ───────────────────────────────────────────────

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode))  return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;

    const unsigned char *ptr = data;
    const unsigned char *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *e = &tree_out->entries[tree_out->count];

        const unsigned char *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        size_t mlen = space - ptr;
        memcpy(mode_str, ptr, mlen);
        e->mode = strtol(mode_str, NULL, 8);

        ptr = space + 1;

        const unsigned char *nullb = memchr(ptr, '\0', end - ptr);
        if (!nullb) return -1;

        size_t nlen = nullb - ptr;
        memcpy(e->name, ptr, nlen);
        e->name[nlen] = '\0';

        ptr = nullb + 1;

        if (ptr + HASH_SIZE > end) return -1;
        memcpy(e->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }

    return 0;
}

static int cmp_entries(const void *a, const void *b) {
    return strcmp(((TreeEntry*)a)->name, ((TreeEntry*)b)->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 300;
    unsigned char *buf = malloc(max_size);
    if (!buf) return -1;

    Tree temp = *tree;
    qsort(temp.entries, temp.count, sizeof(TreeEntry), cmp_entries);

    size_t off = 0;

    for (int i = 0; i < temp.count; i++) {
        TreeEntry *e = &temp.entries[i];

        int w = sprintf((char*)buf + off, "%o %s", e->mode, e->name);
        off += w + 1;

        memcpy(buf + off, e->hash.hash, HASH_SIZE);
        off += HASH_SIZE;
    }

    *data_out = buf;
    *len_out = off;
    return 0;
}

// ─── YOUR IMPLEMENTATION ───────────────────────────────────

// Recursive helper
static int build_tree(IndexEntry *entries, int count, const char *prefix, ObjectID *out_id) {
    Tree tree;
    tree.count = 0;

    for (int i = 0; i < count; i++) {
        const char *path = entries[i].path;

        // skip unrelated paths
        if (prefix && strncmp(path, prefix, strlen(prefix)) != 0)
            continue;

        const char *remaining = prefix ? path + strlen(prefix) : path;

        char *slash = strchr(remaining, '/');

        if (!slash) {
            // file
            TreeEntry *e = &tree.entries[tree.count++];
            e->mode = MODE_FILE;
            strcpy(e->name, remaining);
            e->hash = entries[i].hash;
        } else {
            // directory
            char dirname[256];
            int len = slash - remaining;
            strncpy(dirname, remaining, len);
            dirname[len] = '\0';

            // avoid duplicates
            int found = 0;
            for (int j = 0; j < tree.count; j++) {
                if (strcmp(tree.entries[j].name, dirname) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) continue;

            char new_prefix[512];
            if (prefix)
                snprintf(new_prefix, sizeof(new_prefix), "%s%s/", prefix, dirname);
            else
                snprintf(new_prefix, sizeof(new_prefix), "%s/", dirname);

            ObjectID child_id;
            if (build_tree(entries, count, new_prefix, &child_id) != 0)
                return -1;

            TreeEntry *e = &tree.entries[tree.count++];
            e->mode = MODE_DIR;
            strcpy(e->name, dirname);
            e->hash = child_id;
        }
    }

    void *data;
    size_t len;

    if (tree_serialize(&tree, &data, &len) != 0)
        return -1;

    if (object_write(OBJ_TREE, data, len, out_id) != 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}

// MAIN FUNCTION
int tree_from_index(ObjectID *id_out) {
    Index idx;

    if (index_load(&idx) != 0)
        return -1;

    return build_tree(idx.entries, idx.count, NULL, id_out);
}
