//
// Created by root on 3/7/26.
//

#include <libsdb/error.h>
#include <libsdb/process.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

sdb::stop_reason::stop_reason(int wait_status) {
    if (WIFEXITED(wait_status)) {
        reason = process_state::exited;
        info = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        reason = process_state::terminated;
        info = WTERMSIG(wait_status);
    } else if (WIFSTOPPED(wait_status)) {
        reason = process_state::stopped;
        info = WSTOPSIG(wait_status);
    }
}

std::unique_ptr<sdb::process> sdb::process::launch(const std::filesystem::path& path) {
    pid_t pid;
    if ((pid = fork()) < 0) {
        error::send_errno("Fork failed");
    }

    if (pid == 0) {
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
            error::send_errno("Tracing failed");
        }

        if (execlp(path.c_str(), path.c_str(), nullptr) < 0) {
            error::send_errno("Exec failed");
        }
    }

    std::unique_ptr<process> proc (new process(pid, true));
    proc->wait_on_signal();

    return proc;
}

std::unique_ptr<sdb::process> sdb::process::attach(pid_t pid) {
    if (pid == 0) {
        error::send("Invalid PID");
    }

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
        error::send_errno("Could not attach");
    }

    std::unique_ptr<process> proc (new process(pid, false));
    proc->wait_on_signal();

    return proc;
}

sdb::stop_reason sdb::process::wait_on_signal() {
    int wait_status;
    if (waitpid(pid_, &wait_status, 0) < 0) {
        error::send_errno("waitpid failed");
    }
    stop_reason reason(wait_status);
    state_ = reason.reason;
    return reason;
}

sdb::process::~process() {
    if (pid_ != 0) {
        int status;
        // Inferior process must be stopped for detach to work
        if (state_ == process_state::running) {
            kill(pid_, SIGSTOP);
            waitpid(pid_, &status, 0);
        }
        ptrace(PTRACE_DETACH, pid_, nullptr, nullptr);
        kill(pid_, SIGCONT);

        if (terminate_on_end_) {
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
    }
}

void sdb::process::resume() {
    if (ptrace(PTRACE_CONT, pid_, nullptr, nullptr) < 0) {
        error::send_errno("Could not resume");
    }
    state_ = process_state::running;
}