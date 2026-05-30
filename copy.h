#ifndef COPY_H
#define COPY_H

#include <limits.h>
#include <sys/types.h>

int copy_file(const char *source, const char *target, mode_t mode);
int copy_symlink_adjusted(const char *source_link, const char *target_link, const char *source_root, const char *target_root);
int copy_entry(const char *source, const char *target, const char *source_root, const char *target_root);
int copy_dir(const char *source, const char *target, const char *source_root, const char *target_root);
int files_are_different(const char *source, const char *target);

#endif