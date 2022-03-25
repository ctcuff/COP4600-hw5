#include <cerrno>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#define HISTORY_FILE_NAME "mysh.history"
#define HISTORY_FILE_PATH "./" HISTORY_FILE_NAME

std::set<pid_t> activePids;
std::set<std::string> VALID_COMMANDS;

void parseCommand(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::vector<std::string>& history);

// If no arguments are passed, this prints all history (current application history plus the
// history saved in mysh.history). If "-c" is passed, all history will be cleared (including
// this history in the history file).
void executeHistoryCommand(std::vector<std::string>& history, const std::vector<std::string>& args);

// Re-executes the command at the given number in history.
void executeReplayCommand(const std::vector<std::string>& history, int index);

// If the parameter background is true, this will start the specified program and return
// execution back to this program. Otherwise, execution will halt until the given
// program finishes executing.
void executeStartCommand(const std::vector<std::string>& args, bool background);

// Takes a program name, number of repetitions, and (optionally) additional arguments
// that are passed to that program and starts n processes of that program
void executeRepeatCommand(const std::vector<std::string>& args);

// Returns true if the process was terminated successfully
bool terminateProcess(pid_t pid);

void terminateAllProcesses();

// Takes a path name and check if that path is a file or directory. If the path
// is a file, this function will print `Dwelt indeed`. If the path a directory,
// this function will print `Abode is`. If the path doesn't exist, this function
// will print `Dwelt not`.
void checkFileOrDirectory(std::string path);

// Takes a file name, creates that file, and writes the word `Draft` into it.
// If the file already exists, this will print an error.
void createAndWriteToFile(std::string filename);

// Takes the path to a source file and copies the contents to the dest file.
// If the source file doesn't exist, or the destinations's directory doesn't
// exist, this will print an error.
void copyFileToFile(std::string source, std::string dest);

namespace Util {
    bool isStringEmpty(std::string& string) {
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

    bool doesFileOrDirExist(const std::string& path) {
        return access(path.c_str(), F_OK) != -1;
    }

    bool isFile(const std::string& path) {
        if (!Util::doesFileOrDirExist(path)) {
            return false;
        }

        struct stat pathStat;

        stat(path.c_str(), &pathStat);

        return S_ISREG(pathStat.st_mode) == 1;
    }

    bool isDirectory(const std::string& path) {
        if (!Util::doesFileOrDirExist(path)) {
            return false;
        }

        struct stat pathStat;

        stat(path.c_str(), &pathStat);

        return S_ISDIR(pathStat.st_mode) == 1;
    }

    int writeHistory(const std::vector<std::string>& history) {
        try {
            std::ofstream file;
            file.open(HISTORY_FILE_PATH);

            for (int i = 0; i < static_cast<int>(history.size()); i++) {
                if (history[i] != "byebye") {
                    file << history[i] << std::endl;
                }
            }

            char cwd[512];
            getcwd(cwd, sizeof(cwd));

            std::cout << "mysh: History saved to " << cwd << "/" << HISTORY_FILE_NAME << std::endl;
            file.close();
        } catch (const std::exception& err) {
            std::cout << "mysh: Error writing to history file " << err.what() << std::endl;
            return -1;
        }

        return 0;
    }

    std::vector<std::string> loadHistory() {
        std::vector<std::string> history = std::vector<std::string>();

        std::ifstream file;
        std::string line;
        file.open(HISTORY_FILE_PATH, std::ios_base::in);

        if (file.fail()) {
            return history;
        }

        while (std::getline(file, line, '\n')) {
            history.push_back(line);
        }

        file.close();

        return history;
    }
}

int main() {
    VALID_COMMANDS.insert("background");
    VALID_COMMANDS.insert("byebye");
    VALID_COMMANDS.insert("coppy");
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
        std::cout << "# ";
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

        executeStartCommand(args, command == "background");
    }

    if (command == "byebye") {
        exit(Util::writeHistory(history));
    }

    if (command == "history") {
        executeHistoryCommand(const_cast<std::vector<std::string>&>(history), args);
    }

    if (command == "repeat") {
        executeRepeatCommand(args);
    }

    if (command == "replay") {
        if (args.empty()) {
            std::cerr << "mysh: Missing argument [index]" << std::endl;
            return;
        }

        try {
            executeReplayCommand(history, std::stoi(args[0]));
        } catch (const std::invalid_argument& err) {
            std::cerr << "mysh: Argument must be a number" << std::endl;
        } catch (const std::out_of_range& err) {
            std::cerr << "mysh: Argument out of range" << std::endl;
        }
    }

    if (command == "terminate") {
        if (args.empty()) {
            std::cerr << "mysh: Missing argument [pid]" << std::endl;
            return;
        }

        try {
            pid_t pid = std::stoi(args[0]);

            if (terminateProcess(pid)) {
                activePids.erase(pid);
            }
        } catch (const std::invalid_argument& err) {
            std::cerr << "mysh: Argument must be a number" << std::endl;
        } catch (const std::out_of_range& err) {
            std::cerr << "mysh: Argument out of range" << std::endl;
        }
    }

    if (command == "terminateall") {
        terminateAllProcesses();
    }

    if (command == "movetodir") {
        // TODO
    }

    if (command == "dwelt") {
        if (args.size() < 1) {
            std::cerr << "mysh: Missing argument [file | directory]" << std::endl;
        } else {
            checkFileOrDirectory(args[0]);
        }
    }

    if (command == "maik") {
        if (args.size() < 1) {
            std::cerr << "mysh: Missing argument [filename]" << std::endl;
        } else {
            createAndWriteToFile(args[0]);
        }
    }

    if (command == "coppy") {
        if (args.size() < 2) {
            std::cerr << "mysh: Usage: coppy [source] [destination]" << std::endl;
        } else {
            copyFileToFile(args[0], args[1]);
        }
    }
}

void executeHistoryCommand(std::vector<std::string>& history, const std::vector<std::string>& args) {
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

void executeReplayCommand(const std::vector<std::string>& history, const int index) {
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

void executeStartCommand(const std::vector<std::string>& args, bool background) {
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

    programArgs[args.size()] = nullptr;

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

void executeRepeatCommand(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "mysh: Usage: repeat [repetitions] [command]" << std::endl;
        return;
    }

    int repetitions;
    std::vector<std::string> command = std::vector<std::string>(args.begin() + 1, args.end());

    try {
        repetitions = std::stoi(args[0]);
    } catch (const std::invalid_argument& err) {
        std::cerr << "mysh: Argument [repetitions] must be a number" << std::endl;
        return;
    } catch (const std::out_of_range& err) {
        std::cerr << "mysh: Argument [repetitions] out of range" << std::endl;
        return;
    }

    for (int i = 0; i < repetitions; i++) {
        executeStartCommand(command, true);
    }
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

void checkFileOrDirectory(std::string path) {
    if (!Util::doesFileOrDirExist(path)) {
        std::cout << "mysh: Dwelt not." << std::endl;
    } else if (Util::isFile(path)) {
        std::cout << "mysh: Dwelt indeed" << std::endl;
    } else if (Util::isDirectory(path)) {
        std::cout << "mysh: Abode is." << std::endl;
    }
}

void createAndWriteToFile(std::string filename) {
    if (Util::doesFileOrDirExist(filename)) {
        std::cerr << "mysh: " << filename << " already exists." << std::endl;
        return;
    }

    char cwd[512];
    getcwd(cwd, sizeof(cwd));

    try {
        std::ofstream file;
        file.open(filename.c_str());
        file << "Draft\n";
        file.close();

        std::cout << "mysh: File saved to " << cwd << "/" << filename << std::endl;
    } catch (const std::exception& err) {
        std::cout << "mysh: Error writing to " << filename << " " << err.what() << std::endl;
    }
}

void copyFileToFile(std::string source, std::string dest) {
    if (!Util::doesFileOrDirExist(source)) {
        std::cerr << "mysh: Source file " << source << " doesn't exist" << std::endl;
        return;
    }

    if (Util::isDirectory(source)) {
        std::cerr << "mysh: " << source << " is a directory" << std::endl;
        return;
    }

    if (Util::isDirectory(dest)) {
        std::cerr << "mysh: " << dest << " is already a directory" << std::endl;
        return;
    }

    if (Util::isFile(dest)) {
        std::cerr << "mysh: " << dest << " is already a file" << std::endl;
        return;
    }

    try {
        std::ifstream sourceFile;
        std::ofstream destFile;
        std::string content;

        sourceFile.open(source);
        destFile.open(dest);

        if (sourceFile.is_open() && destFile.is_open()) {
            while (std::getline(sourceFile, content, '\n')) {
                destFile << content << "\n";
            }
        }

        destFile.close();
        sourceFile.close();
    } catch (const std::exception& err) {
        std::cout << "mysh: Error copying file: " << err.what() << std::endl;
    }
}
