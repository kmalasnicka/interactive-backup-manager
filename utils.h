#ifndef UTILS_H
#define UTILS_H 

#include <sys/stat.h> 
#define MAX_ARGS 10 

void usage(char *name);
int is_empty(const char *path);
int parse_line_with_quotes(char *line, char *argv[]);
int is_subpath(const char *source, const char *target);
int path_exists(const char *path);

#endif