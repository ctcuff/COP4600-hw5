#include <unistd.h>
#include <iostream>
#include <fstream>
#include <signal.h>

std::ofstream file;

void signalHandler(int sig) {
    int pid = getpid();
    file << "[" << pid << "]" << " Stopped" << std::endl;
    file.close();
    exit(0);
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    int count = 0;
    int pid = getpid();

    file.open("./tmp.log", std::ios_base::app);

    if (file.fail()) {
        std::cerr << "Failed to open log, exiting..." << std::endl;
        exit(1);
    }

    while (true) {
        file << "[" << pid << "]" << " Running : " << ++count << std::endl;
        sleep(1);
    }
}
