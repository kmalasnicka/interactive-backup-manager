#include "utils.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void print_help(void) {
    printf("commands:\n");
    printf("  add <source path> <target path> [target path...]\n");
    printf("  end <source path> <target path> [target path...]\n");
    printf("  list\n");
    printf("  restore <source path> <target path>\n");
    printf("  help\n");
    printf("  exit\n");
}

int parse_line_with_quotes(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *read_p = line;
    char *write_p = line;

    while (*read_p != '\0') {
        while (isspace((unsigned char)*read_p)) read_p++;
        if (*read_p == '\0') break;

        if (argc >= max_args) {
            fprintf(stderr, "too many arguments\n");
            return -1;
        }

        argv[argc++] = write_p;
        char quote = 0;

        while (*read_p != '\0') {
            if (quote == 0 && isspace((unsigned char)*read_p)) break;

            if (quote == 0 && (*read_p == '\'' || *read_p == '"')) {
                quote = *read_p++;
                continue;
            }

            if (quote != 0 && *read_p == quote) {
                quote = 0;
                read_p++;
                continue;
            }

            if (*read_p == '\\' && read_p[1] != '\0') read_p++;

            *write_p++ = *read_p++;
        }

        if (quote != 0) {
            fprintf(stderr, "error: unmatched quote\n");
            return -1;
        }

        if (*read_p != '\0') read_p++;

        *write_p++ = '\0';
    }

    return argc;
}

int path_exists(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

int is_empty_dir(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}

int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof(tmp)) return -1;

    strcpy(tmp, path);

    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            if (mkdir(tmp, mode) == -1 && errno != EEXIST) {
                return -1;
            }

            *p = '/';
        }
    }

    if (mkdir(tmp, mode) == -1 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

int remove_tree(const char *path) {
    struct stat st;

    if (lstat(path, &st) == -1) {
        if (errno == ENOENT) return 0;
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) return -1;

        struct dirent *ent;

        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

            char child[PATH_MAX];

            snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);

            if (remove_tree(child) == -1) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);

        return rmdir(path);
    }

    return unlink(path);
}

static void normalize_absolute_path(const char *input, char out[PATH_MAX]) {
    char copy[PATH_MAX];
    char *parts[PATH_MAX / 2];
    int count = 0;

    strncpy(copy, input, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(copy, "/", &saveptr);

    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            /* skip */
        } else if (strcmp(token, "..") == 0) {
            if (count > 0) count--;
        } else if (*token != '\0') {
            parts[count++] = token;
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    out[0] = '/';
    out[1] = '\0';

    for (int i = 0; i < count; i++) {
        if (strlen(out) > 1) strncat(out, "/", PATH_MAX - strlen(out) - 1);
        strncat(out, parts[i], PATH_MAX - strlen(out) - 1);
    }
}

int make_absolute_path(const char *path, char out[PATH_MAX]) {
    char tmp[PATH_MAX];

    if (path[0] == '/') {
        strncpy(tmp, path, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = '\0';
    } else {
        char cwd[PATH_MAX];

        if (!getcwd(cwd, sizeof(cwd))) return -1;
        if (strlen(cwd) + 1 + strlen(path) >= sizeof(tmp)) return -1;

        strcpy(tmp, cwd);
        strcat(tmp, "/");
        strcat(tmp, path);
    }

    normalize_absolute_path(tmp, out);

    return 0;
}

int realpath_or_absolute(const char *path, char out[PATH_MAX]) {
    if (realpath(path, out) != NULL) return 0;

    return make_absolute_path(path, out);
}

static int prefix_match_path(const char *parent, const char *child) {
    size_t len = strlen(parent);

    if (strncmp(parent, child, len) != 0) return 0;

    return child[len] == '\0' || child[len] == '/';
}

int is_subpath_or_same(const char *parent, const char *child) {
    char p[PATH_MAX];
    char c[PATH_MAX];

    if (realpath_or_absolute(parent, p) == -1) return 0;
    if (realpath_or_absolute(child, c) == -1) return 0;

    return prefix_match_path(p, c);
}

int is_strict_subpath(const char *parent, const char *child) {
    char p[PATH_MAX];
    char c[PATH_MAX];

    if (realpath_or_absolute(parent, p) == -1) return 0;
    if (realpath_or_absolute(child, c) == -1) return 0;

    return strcmp(p, c) != 0 && prefix_match_path(p, c);
}

int paths_equal(const char *a, const char *b) {
    char ra[PATH_MAX];
    char rb[PATH_MAX];

    if (realpath_or_absolute(a, ra) == -1) return 0;
    if (realpath_or_absolute(b, rb) == -1) return 0;

    return strcmp(ra, rb) == 0;
}

int ensure_parent_dir(const char *path, mode_t mode) {
    char tmp[PATH_MAX];

    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';

    char *slash = strrchr(tmp, '/');

    if (!slash) return 0;
    if (slash == tmp) return 0;

    *slash = '\0';

    return mkdir_p(tmp, mode);
}