//
// Created by root on 3/7/26.
//

#ifndef SDB_PROCESS_H
#define SDB_PROCESS_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <sys/types.h>

namespace sdb {
    // Running state of the process
    enum class process_state {
        stopped,
        running,
        exited,
        terminated,
    };

    // Reason for a process to stop e.g. exited, terminated, or stopped
    struct stop_reason {
        stop_reason(int wait_status);

        process_state reason;
        std::uint8_t info;
    };

    // Process Type
    class process {
    public:
        static std::unique_ptr<process> launch(const std::filesystem::path& path);
        static std::unique_ptr<process> attach(pid_t pid);

        void resume();
        stop_reason wait_on_signal();
        pid_t pid() const { return pid_; }

        // Disable default constructor and copy operations
        process() = delete;
        process(const process&) = delete;
        process& operator=(const process&) = delete;

        // Destructor: clean up inferior processes we launch, leave running otherwise
        ~process();

        process_state state() const { return state_; }

    private:
        pid_t pid_ = 0;
        bool terminate_on_end_ = true;
        process_state state_ = process_state::stopped;

        // Constructor to be used by static members
        process(pid_t pid, bool terminate_on_end) :
            pid_(pid), terminate_on_end_(terminate_on_end) {}
    };
}

#endif //SDB_PROCESS_H