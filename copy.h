#ifndef COPY_H
#define COPY_H

#include <sys/stat.h>

void copy_files(const char *source, const char *target, mode_t mode);
void copy_dir(const char *source, const char *target);

#endif