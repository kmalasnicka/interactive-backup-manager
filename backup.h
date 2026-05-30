#ifndef BACKUP_H
#define BACKUP_H

#include <limits.h>

int run_backup(const char *source, const char *target);
int validate_backup_paths(const char *source, const char *target, char source_abs[PATH_MAX], char target_abs[PATH_MAX]);

#endif