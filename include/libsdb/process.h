//
// Created by root on 3/7/26.
//

#ifndef SDB_PROCESS_H
#define SDB_PROCESS_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <sys/types.h>
#include <libsdb/registers.h>

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
        static std::unique_ptr<process> launch(const std::filesystem::path& path,
                                                bool debug = true,
                                                std::optional<int> stdout_replacement = std::nullopt);
        static std::unique_ptr<process> attach(pid_t pid);

        void resume();
        stop_reason wait_on_signal();

        // Disable default constructor and copy operations
        process() = delete;
        process(const process&) = delete;
        process& operator=(const process&) = delete;

        // Destructor: clean up inferior processes we launch, leave running otherwise
        ~process();

        // Process fields getters
        pid_t pid() const { return pid_; }
        process_state state() const { return state_; }

        registers& get_registers() { return *registers_; }
        const registers& get_registers() const { return *registers_; }

        void write_user_area(std::size_t offset, std::uint64_t data);

        // Write to all x87 registers
        void write_fprs(const user_fpregs_struct& fprs);
        // Write to all GPRs
        void write_gprs(const user_regs_struct& gprs);

    private:
        pid_t pid_ = 0;
        bool terminate_on_end_ = true;
        bool is_attached_ = true;
        process_state state_ = process_state::stopped;
        std::unique_ptr<registers> registers_;

        // Constructor to be used by static members
        process(pid_t pid, bool terminate_on_end, bool is_attached) :
            pid_(pid),
            terminate_on_end_(terminate_on_end),
            is_attached_(is_attached),
            registers_(new registers(*this))
        {}

        void read_all_registers();
    };
}

#endif //SDB_PROCESS_H
