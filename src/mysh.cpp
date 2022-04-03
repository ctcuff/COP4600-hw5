#include <cerrno>
#include <csignal>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#define HISTORY_FILE_NAME "mysh.history"
#define HISTORY_FILE_PATH "./" HISTORY_FILE_NAME
#define MAX_PATH_LENGTH   512

// Used to print in color in debug mode
#ifdef DEBUG
#define BLUE  "\x1B[34m"
#define GREEN "\x1B[32m"
#define RESET "\x1B[0m"
#endif

// Stores all PIDs created by the start and background commands
std::set<pid_t> activePids;
std::set<std::string> VALID_COMMANDS;

void parseCommand(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::vector<std::string>& history);

// If no arguments are passed, this prints all history (current application history plus the
// history saved in mysh.history). If "-c" is passed, all history will be cleared (including
// this history in the history file).
void showHistory(std::vector<std::string>& history, const std::vector<std::string>& args);

// Re-executes the command at the given number in history.
void replayCommand(const std::vector<std::string>& history, int index);

// If the parameter background is true, this will start the specified program and return
// execution back to this program. Otherwise, execution will halt until the given
// program finishes executing.
void startProgram(const std::vector<std::string>& args, bool background);

// Takes a program name, number of repetitions, and (optionally) additional arguments
// that are passed to that program and starts n processes of that program
void repeatCommand(const std::vector<std::string>& args);

// Terminates the process with the given PID.
// Returns true if the process was terminated successfully
bool terminateProcess(const pid_t pid);

void terminateAllProcesses();

// Takes a path and checks if that path is a file or directory. If the path
// is a file, this function will print "Dwelt indeed". If the path a directory,
// this function will print "Abode is". If the path doesn't exist, this function
// will print "Dwelt not".
void checkFileOrDirectory(const std::string& path);

// Takes a file name, creates that file, and writes the word "Draft" into it.
// If the file already exists, this will print an error.
void createAndWriteToFile(const std::string& filename);

// Takes the path to a source file and copies the contents to the dest file.
// If the source file doesn't exist, or the destination's directory doesn't
// exist, this will print an error. If force is true, the file in the
// destination path will be overriden if it already exists.
void copyFileToFile(const std::string& source, const std::string& dest, const bool force);

// Causes "path" to become the current working directory.
// This supports both absolute and relative paths.
void moveToDirectory(const std::string& path);

// Recursively copies all files and subdirectories from the source directory
// to the destination directory
void copyDirectory(const char* source, const char* dest);

namespace Util {
    char* getCurrentDir() {
        static char cwd[MAX_PATH_LENGTH];
        getcwd(cwd, sizeof(cwd));
        return cwd;
    }

    // Takes a string and returns true if that string's length is 0
    // or if the string contains only spaces
    bool isStringEmpty(const std::string& string) {
        if (string.empty()) {
            return true;
        }

        for (int i = 0; i < static_cast<int>(string.size()); i++) {
            if (string[i] != ' ') {
                return false;
            }
        }

        return true;
    }

    // Takes a string and a character delimiter and splits that string into
    // a vector based on the delimiter.
    std::vector<std::string> splitString(const std::string& string, const char delimiter) {
        std::vector<std::string> tokens = std::vector<std::string>();
        std::string token;
        std::stringstream ss(string);

        while (std::getline(ss, token, delimiter)) {
            if (!Util::isStringEmpty(token)) {
                tokens.push_back(token);
            }
        }

        return tokens;
    }

    // Takes a path and returns true if that path points
    // to a file or directory that exists.
    bool doesFileOrDirExist(const std::string& path) {
        return access(path.c_str(), F_OK) != -1;
    }

    // Takes a path and returns true if that path points to a file that exists.
    bool isFile(const std::string& path) {
        if (!Util::doesFileOrDirExist(path)) {
            return false;
        }

        struct stat pathStat;
        stat(path.c_str(), &pathStat);
        return S_ISREG(pathStat.st_mode) == 1;
    }

    // Takes a path and returns true if that path points to a directory that exists.
    bool isDirectory(const std::string& path) {
        if (!Util::doesFileOrDirExist(path)) {
            return false;
        }

        struct stat pathStat;
        stat(path.c_str(), &pathStat);
        return S_ISDIR(pathStat.st_mode) == 1;
    }

    // Saves the current shell's history to the history file (this
    // will append new history to the end). Returns 0 if successful and
    // -1 if an error occurred.
    int writeHistory(const std::vector<std::string>& history) {
        std::ofstream historyFile;
        historyFile.open(HISTORY_FILE_PATH);

        if (!historyFile.is_open()) {
            std::cerr << "mysh: Couldn't save history file: " << std::strerror(errno) << std::endl;
            return -1;
        }

        for (int i = 0; i < static_cast<int>(history.size()); i++) {
            if (history[i] != "byebye") {
                historyFile << history[i] << std::endl;
            }
        }

        std::cout << "mysh: History saved to " << Util::getCurrentDir() << "/" << HISTORY_FILE_NAME << std::endl;
        historyFile.close();

        return 0;
    }

    // Loads all history from the history file (if present) into a string vector.
    std::vector<std::string> loadHistory() {
        std::vector<std::string> history = std::vector<std::string>();
        std::ifstream historyFile;
        std::string line;

        historyFile.open(HISTORY_FILE_PATH, std::ios_base::in);

        if (historyFile.fail()) {
            return history;
        }

        while (std::getline(historyFile, line, '\n')) {
            history.push_back(line);
        }

        historyFile.close();

        return history;
    }

    // Takes a string and returns true if that string can be parsed
    // to a valid integer.
    bool isValidNumber(const std::string& input) {
        for (int i = 0; i < static_cast<int>(input.size()); i++) {
            if (isdigit(input[i]) == 0) {
                return false;
            }
        }
        return true;
    }
}

int main() {
    VALID_COMMANDS.insert("background");
    VALID_COMMANDS.insert("byebye");
    VALID_COMMANDS.insert("coppy");
    VALID_COMMANDS.insert("coppyabode");
    VALID_COMMANDS.insert("dwelt");
    VALID_COMMANDS.insert("history");
    VALID_COMMANDS.insert("maik");
    VALID_COMMANDS.insert("movetodir");
    VALID_COMMANDS.insert("repeat");
    VALID_COMMANDS.insert("replay");
    VALID_COMMANDS.insert("start");
    VALID_COMMANDS.insert("terminate");
    VALID_COMMANDS.insert("terminateall");

    std::string line;

    // This history stores all commands from the history file
    // as well as commands from the current session's history
    std::vector<std::string> history = Util::loadHistory();

    while (line != "byebye") {
#ifdef DEBUG
        std::cout << GREEN "[" << Util::getCurrentDir() << "]"
                  << BLUE " # " RESET;
#else
        std::cout << "# ";
#endif

        // Need to use std::getline instead of std::cin because cin skips spaces
        std::getline(std::cin, line, '\n');
        std::vector<std::string> tokens = Util::splitString(line, ' ');

        if (tokens.empty() || Util::isStringEmpty(line)) {
            continue;
        }

        history.push_back(line);

        std::string command = tokens[0];
        std::vector<std::string> args = std::vector<std::string>(tokens.begin() + 1, tokens.end());

        parseCommand(command, args, history);
    }

    return Util::writeHistory(history);
}

void parseCommand(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::vector<std::string>& history) {

    if (VALID_COMMANDS.find(command) == VALID_COMMANDS.end()) {
        std::cerr << "mysh: " << command << ": command not found" << std::endl;
        return;
    }

    if (command == "background" || command == "start") {
        if (args.empty()) {
            std::cerr << "mysh: Missing argument [program]" << std::endl;
            return;
        }

        startProgram(args, command == "background");
    }

    if (command == "byebye") {
        exit(Util::writeHistory(history));
    }

    if (command == "history") {
        showHistory(const_cast<std::vector<std::string>&>(history), args);
    }

    if (command == "repeat") {
        if (args.size() < 2) {
            std::cerr << "mysh: Usage: repeat [repetitions] [command]" << std::endl;
            return;
        }

        repeatCommand(args);
    }

    if (command == "replay") {
        if (args.empty()) {
            std::cerr << "mysh: Missing argument [index]" << std::endl;
            return;
        }

        if (!Util::isValidNumber(args[0])) {
            std::cerr << "mysh: Argument must be a number" << std::endl;
        } else {
            replayCommand(history, atoi(args[0].c_str()));
        }
    }

    if (command == "terminate") {
        if (args.empty()) {
            std::cerr << "mysh: Missing argument [pid]" << std::endl;
            return;
        }

        if (!Util::isValidNumber(args[0])) {
            std::cerr << "mysh: Argument must be a number" << std::endl;
            return;
        }

        pid_t pid = atoi(args[0].c_str());

        if (terminateProcess(pid)) {
            activePids.erase(pid);
        }
    }

    if (command == "terminateall") {
        terminateAllProcesses();
    }

    if (command == "movetodir") {
        if (args.empty()) {
            std::cerr << "mysh: Missing argument [directory]" << std::endl;
        } else {
            moveToDirectory(args[0]);
        }
    }

    if (command == "dwelt") {
        if (args.empty()) {
            std::cerr << "mysh: Missing argument [file | directory]" << std::endl;
        } else {
            checkFileOrDirectory(args[0]);
        }
    }

    if (command == "maik") {
        if (args.empty()) {
            std::cerr << "mysh: Missing argument [filename]" << std::endl;
        } else {
            createAndWriteToFile(args[0]);
        }
    }

    if (command == "coppy") {
        if (args.size() < 2) {
            std::cerr << "mysh: Usage: coppy [source] [destination]" << std::endl;
        } else {
            copyFileToFile(args[0], args[1], false);
        }
    }

    if (command == "coppyabode") {
        if (args.size() < 2) {
            std::cerr << "mysh: Usage: coppyabode [source-dir] [target-dir]" << std::endl;
            return;
        }

        std::string source = args[0];
        std::string dest = args[1];

        // Need to replace any leading "./" because it causes the copyDirectory
        // function to recurse into the dest directory and copy infinitely
        if (source.find("./", 0) == 0) {
            source = source.replace(0, 2, "");
        }

        if (dest.find("./", 0) == 0) {
            dest = dest.replace(0, 2, "");
        }

        if (Util::isDirectory(source) && strcmp(source.c_str(), dest.c_str()) == 0) {
            std::cerr << "mysh: Cannot copy '" << source << "' into itself" << std::endl;
            return;
        }

        copyDirectory(source.c_str(), dest.c_str());
    }
}

void showHistory(std::vector<std::string>& history, const std::vector<std::string>& args) {
    int historySize = static_cast<int>(history.size());

    if (args.empty()) {
        for (int i = historySize - 1; i >= 0; i--) {
            int index = historySize - (i + 1);
            std::cout << index << ": " << history[i] << std::endl;
        }
        return;
    }

    if (args[0] == "-c") {
        history.clear();
        std::cout << "mysh: History cleared" << std::endl;
    }
}

void replayCommand(const std::vector<std::string>& history, const int index) {
    if (index == static_cast<int>(history.size())) {
        std::cerr << "mysh: Index out of range" << std::endl;
        return;
    }

    // Need to subtract 2 here because "replay" will be added to the history
    // before the command is run, and we need to get the command that was executed
    // at position index - 1.
    std::string command = history[history.size() - index - 2];
    std::vector<std::string> tokens = Util::splitString(command, ' ');
    std::vector<std::string> args = std::vector<std::string>(tokens.begin() + 1, tokens.end());

    // Don't replay a replay command since it might cause an infinite loop
    if (command.find("replay") != 0) {
        parseCommand(tokens[0], args, history);
    } else {
        std::cerr << "mysh: Cannot replay a replay command" << std::endl;
    }
}

void startProgram(const std::vector<std::string>& args, bool background) {
    // Check if the file exists before running, so we don't unnecessarily fork
    if (!Util::doesFileOrDirExist(args[0])) {
        std::cerr << "mysh: " << args[0] << ": No such file or directory" << std::endl;
        return;
    }

    // execv ony takes a char** array, so we need to convert the string vector
    // arguments to a char** array
    const char** programArgs = new const char*[args.size() + 1];

    for (int i = 0; i < static_cast<int>(args.size()); i++) {
        programArgs[i] = args[i].c_str();
    }

    programArgs[args.size()] = NULL;

    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "mysh: Couldn't fork process." << std::endl;
        return;
    }

    activePids.insert(pid);

    if (pid == 0) {
        // This process is the child process, so we need to make sure the
        // process exits with it finishes
        int statusCode = execv(programArgs[0], const_cast<char**>(programArgs));

        if (statusCode != 0) {
            std::cerr << "mysh: " << std::strerror(errno) << std::endl;
        }

        activePids.erase(pid);
        exit(statusCode);
    } else {
        // This is the original process (the one that started the child process)
        if (background) {
            std::cout << "mysh: Spawned process with pid " << pid << std::endl;
        } else {
            int status;
            waitpid(pid, &status, 0);
            activePids.erase(pid);
        }
    }
}

bool terminateProcess(const pid_t pid) {
    if (pid < 0) {
        std::cerr << "mysh: Argument [pid] must be >= 0" << std::endl;
        return false;
    }

    int statusCode = kill(pid, SIGTERM);

    if (statusCode != 0) {
        std::cerr << "mysh: " << std::strerror(errno) << std::endl;
        return false;
    }

    std::cout << "mysh: Terminated process with pid " << pid << std::endl;

    return true;
}

void repeatCommand(const std::vector<std::string>& args) {
    std::vector<std::string> command = std::vector<std::string>(args.begin() + 1, args.end());

    if (!Util::isValidNumber(args[0])) {
        std::cerr << "mysh: Argument [repetitions] must be a number" << std::endl;
        return;
    }

    int repetitions = atoi(args[0].c_str());

    for (int i = 0; i < repetitions; i++) {
        startProgram(command, true);
    }

    // Using sleep here because spawning a large number of processess
    // can sometimes cause the terminal to display warning messages
    // that mess up this program's prompt
    sleep(1);
}

void terminateAllProcesses() {
    if (activePids.empty()) {
        std::cout << "mysh: No processes to terminate" << std::endl;
        return;
    }

    size_t numPids = activePids.size();

    for (std::set<pid_t>::iterator it = activePids.begin(); it != activePids.end(); ++it) {
        terminateProcess(*it);
    }

    activePids.clear();

    std::cout << "mysh: Terminated "
              << numPids
              << (numPids == 1 ? " process" : " processes")
              << std::endl;
}

void checkFileOrDirectory(const std::string& path) {
    if (!Util::doesFileOrDirExist(path)) {
        std::cout << "Dwelt not." << std::endl;
    } else if (Util::isFile(path)) {
        std::cout << "Dwelt indeed." << std::endl;
    } else if (Util::isDirectory(path)) {
        std::cout << "Abode is." << std::endl;
    }
}

void createAndWriteToFile(const std::string& filename) {
    if (Util::doesFileOrDirExist(filename)) {
        std::cerr << "mysh: " << filename << " already exists." << std::endl;
        return;
    }

    std::ofstream file;
    file.open(filename.c_str());

    if (!file.is_open()) {
        std::cerr << "mysh: " << filename << ": " << std::strerror(errno) << std::endl;
        return;
    }

    file << "Draft\n";
    file.close();
}

void copyFileToFile(const std::string& source, const std::string& dest, const bool force) {
    if (!Util::doesFileOrDirExist(source) || Util::isDirectory(source)) {
        std::cerr << "mysh: " << source << ": No such file" << std::endl;
        return;
    }

    if (Util::isDirectory(dest)) {
        std::cerr << "mysh: " << dest << ": Destination cannot be a directory" << std::endl;
        return;
    }

    if (!force && Util::isFile(dest)) {
        std::cerr << "mysh: " << dest << ": File already exists" << std::endl;
        return;
    }

    std::ifstream sourceFile;
    std::ofstream destFile;
    std::string content;

    sourceFile.open(source.c_str());

    if (!sourceFile.is_open()) {
        std::cerr << "mysh: " << source << ": " << std::strerror(errno) << std::endl;
        return;
    }

    destFile.open(dest.c_str());

    if (!destFile.is_open()) {
        std::cerr << "mysh: " << dest << ": " << std::strerror(errno) << std::endl;
        sourceFile.close();
        return;
    }

    if (sourceFile.is_open() && destFile.is_open()) {
        while (std::getline(sourceFile, content, '\n')) {
            destFile << content << "\n";
        }
    }

    destFile.close();
    sourceFile.close();
}

void moveToDirectory(const std::string& path) {
    if (!Util::isDirectory(path)) {
        std::cerr << "mysh: " << path << ": Not a directory" << std::endl;
        return;
    }

    int errorCode = chdir(path.c_str());

    if (errorCode == -1) {
        std::cerr << "mysh: " << std::strerror(errno) << std::endl;
    }
}

void copyDirectory(const char* source, const char* dest) {
    if (!Util::isDirectory(std::string(source))) {
        std::cerr << "mysh: " << source << ": Not a directory" << std::endl;
        return;
    }

    DIR* directory = opendir(source);
    struct dirent* dir;
    char sourceBuffer[MAX_PATH_LENGTH];
    char destBuffer[MAX_PATH_LENGTH];

    if (!directory) {
        std::cerr << "mysh: " << source << ": " << std::strerror(errno) << std::endl;
        return;
    }

    if (!Util::isDirectory(std::string(dest)) && mkdir(dest, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
        std::cerr << "mysh: " << dest << " : " << std::strerror(errno) << std::endl;
        closedir(directory);
        return;
    }

    std::vector<std::string> parts = Util::splitString(std::string(dest), '/');
    const char* baseDestPath = parts[0].c_str();

    while ((dir = readdir(directory)) != NULL) {
        if (
            strcmp(dir->d_name, ".") == 0 ||
            strcmp(dir->d_name, "..") == 0 ||
            strcmp(dir->d_name, dest) == 0) {
            continue;
        }

        snprintf(sourceBuffer, sizeof(sourceBuffer), "%s%c%s", source, '/', dir->d_name);
        snprintf(destBuffer, sizeof(destBuffer), "%s%c%s", dest, '/', dir->d_name);

        switch (dir->d_type) {
            case DT_REG: // File
                std::cout << "mysh: " << sourceBuffer << " => " << destBuffer << std::endl;
                copyFileToFile(sourceBuffer, destBuffer, true);
                break;
            case DT_DIR: // Directory
                // Make sure we don't try to recursively copy the destination
                // directory to the destination directory again.
                if (strcmp(dir->d_name, baseDestPath) != 0) {
                    copyDirectory(sourceBuffer, destBuffer);
                }
                break;
        }
    }

    closedir(directory);
}
