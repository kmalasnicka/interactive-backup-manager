#include "copy.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static int map_link_target(const char *old_value, char new_value[PATH_MAX], const char *from_root, const char *to_root) {
    if (old_value[0] != '/') {
        strncpy(new_value, old_value, PATH_MAX);
        new_value[PATH_MAX - 1] = '\0';
        return 0;
    }

    char old_abs[PATH_MAX];
    char from_abs[PATH_MAX];
    char to_abs[PATH_MAX];

    if (realpath_or_absolute(old_value, old_abs) == -1) return -1;
    if (realpath_or_absolute(from_root, from_abs) == -1) return -1;
    if (realpath_or_absolute(to_root, to_abs) == -1) return -1;

    size_t len = strlen(from_abs);
    if (strncmp(old_abs, from_abs, len) == 0 && (old_abs[len] == '\0' || old_abs[len] == '/')) {
        snprintf(new_value, PATH_MAX, "%s%s", to_abs, old_abs + len);
    } else {
        strncpy(new_value, old_value, PATH_MAX);
        new_value[PATH_MAX - 1] = '\0';
    }

    return 0;
}

int copy_file(const char *source, const char *target, mode_t mode) {
    if (ensure_parent_dir(target, 0755) == -1) return -1;

    int in = open(source, O_RDONLY);
    if (in == -1) return -1;

    int out = open(target, O_WRONLY | O_CREAT | O_TRUNC, mode & 0777);
    if (out == -1) {
        close(in);
        return -1;
    }

    char buffer[8192];
    ssize_t r;
    while ((r = read(in, buffer, sizeof(buffer))) > 0) {
        ssize_t written = 0;
        while (written < r) {
            ssize_t w = write(out, buffer + written, (size_t)(r - written));
            if (w == -1) {
                close(in);
                close(out);
                return -1;
            }
            written += w;
        }
    }

    int ok = (r == -1) ? -1 : 0;
    fchmod(out, mode & 0777);
    close(in);
    close(out);
    return ok;
}

int copy_symlink_adjusted(const char *source_link, const char *target_link, const char *source_root, const char *target_root) {
    char old_value[PATH_MAX];
    char new_value[PATH_MAX];

    ssize_t len = readlink(source_link, old_value, sizeof(old_value) - 1);
    if (len == -1) return -1;
    old_value[len] = '\0';

    if (map_link_target(old_value, new_value, source_root, target_root) == -1) return -1;

    if (ensure_parent_dir(target_link, 0755) == -1) return -1;
    unlink(target_link);

    return symlink(new_value, target_link);
}

int copy_dir(const char *source, const char *target, const char *source_root, const char *target_root) {
    struct stat st;
    if (lstat(source, &st) == -1) return -1;

    if (mkdir_p(target, st.st_mode & 0777) == -1) return -1;

    DIR *dir = opendir(source);
    if (!dir) return -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char src[PATH_MAX];
        char dst[PATH_MAX];
        snprintf(src, sizeof(src), "%s/%s", source, ent->d_name);
        snprintf(dst, sizeof(dst), "%s/%s", target, ent->d_name);

        if (copy_entry(src, dst, source_root, target_root) == -1) {
            perror("copy_entry");
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    chmod(target, st.st_mode & 0777);
    return 0;
}

int copy_entry(const char *source, const char *target, const char *source_root, const char *target_root) {
    struct stat st;
    if (lstat(source, &st) == -1) return -1;

    if (S_ISDIR(st.st_mode)) {
        return copy_dir(source, target, source_root, target_root);
    }

    if (S_ISREG(st.st_mode)) {
        unlink(target);
        return copy_file(source, target, st.st_mode & 0777);
    }

    if (S_ISLNK(st.st_mode)) {
        return copy_symlink_adjusted(source, target, source_root, target_root);
    }

    return 0;
}

int files_are_different(const char *source, const char *target) {
    struct stat a;
    struct stat b;

    if (lstat(source, &a) == -1) return 1;
    if (lstat(target, &b) == -1) return 1;

    if ((a.st_mode & S_IFMT) != (b.st_mode & S_IFMT)) return 1;

    if (S_ISREG(a.st_mode)) {
        return a.st_size != b.st_size || a.st_mtime != b.st_mtime;
    }

    if (S_ISLNK(a.st_mode)) {
        char la[PATH_MAX];
        char lb[PATH_MAX];
        ssize_t ra = readlink(source, la, sizeof(la) - 1);
        ssize_t rb = readlink(target, lb, sizeof(lb) - 1);
        if (ra == -1 || rb == -1) return 1;
        la[ra] = '\0';
        lb[rb] = '\0';
        return strcmp(la, lb) != 0;
    }

    return 0;
}