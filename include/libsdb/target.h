//
// Created by root on 10/29/26.
//

#ifndef SDB_TARGET_H
#define SDB_TARGET_H

#include <memory>
#include <libsdb/breakpoint.h>
#include <libsdb/elf.h>
#include <libsdb/dwarf.h>
#include <libsdb/process.h>
#include <libsdb/stack.h>

namespace sdb {
    // Manage the symbolic level of the program being debugged e.g. carrying out
    // debugger operations at the level of the source code
    class target {
    public:
        target() = delete;
        target(const target&) = delete;
        target& operator=(const target&) = delete;

        static std::unique_ptr<target> launch(
            const std::filesystem::path& path,
            std::optional<int> stdout_replacement = std::nullopt);
        static std::unique_ptr<target> attach(pid_t pid);

        process& get_process() { return *process_; }
        const process& get_process() const { return *process_; }
        elf& get_elf() { return *elf_; }
        const elf& get_elf() const { return *elf_; }

        void notify_stop(const stop_reason& reason);

        // Convert program counter from virtual address to file address
        file_addr get_pc_file_address() const;

        stack& get_stack() { return stack_; }
        const stack& get_stack() const { return stack_; }

        // Source-level stepping
        stop_reason step_in();
        stop_reason step_out();
        stop_reason step_over();

        line_table::iterator line_entry_at_pc() const;
        stop_reason run_until_address(virt_addr address);

        struct find_functions_result {
            std::vector<die> dwarf_functions;
            std::vector<std::pair<const elf*, const Elf64_Sym*>> elf_functions;
        };
        find_functions_result find_functions(std::string name) const;

        breakpoint& create_address_breakpoint(virt_addr address,
            bool hardware = false, bool internal = false);
        breakpoint& create_function_breakpoint(std::string function_name,
            bool hardware = false, bool internal = false);
        breakpoint& create_line_breakpoint(std::filesystem::path file,
            std::size_t line, bool hardware = false, bool internal = false);

        stoppoint_collection<breakpoint>& breakpoints() { return breakpoints_; }
        const stoppoint_collection<breakpoint>& breakpoints() const {
            return breakpoints_;
        }

    private:
        target(std::unique_ptr<process> proc, std::unique_ptr<elf> obj)
            : process_(std::move(proc)), elf_(std::move(obj)), stack_(this) {}

        std::unique_ptr<process> process_;
        std::unique_ptr<elf> elf_;
        stack stack_;
        stoppoint_collection<breakpoint> breakpoints_;
    };
}

#endif //SDB_TARGET_H
