#include "monitor.h"
#include "copy.h"
#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_WATCHES 4096
#define EVENT_BUF_SIZE 8192

struct watch_entry {
    int wd;
    char path[PATH_MAX];
};

static struct watch_entry watches[MAX_WATCHES];
static int watch_count = 0;
static int inotify_fd_global = -1;
static volatile sig_atomic_t running = 1;

static void child_signal_handler(int sig) {
    (void)sig;
    running = 0;
    if (inotify_fd_global != -1) close(inotify_fd_global);
    inotify_fd_global = -1;
}

static const char *find_path_by_wd(int wd) {
    for (int i = 0; i < watch_count; i++) {
        if (watches[i].wd == wd) return watches[i].path;
    }
    return NULL;
}

static void remove_watch_from_table(int wd) {
    for (int i = 0; i < watch_count; i++) {
        if (watches[i].wd == wd) {
            watches[i] = watches[watch_count - 1];
            watch_count--;
            return;
        }
    }
}

static int add_watch(int inotify_fd, const char *path) {
    if (watch_count >= MAX_WATCHES) {
        fprintf(stderr, "error: too many watched directories\n");
        return -1;
    }

    int mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_CLOSE_WRITE |
               IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF;

    int wd = inotify_add_watch(inotify_fd, path, mask);
    if (wd == -1) return -1;

    watches[watch_count].wd = wd;
    strncpy(watches[watch_count].path, path, PATH_MAX);
    watches[watch_count].path[PATH_MAX - 1] = '\0';
    watch_count++;

    return 0;
}

static int add_watches_recursive(int inotify_fd, const char *path) {
    if (add_watch(inotify_fd, path) == -1) return -1;

    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);

        struct stat st;
        if (lstat(child, &st) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            add_watches_recursive(inotify_fd, child);
        }
    }

    closedir(dir);
    return 0;
}

static void build_paths(const char *base, const char *name,
                        const char *source_root, const char *target_root,
                        char source_path[PATH_MAX], char target_path[PATH_MAX]) {
    snprintf(source_path, PATH_MAX, "%s/%s", base, name);
    snprintf(target_path, PATH_MAX, "%s%s", target_root, source_path + strlen(source_root));
}

static void handle_create_or_move_to(const char *source_path,
                                     const char *target_path,
                                     int inotify_fd,
                                     const char *source_root,
                                     const char *target_root) {
    struct stat st;

    if (lstat(source_path, &st) == -1) return;

    if (S_ISDIR(st.st_mode)) {
        copy_dir(source_path, target_path, source_root, target_root);
        add_watches_recursive(inotify_fd, source_path);
    } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
        copy_entry(source_path, target_path, source_root, target_root);
    }
}

static void handle_event(struct inotify_event *event,
                         int inotify_fd,
                         const char *source_root,
                         const char *target_root) {
    const char *base = find_path_by_wd(event->wd);
    if (!base) return;

    if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
        remove_watch_from_table(event->wd);
        if (strcmp(base, source_root) == 0) running = 0;
        return;
    }

    if (event->len == 0) return;

    char source_path[PATH_MAX];
    char target_path[PATH_MAX];

    build_paths(base, event->name, source_root, target_root, source_path, target_path);

    if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
        remove_tree(target_path);
        return;
    }

    if (event->mask & IN_MOVED_TO) {
        handle_create_or_move_to(source_path, target_path, inotify_fd, source_root, target_root);
        return;
    }

    if (event->mask & IN_CREATE) {
        handle_create_or_move_to(source_path, target_path, inotify_fd, source_root, target_root);
        return;
    }

    if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
        struct stat st;
        if (lstat(source_path, &st) == -1) return;

        if (S_ISREG(st.st_mode)) {
            copy_entry(source_path, target_path, source_root, target_root);
        }
    }
}

int start_monitoring(const char *source, const char *target) {
    signal(SIGTERM, child_signal_handler);
    signal(SIGINT, child_signal_handler);
    signal(SIGPIPE, SIG_IGN);

    watch_count = 0;
    running = 1;

    int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        perror("inotify_init");
        return -1;
    }

    inotify_fd_global = inotify_fd;

    if (add_watches_recursive(inotify_fd, source) == -1) {
        perror("inotify_add_watch");
        close(inotify_fd);
        inotify_fd_global = -1;
        return -1;
    }

    char buffer[EVENT_BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    while (running) {
        ssize_t len = read(inotify_fd, buffer, sizeof(buffer));

        if (!running) break;

        if (len == -1) {
            if (errno == EINTR) continue;
            break;
        }

        for (char *ptr = buffer; ptr < buffer + len;) {
            struct inotify_event *event = (struct inotify_event *)ptr;
            handle_event(event, inotify_fd, source, target);
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }

    if (inotify_fd_global != -1) close(inotify_fd_global);
    inotify_fd_global = -1;

    return 0;
}