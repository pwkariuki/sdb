//
// Created by root on 10/29/26.
//

#include <csignal>
#include <optional>
#include <libsdb/bit.h>
#include <libsdb/disassembler.h>
#include <libsdb/target.h>
#include <libsdb/types.h>

namespace {
    // Get the auxiliary vector for the process, create an elf object for the
    // file at the path, and set the load address of its `.text` section
    std::unique_ptr<sdb::elf> create_loaded_elf(
        const sdb::process& proc, const std::filesystem::path& path) {
        auto auxv = proc.get_auxv();
        auto obj = std::make_unique<sdb::elf>(path);
        obj->notify_loaded(
            sdb::virt_addr(auxv[AT_ENTRY] - obj->get_header().e_entry));
        return obj;
    }
}

std::unique_ptr<sdb::target>
sdb::target::launch(const std::filesystem::path& path, std::optional<int> stdout_replacement) {
    auto proc = process::launch(path, true, stdout_replacement);
    auto obj = create_loaded_elf(*proc, path);
    auto tgt = std::unique_ptr<target>(
        new target(std::move(proc), std::move(obj)));
    tgt->get_process().set_target(tgt.get());
    return tgt;
}

std::unique_ptr<sdb::target> sdb::target::attach(pid_t pid) {
    auto elf_path = std::filesystem::path("/proc") / std::to_string(pid) / "exe";
    auto proc = process::attach(pid);
    auto obj = create_loaded_elf(*proc, elf_path);
    auto tgt = std::unique_ptr<target>(
        new target(std::move(proc), std::move(obj)));
    tgt->get_process().set_target(tgt.get());
    return tgt;
}

void sdb::target::notify_stop(const stop_reason &reason) {
    stack_.reset_inline_height();
}

sdb::file_addr sdb::target::get_pc_file_address() const {
    return process_->get_pc().to_file_addr(*elf_);
}

sdb::stop_reason sdb::target::step_in() {
    auto& stack = get_stack();
    if (stack.inline_height() > 0) { // stopped in the caller, jump in callee
        stack.simulate_inlined_step_in();
        return {process_state::stopped, SIGTRAP, trap_type::single_step};
    }

    // Step to a new line of source code
    auto orig_line = line_entry_at_pc();
    do {
        auto reason = process_->step_instruction();
        if (!reason.is_step()) {
            return reason;
        }
    } while ((line_entry_at_pc() == orig_line
        or line_entry_at_pc()->end_sequence)
        and line_entry_at_pc() != line_table::iterator{});

    // Skip prologue
    auto pc = get_pc_file_address();
    if (pc.elf_file() != nullptr) {
        auto& dwarf = pc.elf_file()->get_dwarf();
        auto func = dwarf.function_containing_address(pc);
        if (func and func->low_pc() == pc) {
            auto line = line_entry_at_pc();
            if (line != line_table::iterator{}) {
                ++line;
                return run_until_address(line->address.to_virt_addr());
            }
        }
    }

    return {process_state::stopped, SIGTRAP, trap_type::single_step};
}

sdb::stop_reason sdb::target::step_out() {
    auto& stack = get_stack();
    auto inline_stack = stack.inline_stack_at_pc();
    auto has_inline_frames = inline_stack.size() > 1;
    auto at_inline_frame = stack.inline_height() < inline_stack.size() - 1;

    if (has_inline_frames and at_inline_frame) { // step to end of inline frame
        const auto& current_frame =
            inline_stack[inline_stack.size() - stack.inline_height() - 1];
        auto return_address = current_frame.high_pc().to_virt_addr();
        return run_until_address(return_address);
    }

    auto frame_pointer = process_->get_registers()
        .read_by_id_as<std::uint64_t>(register_id::rbp);

    auto return_address = process_->read_memory_as<std::uint64_t>(
        virt_addr{ frame_pointer + 8 });

    return run_until_address(virt_addr{ return_address });
}

sdb::stop_reason sdb::target::step_over() {
    auto orig_line = line_entry_at_pc();
    disassembler dis(*process_);
    stop_reason reason;
    auto& stack = get_stack();
    do {
        do {
            auto inline_stack = stack.inline_stack_at_pc();
            if (stack.inline_height() > 0) { // jump over inlined function
                const auto& frame_to_skip =
                    inline_stack[inline_stack.size() - stack.inline_height()];
                auto return_address = frame_to_skip.high_pc().to_virt_addr();
                reason = run_until_address(return_address);
                if (!reason.is_step()
                    or process_->get_pc() != return_address) {
                    return reason;
                    }
            } else if (auto instructions = dis.disassemble(2, process_->get_pc());
                instructions[0].text.rfind("call") == 0) { // jump over call
                reason = run_until_address(instructions[1].address);
                if (!reason.is_step()
                    or process_->get_pc() != instructions[1].address) {
                    return reason;
                }
            } else { // step over single instruction
                reason = process_->step_instruction();
                if (!reason.is_step()) return reason;
            }
        } while (line_entry_at_pc() == orig_line or
            line_entry_at_pc()->end_sequence);
    } while ((line_entry_at_pc() == orig_line
        or line_entry_at_pc()->end_sequence)
        and line_entry_at_pc() != line_table::iterator{});
    return reason;
}

sdb::line_table::iterator sdb::target::line_entry_at_pc() const {
    auto pc = get_pc_file_address();
    if (!pc.elf_file()) return line_table::iterator{};
    auto cu = pc.elf_file()->get_dwarf().compile_unit_containing_address(pc);
    if (!cu) return line_table::iterator{};
    return cu->lines().get_entry_by_address(pc);
}

// Set temporary breakpoint, run until breakpoint, then remove breakpoint
sdb::stop_reason sdb::target::run_until_address(virt_addr address) {
    breakpoint_site* breakpoint_to_remove = nullptr;
    if (!process_->breakpoint_sites().contains_address(address)) {
        breakpoint_to_remove = &process_->create_breakpoint_site(
            address, false, true);
        breakpoint_to_remove->enable();
    }

    process_->resume();
    auto reason = process_->wait_on_signal();
    if (reason.is_breakpoint()
        and process_->get_pc() == address) {
        reason.trap_reason = trap_type::single_step;
    }

    if (breakpoint_to_remove) {
        process_->breakpoint_sites().remove_by_address(
            breakpoint_to_remove->address());
    }

    return reason;
}
