#ifndef MONITOR_H
#define MONITOR_H

#include <sys/inotify.h>

void start_monitoring(const char *source, const char *target);
void add_watch(int inotify_fd, const char *path);
void add_watches_recursive(int inotify_fd, const char *path);
void monitor_loop(int inotify_fd, const char *source, const char *target);
void handle_event(struct inotify_event *event, int inotify_fd, const char *source, const char *target);
const char *find_path_by_wd(int wd);
void remove_watch(int inotify_fd, int wd);

#endif
