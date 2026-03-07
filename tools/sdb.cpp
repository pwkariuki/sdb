//
// Created by root on 3/4/26.
//
#include <algorithm>
#include <editline/readline.h>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
    // Launches, attaches to the given program name or PID,
    // and returns PID of the inferior.
    pid_t attach(int argc, const char **argv) {
        pid_t pid = 0;
        // Passing PID
        if (argc == 3 && argv[1] == std::string_view("-p")) {
            pid = std::atoi(argv[2]);
            if (pid <= 0) {
                std::cerr << "Invalid pid" << std::endl;
                return -1;
            }
            // Attach to process
            if (ptrace(PTRACE_ATTACH, pid, /*addr=*/nullptr, /*addr=*/nullptr) < 0) {
                std::perror("Could not attach");
                return -1;
            }
        }
        // Passing program name
        else {
            const char* program_path = argv[1];
            if ((pid = fork()) < 0) {
                std::perror("fork failed");
                return -1;
            }

            // Replace currently executing program with the one to debug
            if (pid == 0) {
                // Set child process to be traced
                if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
                    std::perror("Tracing failed");
                    return -1;
                }
                if (execlp(program_path, program_path, nullptr) < 0) {
                    std::perror("Exec failed");
                    return -1;
                }
            }
        }
        return pid;
    }

    std::vector<std::string> split(std::string_view str, char delimiter) {
        std::vector<std::string> out;
        std::stringstream ss {std::string{str}};
        std::string item;

        while (std::getline(ss, item, delimiter)) {
            out.emplace_back(item);
        }

        return out;
    }

    bool is_prefix(std::string_view str, std::string_view of) {
        if (str.size() > of.size()) return false;
        return std::equal(str.begin(), str.end(), of.begin());
    }

    void resume(pid_t pid) {
        if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0) {
            std::cerr << "Could not continue" << std::endl;
            std::exit(-1);
        }
    }

    void wait_on_signal(pid_t pid) {
        int wait_status;
        int options = 0;
        if (waitpid(pid, &wait_status, options) < 0) {
            std::perror("waitpid failed");
            std::exit(-1);
        }
    }

    void handle_command(pid_t pid, std::string_view line) {
        auto args = split(line, ' ');
        const auto& command = args[0];

        if (is_prefix(command, "continue")) {
            resume(pid);
            wait_on_signal(pid);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
        }
    }
}

int main(int argc, const char **argv) {
    if (argc == 1) {
        std::cerr << "No arguments given" << std::endl;
        return -1;
    }

    pid_t pid = attach(argc, argv);

    int wait_status;
    int options = 0;
    // Wait for process to stop after we attach to it
    if (waitpid(pid, &wait_status, options) < 0) {
        std::perror("waitpid failed");
    }

    // Process user commands from debug console
    char* line = nullptr;
    while ((line = readline("sdb> ")) != nullptr) {
        std::string line_str;

        if (line == std::string_view("")) {
            free(line);
            if (history_length > 0) { // rerun previous command
                line_str = history_list()[history_length - 1]->line;
            }
        } else {
            line_str = line;
            add_history(line); // add to searchable history
            free(line);
        }

        if (!line_str.empty()) {
            handle_command(pid, line_str);
        }
    }
}
