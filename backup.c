#include "backup.h"
#include "copy.h"
#include "monitor.h"
#include "utils.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int validate_backup_paths(const char *source, const char *target, char source_abs[PATH_MAX], char target_abs[PATH_MAX]) {
    struct stat st;

    if (realpath(source, source_abs) == NULL) {
        fprintf(stderr, "error: source does not exist: %s\n", source);
        return -1;
    }

    if (lstat(source_abs, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "error: source is not a directory: %s\n", source);
        return -1;
    }

    if (realpath_or_absolute(target, target_abs) == -1) {
        fprintf(stderr, "error: cannot resolve target path: %s\n", target);
        return -1;
    }

    if (paths_equal(source_abs, target_abs)) {
        fprintf(stderr, "error: source and target cannot be the same directory\n");
        return -1;
    }

    if (is_strict_subpath(source_abs, target_abs)) {
        fprintf(stderr, "error: target cannot be inside source directory\n");
        return -1;
    }

    return 0;
}

int run_backup(const char *source, const char *target) {
    char source_abs[PATH_MAX];
    char target_abs[PATH_MAX];
    struct stat st;

    if (validate_backup_paths(source, target, source_abs, target_abs) == -1) return -1;

    if (lstat(target_abs, &st) == -1) {
        if (mkdir_p(target_abs, 0755) == -1) {
            perror("mkdir_p target");
            return -1;
        }
    } else {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "error: target exists but is not a directory: %s\n", target_abs);
            return -1;
        }

        int empty = is_empty_dir(target_abs);
        if (empty != 1) {
            if (empty == 0) fprintf(stderr, "error: target directory must be empty: %s\n", target_abs);
            else perror("opendir target");
            return -1;
        }
    }

    if (copy_dir(source_abs, target_abs, source_abs, target_abs) == -1) {
        perror("initial copy");
        return -1;
    }

    return start_monitoring(source_abs, target_abs);
}