# Extra Credit Implementation Overview

This assignment implements the extra credit `coppyabode` command.

## Usage

`coppyabode [source-directory] [target-directory]`
- `[source-directory]` is the directory you'd like to copy and `[target-directory]` is the directory you'd like to copy the files into. This will recursively copy all the files and subdirectories.

## Implementation
The core functionality of the `coppyabode` command comes from the `copyDirectory` function. This function takes a source path and a destination path and recursively copies all files from the source directory into the destination directory (assuming the source directory exists). If the destination directory doesn't exist, it will be created when the command is executed. If the destination directory does exist, any files or folders in that directory will be overridden.

This function works by iterating through every file and folder in the current directory using the `opendir` and `readdir` methods. If `readdir` returns the path to a file, that file is read and copied into the destination directory. If `readdir` returns a directory, `copyDirectory` is called again with the path of that directory passed as an argument appended to the current source and destination directories. This function stops when there are no more files or directories to be copied.
