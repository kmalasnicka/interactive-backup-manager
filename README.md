# Interactive Backup Management System

A Linux command-line application written in C for automatically creating and managing directory backups.

The program allows users to back up a selected source directory to one or more target locations. After the initial copy is completed, the source is monitored in real time, and all changes are mirrored in the corresponding backup directories.

The application works in an interactive mode, allowing users to start new backups, stop selected processes, display active backups, and restore data from a chosen backup location. It was developed as part of an Operating Systems course and demonstrates practical use of POSIX system programming.

## Features

- Interactive command-line interface
- Backup creation from one source directory to one or more target directories
- Recursive copying of files, directories, and symbolic links
- Real-time monitoring of source directories with `inotify`
- Synchronization of changes between source and backup directories
- Parallel backup handling using child processes
- Validation of source and target paths
- Prevention of recursive and duplicated backups
- Listing and stopping active backups
- Blocking restore operation from a selected backup
- Support for paths containing spaces through quote parsing
- Graceful cleanup on `exit`, `SIGINT`, and `SIGTERM`

## Available Commands

### Add a backup

`add <source_path> <target_path> [target_path...]`

Creates a backup of the source directory in one or more target directories.
If a target directory does not exist, it is created automatically. If it already exists, it must be empty before copying starts.

### End a backup

`end <source_path> <target_path> [target_path...]`

Stops synchronization for selected source-target backup pairs.
The contents of the target directories remain unchanged.

### List active backups

`list`

Displays all currently active backups together with their source and target paths.

### Restore a backup

`restore <source_path> <target_path>`

Restores the source directory from the selected backup target.
The command works in blocking mode, so the next command can be entered only after restoration is completed.

### Show help

`help`

Displays the list of available commands.

### Exit

`exit`

Terminates the program and stops all active child processes.

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
