#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "lib_tar.h"

/**
 * You are free to use this file to write tests for your implementation
 */

void debug_dump(const uint8_t *bytes, size_t len) {
    for (int i = 0; i < len;) {
        printf("%04x:  ", (int) i);

        for (int j = 0; j < 16 && i + j < len; j++) {
            printf("%02x ", bytes[i + j]);
        }
        printf("\t");
        for (int j = 0; j < 16 && i < len; j++, i++) {
            printf("%c ", bytes[i]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s tar_file\n", argv[0]);
        return -1;
    }

    int fd = open(argv[1], O_RDWR); // O_RDWR pour permettre add_file plus tard
    if (fd == -1) {
        perror("open(tar_file)");
        return -1;
    }

    // 1. Test de validité
    int ret = check_archive(fd);
    printf("Validation\n");
    printf("check_archive: %d (attendu: >0)\n\n", ret);

    // 2. Test d'existence et type
    printf("Tests Existence\n");
    char *test_path = "test_plus_complexe/folder1/";
    printf("Exists '%s': %d\n", test_path, exists(fd, test_path));
    printf("Is Dir '%s': %d\n", test_path, is_dir(fd, test_path));
    printf("Is File '%s': %d\n", test_path, is_file(fd, test_path));

    // 3. Test de list()
    printf("Test List (Root)\n");
    size_t no_entries = 10;
    char **entries = malloc(no_entries * sizeof(char *));
    for (int i = 0; i < no_entries; i++) entries[i] = malloc(100 * sizeof(char));

    int list_ret = list(fd, NULL, entries, &no_entries);
    printf("list(root) returned: %d, count: %zu\n", list_ret, no_entries);
    for (size_t i = 0; i < no_entries; i++) {
        printf("  -> %s\n", entries[i]);
    }

    printf("\nTest List (Subdir)\n");
    size_t no_entries_sub = 10;
    char *subdir = "test_plus_complexe/folder1/";
    int list_sub_ret = list(fd, subdir, entries, &no_entries_sub);
    printf("list(%s) returned: %d, count: %zu\n", subdir, list_sub_ret, no_entries_sub);
    for (size_t i = 0; i < no_entries_sub; i++) {
        printf("  -> %s\n", entries[i]);
    }

    // Libération mémoire
    for (int i = 0; i < 10; i++) free(entries[i]);
    free(entries);
    close(fd);
    return 0;
}