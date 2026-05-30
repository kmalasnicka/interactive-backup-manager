# Interactive Backup Management System

A Linux command-line application written in C for automatically creating and managing directory backups.

The program allows users to back up a selected source directory to one or more target locations. After the initial copy is completed, the source is monitored in real time using `inotify`, and all changes are mirrored in the corresponding backup directories.

The application works in interactive mode, allowing users to start new backups, stop selected ones, display active backups, and restore data from a chosen backup location. It was developed as part of an Operating Systems course and demonstrates practical use of POSIX system programming.

## Available Commands

### Add a backup

```
add <source_path> <target_path> [target_path...]
```

Creates a backup of the source directory in one or more target directories and starts monitoring it for changes. Multiple target paths can be given in a single command — each runs in its own subprocess.

- If a target directory does not exist, it is created automatically.
- If a target directory already exists, it must be empty before copying starts.
- Creating a backup of a directory inside itself is not allowed.
- Creating a duplicate backup (same source and target) is not allowed.

### End a backup

```
end <source_path> <target_path> [target_path...]
```

Stops synchronization for the given source-target pairs. The contents of the target directories are left unchanged.

### List active backups

```
list
```

Displays all currently active backup processes together with their source path, target path, and PID.

### Restore a backup

```
restore <source_path> <target_path>
```

Restores the source directory from the selected backup:

- Files present in the backup but missing or changed in the source are copied back.
- Files present in the source but absent from the backup are deleted.
- Only files that differ (by size or modification time) are copied, minimizing unnecessary I/O.
- Symbolic link targets are adjusted back to point to the source tree.
- If the backup is actively running, it is stopped before restoration begins.
- The command is **blocking** — the prompt returns only after restoration completes.

### Show help

```
help
```

Prints the list of available commands.

### Exit

```
exit
```

Terminates the program and stops all active child processes cleanly. The program also handles `SIGINT` and `SIGTERM` gracefully. All other signals are ignored.

## Technical Details

The project uses Linux/POSIX APIs and low-level system programming mechanisms, including:

- `fork()` for parallel backup handling
- `inotify` for monitoring filesystem changes
- `open()`, `read()`, `write()`, and `close()` for file operations
- `lstat()`, `readlink()`, and `symlink()` for file and symbolic link handling
- `mkdir()`, `unlink()`, and `rmdir()` for filesystem management
- `realpath()` and custom path normalization for path validation
- signal handling for graceful termination
- custom command parsing with support for quoted paths
