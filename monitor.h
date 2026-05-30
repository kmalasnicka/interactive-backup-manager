#ifndef MONITOR_H
#define MONITOR_H

#include <sys/inotify.h>

int start_monitoring(const char *source, const char *target);

#endif