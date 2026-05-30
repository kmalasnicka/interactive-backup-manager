#include "restore.h"
#include "copy.h"
#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int remove_extra_from_source(const char *source, const char *backup) {
    DIR *dir = opendir(source);
    if (!dir) return 0;

    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char src_path[PATH_MAX];
        char backup_path[PATH_MAX];

        snprintf(src_path, sizeof(src_path), "%s/%s", source, ent->d_name);
        snprintf(backup_path, sizeof(backup_path), "%s/%s", backup, ent->d_name);

        if (!path_exists(backup_path)) {
            if (remove_tree(src_path) == -1) perror("remove extra");
        } else {
            struct stat st_src;
            struct stat st_backup;

            if (lstat(src_path, &st_src) == -1 || lstat(backup_path, &st_backup) == -1) continue;

            if (S_ISDIR(st_src.st_mode) && S_ISDIR(st_backup.st_mode)) {
                remove_extra_from_source(src_path, backup_path);
            }
        }
    }

    closedir(dir);
    return 0;
}

static int restore_dir_recursive(const char *source, const char *backup, const char *source_root, const char *backup_root) {
    struct stat backup_st;

    if (lstat(backup, &backup_st) == -1) return -1;

    if (mkdir_p(source, backup_st.st_mode & 0777) == -1) return -1;

    DIR *dir = opendir(backup);
    if (!dir) return -1;

    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char src_path[PATH_MAX];
        char backup_path[PATH_MAX];

        snprintf(src_path, sizeof(src_path), "%s/%s", source, ent->d_name);
        snprintf(backup_path, sizeof(backup_path), "%s/%s", backup, ent->d_name);

        struct stat st_backup;
        struct stat st_src;

        if (lstat(backup_path, &st_backup) == -1) continue;

        if (S_ISDIR(st_backup.st_mode)) {
            if (path_exists(src_path)) {
                if (lstat(src_path, &st_src) == -1 || !S_ISDIR(st_src.st_mode)) {
                    remove_tree(src_path);
                }
            }

            restore_dir_recursive(src_path, backup_path, source_root, backup_root);
        } else if (S_ISREG(st_backup.st_mode)) {
            if (!path_exists(src_path) || files_are_different(backup_path, src_path)) {
                remove_tree(src_path);

                if (copy_file(backup_path, src_path, st_backup.st_mode & 0777) == -1) {
                    perror("restore file");
                }
            }
        } else if (S_ISLNK(st_backup.st_mode)) {
            if (!path_exists(src_path) || files_are_different(backup_path, src_path)) {
                remove_tree(src_path);

                if (copy_symlink_adjusted(backup_path, src_path, backup_root, source_root) == -1) {
                    perror("restore symlink");
                }
            }
        }
    }

    closedir(dir);
    chmod(source, backup_st.st_mode & 0777);

    return 0;
}

int restore_backup(const char *source, const char *target) {
    char source_abs[PATH_MAX];
    char target_abs[PATH_MAX];
    struct stat st;

    if (realpath_or_absolute(source, source_abs) == -1) {
        fprintf(stderr, "error: cannot resolve source path\n");
        return -1;
    }

    if (realpath(target, target_abs) == NULL) {
        fprintf(stderr, "error: backup target does not exist: %s\n", target);
        return -1;
    }

    if (lstat(target_abs, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "error: target is not a directory: %s\n", target_abs);
        return -1;
    }

    if (restore_dir_recursive(source_abs, target_abs, source_abs, target_abs) == -1) {
        perror("restore");
        return -1;
    }

    remove_extra_from_source(source_abs, target_abs);

    return 0;
}