# Interactive Backup Management System

A command-line application written in C for automatically creating and managing directory backups on Linux.

The program allows users to back up a selected source directory to one or more target locations. After the initial copy is completed, the source is monitored in real time, and all changes are mirrored in the corresponding backup directories.

The application works in an interactive mode, allowing users to start new backups, stop selected backups, display active backup configurations, and restore data from a chosen backup location. It was developed as part of an Operating Systems course and demonstrates practical use of POSIX system programming, filesystem operations, signal handling, and `inotify`-based monitoring.

## Usage

`./backup-manager`

After starting, the program prints the list of available commands and waits for input. Directory paths containing spaces must be wrapped in single or double quotes, for example: `add "/my dir" "/backup dir"`.

## Available Commands

### Add a backup

`add <source_path> <target_path> [target_path...]`

Creates a backup of the source directory in one or more target directories and starts monitoring it for changes.

- If a target directory does not exist, it is created automatically.
- If a target directory already exists, it must be empty before copying starts.
- Creating a backup of a directory inside itself is not allowed.
- Creating a duplicate backup with the same source and target is not allowed.

### End a backup

`end <source_path> <target_path> [target_path...]`

Stops synchronization for the given source-target pairs. The contents of the target directories are left unchanged.

### List active backups

`list`

Displays all currently active backups together with their source and target paths.

### Restore a backup

`restore <source_path> <target_path>`

Restores the source directory from the selected backup:

- Files present in the backup but missing or changed in the source are copied back.
- Files present in the source but absent from the backup are deleted.
- Only files that differ by size or modification time are copied, minimizing unnecessary I/O.
- Symbolic link targets are adjusted back to point to the source tree.
- If the selected backup is active, it is stopped before restoration begins.
- The command is **blocking**, so the prompt returns only after the restore operation is completed.

### Show help

`help`

Prints the list of available commands.

### Exit

`exit`

Terminates the program and stops all active backups cleanly. The program also handles `SIGINT` and `SIGTERM` gracefully. Other signals are ignored.

## Technical Details

The project uses Linux/POSIX APIs and low-level system programming mechanisms, including:

- `inotify` for monitoring filesystem changes
- `open()`, `read()`, `write()`, and `close()` for file operations
- `lstat()`, `readlink()`, and `symlink()` for file and symbolic link handling
- `mkdir()`, `unlink()`, and `rmdir()` for filesystem management
- `realpath()` and custom path normalization for path validation
- signal handling for graceful termination
- custom command parsing with support for quoted paths
