#ifndef UTILS_H
#define UTILS_H

#include <limits.h>
#include <sys/types.h>

#define MAX_ARGS 64

void print_help(void);
int parse_line_with_quotes(char *line, char *argv[], int max_args);
int path_exists(const char *path);
int is_empty_dir(const char *path);
int mkdir_p(const char *path, mode_t mode);
int remove_tree(const char *path);
int make_absolute_path(const char *path, char out[PATH_MAX]);
int realpath_or_absolute(const char *path, char out[PATH_MAX]);
int is_subpath_or_same(const char *parent, const char *child);
int is_strict_subpath(const char *parent, const char *child);
int paths_equal(const char *a, const char *b);
int ensure_parent_dir(const char *path, mode_t mode);

#endif