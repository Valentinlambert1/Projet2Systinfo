#include "lib_tar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h> 


int block_size = 512;

/**
 * Checks whether the archive is valid.
 *
 * Each non-null header of a valid archive has:
 *  - a magic value of "ustar" and a null,
 *  - a version value of "00" and no null,
 *  - a correct checksum
 *
 * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 *
 * @return a zero or positive value if the archive is valid, representing the number of non-null headers in the archive,
 *         -1 if the archive contains a header with an invalid magic value,
 *         -2 if the archive contains a header with an invalid version value,
 *         -3 if the archive contains a header with an invalid checksum value
 */
int check_archive(int tar_fd) {

    int num = 0;
    struct posix_header header;
    
    while(read(tar_fd, &header, block_size) == block_size){
        if (header.name[0] == '\0'){
            break;
        }
        num++;
        long file_size = strtol(header.size, NULL, 8);

        printf("Filename: %s\n", header.name);
        printf("Size octal: %ld\n", file_size);

        if (strcmp(header.magic, "ustar") != 0){
            return -1;
        }
        if (header.version[0] != '0' && header.version[1] != '0'){
            return -2;
        }

        unsigned int checksum = 0;

        unsigned char *bytes = (unsigned char *) &header;
        
        for (int i = 0; i < block_size; i++){
            if (i >= 148 && i < 156){
                checksum += 32;
            }
            else{
                checksum += bytes[i];
            }
        }

        if (checksum != strtol(header.chksum, NULL, 8)){
            return -3;
        }
        int nbr_blocks = (file_size + block_size - 1) / block_size;
        lseek(tar_fd, nbr_blocks * block_size, SEEK_CUR);
    }
    return num;
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
int exists(int tar_fd, char *path) {
    lseek(tar_fd, 0, SEEK_SET);
    struct posix_header header;

    while (read(tar_fd, &header, block_size) == block_size){
        if (header.name[0] == '\0'){
            break;
        }
        if (strcmp(header.name, path) == 0){
            return 1;
        }
        long file_size = strtol(header.size, NULL, 8);

        int nbr_blocks = (file_size + block_size - 1) / block_size;
        lseek(tar_fd, nbr_blocks * block_size, SEEK_CUR);
    }
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 */
int is_dir(int tar_fd, char *path) {
    //TO DO
    lseek(tar_fd, 0, SEEK_SET); 
    struct posix_header header;

    while (read(tar_fd, &header, 512) == 512) {
        if (header.name[0] == '\0') break;

        
        size_t h_len = strlen(header.name);
        char h_name[101];
        strcpy(h_name, header.name);
        if (h_name[h_len-1] == '/') h_name[h_len-1] = '\0';

        
        char p_clean[101];
        strcpy(p_clean, path);
        size_t p_len = strlen(p_clean);
        if (p_len > 0 && p_clean[p_len-1] == '/') p_clean[p_len-1] = '\0';

        if (strcmp(h_name, p_clean) == 0 && header.typeflag == DIRTYPE) {
            return 1;
        }

        long file_size = TAR_INT(header.size);
        lseek(tar_fd, ((file_size + 511) / 512) * 512, SEEK_CUR);
    }
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path) {
    lseek(tar_fd, 0, SEEK_SET);
    struct posix_header header;
    
    while (read(tar_fd, &header, block_size) == block_size){
        if (header.name[0] == '\0'){
            break;
        }
        if (strcmp(header.name, path) == 0 && (header.typeflag == '0' || header.typeflag == '\0')){
            return 1;
        }
        long file_size = strtol(header.size, NULL, 8);

        int nbr_blocks = (file_size + block_size - 1) / block_size;
        lseek(tar_fd, nbr_blocks * block_size, SEEK_CUR);
    }
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path) {
    struct posix_header header;
    

    while (read(tar_fd, &header, block_size) == block_size){
        if (header.name[0] == '\0'){
            break;
        }
        if (strcmp(header.name, path) == 0 && header.typeflag == '2'){
            return 1;
        }
        long file_size = strtol(header.size, NULL, 8);

        int nbr_blocks = (file_size + block_size - 1) / block_size;
        lseek(tar_fd, nbr_blocks * block_size, SEEK_CUR);
    }
    return 0;
}

/**
 * Lists the entries at a given path in the archive.
 * list() does *not* recurse into the directories listed at the given path.
 * If the path is NULL, it lists the entries at the root of the archive.
 *
 * Example:
 *  dir/          list(..., "dir/", ...) lists "dir/a", "dir/b", "dir/c/" and "dir/e/"
 *   ├── a
 *   ├── b
 *   ├── c/
 *   │   └── d
 *   └── e/
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry path.
 * @param no_entries An in-out argument.
 *                   The caller set it to the number of entries in `entries`.
 *                   The callee set it to the number of entries listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         1 in case of success,
 *         -1 in case of error.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries) {
    struct posix_header header;
    char current_path[100];
    size_t count = 0;
    size_t max_out = *no_entries;

    if (path == NULL || strlen(path) == 0) {
        strcpy(current_path, "");
    } else {
        strncpy(current_path, path, 100);
    }

    int resolved = 0;
    while (!resolved) {
        resolved = 1;
        if (lseek(tar_fd, 0, SEEK_SET) == (off_t)-1) return -1;
        while (read(tar_fd, &header, 512) == 512) {
            if (header.name[0] == '\0') break;
            if (strcmp(header.name, current_path) == 0 && header.typeflag == SYMTYPE) {
                strncpy(current_path, header.linkname, 100);
                resolved = 0;
                break;
            }
            long size = TAR_INT(header.size);
            lseek(tar_fd, ((size + 511) / 512) * 512, SEEK_CUR);
        }
    }

    if (lseek(tar_fd, 0, SEEK_SET) == (off_t)-1) return -1;
    int path_exists = (strlen(current_path) == 0); 
    size_t t_len = strlen(current_path);

    while (read(tar_fd, &header, 512) == 512) {
        if (header.name[0] == '\0') break;

        if (t_len > 0 && strcmp(header.name, current_path) == 0) {
            path_exists = 1;
        }

        if (t_len == 0 || strncmp(header.name, current_path, t_len) == 0) {
            if (t_len == 0 || strcmp(header.name, current_path) != 0) {
                char *rel = header.name + t_len;
                if (rel[0] == '/') rel++;

                if (rel[0] != '\0') {
                    char *slash = strchr(rel, '/');
                    if (slash == NULL || slash[1] == '\0') {
                        if (count < max_out) {
                            strncpy(entries[count], header.name, 100);
                            count++;
                        }
                        path_exists = 1;
                    }
                }
            }
        }
        long size = TAR_INT(header.size);
        lseek(tar_fd, ((size + 511) / 512) * 512, SEEK_CUR);
    }

    *no_entries = count;
    return path_exists ? 1 : 0;
}

/**
 * Adds a file at the end of the archive, at the archive's root level.
 * The archive's metadata must be updated accordingly.
 * For the file header, only the name, size, typeflag, magic value (to "ustar"), version value (to "00") and checksum fields need to be correctly set.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param filename The name of the file to add. If an entry already exists with the same name, the file is not written, and the function returns -1.
 * @param src A source buffer containing the file content to add.
 * @param len The length of the source buffer.
 *
 * @return 0 if the file was added successfully,
 *         -1 if the archive already contains an entry at the given path,
 *         -2 if an error occurred
 */
int add_file(int tar_fd, char *filename, uint8_t *src, size_t len) {
    if (exists(tar_fd, filename)) {
        return -1;
    }

    lseek(tar_fd, 0, SEEK_SET);
    struct posix_header header;
    while (read(tar_fd, &header, 512) == 512) {
        if (header.name[0] == '\0') {
            lseek(tar_fd, -512, SEEK_CUR);
            break;
        }
        long size = strtol(header.size, NULL, 8);
        lseek(tar_fd, ((size + 511) / 512) * 512, SEEK_CUR);
    }

    struct posix_header new_header;
    memset(&new_header, 0, sizeof(struct posix_header));

    strncpy(new_header.name, filename, 100);
    sprintf(new_header.mode, "%07o", 0644);
    sprintf(new_header.size, "%011lo", (unsigned long)len);
    new_header.typeflag = REGTYPE;
    memcpy(new_header.magic, TMAGIC, TMAGLEN);
    memcpy(new_header.version, TVERSION, TVERSLEN);

    memset(new_header.chksum, ' ', 8);
    unsigned int sum = 0;
    unsigned char *p = (unsigned char *)&new_header;
    for (int i = 0; i < 512; i++) {
        sum += p[i];
    }
    sprintf(new_header.chksum, "%06o", sum);

    if (write(tar_fd, &new_header, 512) != 512) return -2;

    if (write(tar_fd, src, len) != (ssize_t)len) return -2;

    size_t padding = (512 - (len % 512)) % 512;
    if (padding > 0) {
        uint8_t pad[512];
        memset(pad, 0, padding);
        if (write(tar_fd, pad, padding) != (ssize_t)padding) return -2;
    }

    uint8_t end_blocks[1024];
    memset(end_blocks, 0, 1024);
    if (write(tar_fd, end_blocks, 1024) != 1024) return -2;

    return 0;
}
