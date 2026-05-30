#include "backup.h"
#include "restore.h"
#include "utils.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct backup_entry {
    pid_t pid;
    char source[PATH_MAX];
    char target[PATH_MAX];
    struct backup_entry *next;
};

static struct backup_entry *head = NULL;
static volatile sig_atomic_t exiting = 0;

static void parent_signal_handler(int sig) {
    (void)sig;
    exiting = 1;
}

static void setup_parent_signals(void) {
    for (int i = 1; i < NSIG; i++) {
        if (i == SIGKILL || i == SIGSTOP) continue;
        signal(i, SIG_IGN);
    }

    signal(SIGINT, parent_signal_handler);
    signal(SIGTERM, parent_signal_handler);
    signal(SIGCHLD, SIG_DFL);
}

static void setup_child_signals(void) {
    for (int i = 1; i < NSIG; i++) {
        if (i == SIGKILL || i == SIGSTOP) continue;
        signal(i, SIG_DFL);
    }

    signal(SIGPIPE, SIG_IGN);
}

static struct backup_entry *find_entry(const char *source, const char *target) {
    char src_abs[PATH_MAX];
    char tgt_abs[PATH_MAX];

    if (realpath_or_absolute(source, src_abs) == -1) return NULL;
    if (realpath_or_absolute(target, tgt_abs) == -1) return NULL;

    for (struct backup_entry *cur = head; cur != NULL; cur = cur->next) {
        if (strcmp(cur->source, src_abs) == 0 && strcmp(cur->target, tgt_abs) == 0) {
            return cur;
        }
    }

    return NULL;
}

static void add_entry(pid_t pid, const char *source_abs, const char *target_abs) {
    struct backup_entry *entry = malloc(sizeof(*entry));

    if (!entry) {
        perror("malloc");
        return;
    }

    entry->pid = pid;

    strncpy(entry->source, source_abs, PATH_MAX);
    entry->source[PATH_MAX - 1] = '\0';

    strncpy(entry->target, target_abs, PATH_MAX);
    entry->target[PATH_MAX - 1] = '\0';

    entry->next = head;
    head = entry;
}

static int stop_entry_exact(const char *source, const char *target) {
    char src_abs[PATH_MAX];
    char tgt_abs[PATH_MAX];

    if (realpath_or_absolute(source, src_abs) == -1) return 0;
    if (realpath_or_absolute(target, tgt_abs) == -1) return 0;

    struct backup_entry **cur = &head;

    while (*cur != NULL) {
        struct backup_entry *entry = *cur;

        if (strcmp(entry->source, src_abs) == 0 && strcmp(entry->target, tgt_abs) == 0) {
            kill(entry->pid, SIGTERM);
            waitpid(entry->pid, NULL, 0);

            *cur = entry->next;
            free(entry);

            printf("backup stopped: %s -> %s\n", src_abs, tgt_abs);

            return 1;
        }

        cur = &(*cur)->next;
    }

    return 0;
}

static void cleanup_finished_children(void) {
    struct backup_entry **cur = &head;

    while (*cur != NULL) {
        struct backup_entry *entry = *cur;
        int status;

        pid_t result = waitpid(entry->pid, &status, WNOHANG);

        if (result == entry->pid) {
            printf("backup process finished: %s -> %s\n", entry->source, entry->target);

            *cur = entry->next;
            free(entry);
        } else {
            cur = &(*cur)->next;
        }
    }
}

static void stop_all_backups(void) {
    while (head != NULL) {
        struct backup_entry *entry = head;
        head = head->next;

        kill(entry->pid, SIGTERM);
        waitpid(entry->pid, NULL, 0);

        free(entry);
    }
}

static void list_backups(void) {
    cleanup_finished_children();

    if (head == NULL) {
        printf("no active backups\n");
        return;
    }

    printf("active backups:\n");

    for (struct backup_entry *cur = head; cur != NULL; cur = cur->next) {
        printf("  %s -> %s (pid: %d)\n", cur->source, cur->target, cur->pid);
    }
}

static void command_add(char *args[], int argc) {
    if (argc < 3) {
        printf("usage: add <source path> <target path> [target path...]\n");
        return;
    }

    for (int i = 2; i < argc; i++) {
        char source_abs[PATH_MAX];
        char target_abs[PATH_MAX];

        if (validate_backup_paths(args[1], args[i], source_abs, target_abs) == -1) {
            continue;
        }

        if (find_entry(source_abs, target_abs) != NULL) {
            fprintf(stderr, "error: backup already exists: %s -> %s\n", source_abs, target_abs);
            continue;
        }

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            setup_child_signals();

            int result = run_backup(source_abs, target_abs);

            exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        }

        add_entry(pid, source_abs, target_abs);

        printf("backup started: %s -> %s (pid: %d)\n", source_abs, target_abs, pid);
    }
}

static void command_end(char *args[], int argc) {
    if (argc < 3) {
        printf("usage: end <source path> <target path> [target path...]\n");
        return;
    }

    for (int i = 2; i < argc; i++) {
        if (!stop_entry_exact(args[1], args[i])) {
            printf("backup not found: %s -> %s\n", args[1], args[i]);
        }
    }
}

static void command_restore(char *args[], int argc) {
    if (argc != 3) {
        printf("usage: restore <source path> <target path>\n");
        return;
    }

    stop_entry_exact(args[1], args[2]);

    if (restore_backup(args[1], args[2]) == 0) {
        printf("restore completed: %s <- %s\n", args[1], args[2]);
    } else {
        printf("restore failed\n");
    }
}

int main(void) {
    setup_parent_signals();
    print_help();

    char line[4096];

    while (!exiting) {
        cleanup_finished_children();

        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (exiting) break;
            if (feof(stdin)) break;

            clearerr(stdin);
            continue;
        }

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') continue;

        char *args[MAX_ARGS];
        int argc = parse_line_with_quotes(line, args, MAX_ARGS);

        if (argc <= 0) continue;

        if (strcmp(args[0], "add") == 0) {
            command_add(args, argc);
        } else if (strcmp(args[0], "end") == 0) {
            command_end(args, argc);
        } else if (strcmp(args[0], "list") == 0) {
            list_backups();
        } else if (strcmp(args[0], "restore") == 0) {
            command_restore(args, argc);
        } else if (strcmp(args[0], "help") == 0) {
            print_help();
        } else if (strcmp(args[0], "exit") == 0) {
            break;
        } else {
            printf("unknown command: %s\n", args[0]);
        }
    }

    stop_all_backups();

    return 0;
}