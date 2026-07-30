//
// Created by root on 3/4/26.
//

#include <algorithm>
#include <editline/readline.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>
#include <libsdb/disassembler.h>
#include <libsdb/error.h>
#include <libsdb/parse.h>
#include <libsdb/process.h>

namespace {
    std::unique_ptr<sdb::process> attach(int argc, const char **argv) {
        // Passing PID
        if (argc == 3 && argv[1] == std::string_view("-p")) {
            pid_t pid = std::atoi(argv[2]);
            return sdb::process::attach(pid);
        }
        // Passing program name
        const char* program_path = argv[1];
        auto proc = sdb::process::launch(program_path);
        fmt::println("Launched process with PID {}", proc->pid());
        return proc;
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

    void print_stop_reason(const sdb::process& process, sdb::stop_reason reason) {
       std::string message;

        switch (reason.reason) {
            case sdb::process_state::exited:
                message = fmt::format("exited with status {}",
                     static_cast<int>(reason.info));
                break;
            case sdb::process_state::terminated:
                message = fmt::format("terminated with signal {}",
                    sigabbrev_np(reason.info));
                break;
            case sdb::process_state::stopped:
                message = fmt::format("stopped with signal {} at {:#x}",
                    sigabbrev_np(reason.info), process.get_pc().addr());
                break;
            default:
                break;
        }

        fmt::println("Process {} {}", process.pid(), message);
    }

    void print_help(const std::vector<std::string>& args) {
        if (args.size() == 1) {
            std::cerr << R"(Available commands:
    breakpoint  - Commands for operating on breakpoints
    continue    - Resume the process
    disassemble - Disassemble machine code to assembly
    memory      - Commands for operating on memory
    register    - Commands for operating on registers
    step        - Step over a single instruction
    watchpoint  - Commands for operating on watchpoints
)";
        }
        else if (is_prefix(args[1], "breakpoint")) {
            std::cerr << R"(Available commands:
    list
    delete <id>
    disable <id>
    enable <id>
    set <address>
    set <address> -h
)";
        }
        else if (is_prefix(args[1], "disassemble")) {
            std::cerr << R"(Available options:
    -c <number of instructions>
    -a <start address>
)";
        }
        else if (is_prefix(args[1], "memory")) {
            std::cerr << R"(Available commands:
    read <address>
    read <address> <number of bytes>
    write <address> <bytes>
)";
        }
        else if (is_prefix(args[1], "register")) {
            std::cerr << R"(Available commands:
     read
     read <register>
     read all
     write <register> <value>
)";
        }
        else if (is_prefix(args[1], "watchpoint")) {
            std::cerr << R"(Available commands:
    list
    delete <id>
    disable <id>
    enable <id>
    set <address> <write|rw|execute> <size>
)";
        }
        else {
            std::cerr << "No help available on that" << std::endl;
        }
    }

    void print_disassembly(sdb::process& process,
          sdb::virt_addr address, std::size_t n_instructions) {
        sdb::disassembler dis(process);
        auto instructions = dis.disassemble(n_instructions, address);
        for (auto& instr : instructions) {
            fmt::println("{:#018x}: {}", instr.address.addr(), instr.text);
        }
    }

    void handle_stop(sdb::process& process, sdb::stop_reason reason) {
        print_stop_reason(process, reason);
        if (reason.reason == sdb::process_state::stopped) {
            print_disassembly(process, process.get_pc(), 5);
        }
    }

    void handle_register_read(sdb::process& process,
        const std::vector<std::string>& args) {
        auto format = [](auto t) {
            if constexpr (std::is_floating_point_v<decltype(t)>) {
                return fmt::format("{}", t);
            } else if constexpr (std::is_integral_v<decltype(t)>) {
                return fmt::format("{:#0{}x}", t, sizeof(t) * 2 + 2);
            } else {
                return fmt::format("[{:#04x}]", fmt::join(t, ","));
            }
        };

        if (args.size() == 2 or
            (args.size() == 3 && args[2] == "all")) {
            for (auto& info : sdb::g_register_infos) {
                auto should_print = (args.size() == 3 or
                    info.type == sdb::register_type::gpr) and
                    info.name != "orig_rax";
                if (!should_print) continue;
                auto value = process.get_registers().read(info);
                fmt::print("{}:\t{}\n", info.name, std::visit(format, value));
            }
        } else if (args.size() == 3) {
            try {
                auto info = sdb::register_info_by_name(args[2]);
                auto value = process.get_registers().read(info);
                fmt::print("{}:\t{}\n", info.name, std::visit(format, value));
            } catch (sdb::error& err) {
                std::cerr << "No such register" << std::endl;
                return;
            }
        } else {
            print_help({ "help", "register" });
        }
    }

    sdb::registers::value parse_register_value(
        sdb::register_info info, std::string_view text) {
        try {
            if (info.format == sdb::register_format::uint) {
                switch (info.size) {
                    case 1: return sdb::to_integral<std::uint8_t>(text, 16).value();
                    case 2: return sdb::to_integral<std::uint16_t>(text, 16).value();
                    case 4: return sdb::to_integral<std::uint32_t>(text, 16).value();
                    case 8: return sdb::to_integral<std::uint64_t>(text, 16).value();
                }
            } else if (info.format == sdb::register_format::double_float) {
                return sdb::to_float<double>(text).value();
            } else if (info.format == sdb::register_format::long_double) {
                return sdb::to_float<long double>(text).value();
            } else if (info.format == sdb::register_format::vector) {
                if (info.size == 8) {
                    return sdb::parse_vector<8>(text);
                }
                if (info.size == 16) {
                    return sdb::parse_vector<16>(text);
                }
            }
        } catch (...) {}
        sdb::error::send("Invalid format");
    }

    void handle_register_write(sdb::process& process,
        const std::vector<std::string>& args) {
        if (args.size() != 4) {
            print_help({ "help", "register" });
            return;
        }

        try {
            auto info = sdb::register_info_by_name(args[2]);
            auto value = parse_register_value(info, args[3]);
            process.get_registers().write(info, value);
        } catch (sdb::error& err) {
            std::cerr << err.what() << std::endl;
            return;
        }
    }

    void handle_register_command(sdb::process& process,
        const std::vector<std::string>& args) {
        if (args.size() < 2) {
            print_help({ "help", "register" });
            return;
        }

        if (is_prefix(args[1], "read")) {
            handle_register_read(process, args);
        } else if (is_prefix(args[1], "write")) {
            handle_register_write(process, args);
        } else {
            print_help({ "help", "register" });
        }
    }

    void handle_breakpoint_command(sdb::process& process,
        const std::vector<std::string>& args) {
        if (args.size() < 2) {
            print_help({ "help", "breakpoint" });
            return;
        }

        const auto& command = args[1];

        if (is_prefix(command, "list")) {
            if (process.breakpoint_sites().empty()) {
                fmt::println("No breakpoints set");
            } else {
                fmt::println("Current breakpoints:");
                process.breakpoint_sites().for_each([](auto& site) {
                    if (site.is_internal()) return;
                    fmt::println("{}: address = {:#x}, {}",
                        site.id(), site.address().addr(),
                        site.is_enabled() ? "enabled" : "disabled");
                });
            }
            return;
        }

        if (args.size() < 3) {
            print_help({ "help", "breakpoint" });
            return;
        }

        if (is_prefix(command, "set")) {
            auto address = sdb::to_integral<std::uint64_t>(args[2], 16);

            if (!address) {
                fmt::println(stderr,
                    "Breakpoint command expects address in "
                    "hexadecimal, prefixed with '0x'");
                return;
            }

            bool hardware = false;
            if (args.size() == 4) {
                if (args[3] == "-h") hardware = true;
                else sdb::error::send("Invalid breakpoint command argument");
            }

            process.create_breakpoint_site(sdb::virt_addr{ *address }, hardware).enable();
            return;
        }

        auto id = sdb::to_integral<sdb::breakpoint_site::id_type>(args[2]);
        if (!id) {
            std::cerr << "Command expects breakpoint id";
            return;
        }

        if (is_prefix(command, "enable")) {
            process.breakpoint_sites().get_by_id(*id).enable();
        } else if (is_prefix(command, "disable")) {
            process.breakpoint_sites().get_by_id(*id).disable();
        } else if (is_prefix(command, "delete")) {
            process.breakpoint_sites().remove_by_id(*id);
        }
    }

    // Reads 32 bytes by default from specified address
    // unless user provides the amount to read as an argument
    void handle_memory_read_command(sdb::process& process,
        const std::vector<std::string>& args) {
        auto address = sdb::to_integral<std::uint64_t>(args[2], 16);
        if (!address) {
            sdb::error::send("Invalid address format");
        }

        std::size_t n_bytes = 32;
        if (args.size() == 4) {
            auto bytes_arg = sdb::to_integral<std::size_t>(args[3]);
            if (!bytes_arg) {
                sdb::error::send("Invalid number of bytes");
            }
            n_bytes = *bytes_arg;
        }

        auto data = process.read_memory(sdb::virt_addr{ *address }, n_bytes);

        // Print out the memory in batches of 16 bytes in hexadecimal
        for (std::size_t i = 0; i < data.size(); i += 16) {
            auto start = data.begin() + i;
            auto end = data.begin() + std::min(i + 16, data.size());
            fmt::println("{:#016x}: {:02x}",
                *address + i, fmt::join(start, end, " "));
        }
    }

    void handle_memory_write_command(sdb::process& process,
        const std::vector<std::string>& args) {
        if (args.size() != 4) {
            print_help({ "help", "memory" });
            return;
        }

        auto address = sdb::to_integral<std::uint64_t>(args[2], 16);
        if (!address) {
            sdb::error::send("Invalid address format");
        }

        auto data = sdb::parse_vector(args[3]);
        process.write_memory(
            sdb::virt_addr{ *address }, { data.data(), data.size() });
    }

    void handle_memory_command(sdb::process& process,
        const std::vector<std::string>& args) {
        if (args.size() < 3) {
            print_help({ "help", "memory" });
            return;
        }

        if (is_prefix(args[1], "read")) {
            handle_memory_read_command(process, args);
        } else if (is_prefix(args[1], "write")) {
            handle_memory_write_command(process, args);
        } else {
            print_help({ "help", "memory" });
        }
    }

    void handle_disassemble_command(sdb::process& process,
        const std::vector<std::string> args) {
        // Default disassemble parameters
        auto address = process.get_pc();
        std::size_t n_instructions = 5;

        // Fill in address and n_instructions with user specifications
        auto it = args.begin() + 1;
        while (it != args.end()) {
            if (*it == "-a" and it + 1 != args.end()) {
                ++it;
                auto opt_addr = sdb::to_integral<std::uint64_t>(*it++, 16);
                if (!opt_addr) {
                    sdb::error::send("Invalid address format");
                }
                address = sdb::virt_addr{ *opt_addr };
            } else if (*it == "-c" and it + 1 != args.end()) {
                ++it;
                auto opt_n = sdb::to_integral<std::size_t>(*it++);
                if (!opt_n) {
                    sdb::error::send("Invalid instruction count");
                }
                n_instructions = *opt_n;
            } else {
                print_help({ "help", "disassemble" });
                return;
            }
        }

        print_disassembly(process, address, n_instructions);
    }

    void handle_watchpoint_list(sdb::process& process,
        const std::vector<std::string>& args) {
        auto stoppoint_mode_to_string = [](auto mode) {
            switch (mode) {
                case sdb::stoppoint_mode::execute: return "execute";
                case sdb::stoppoint_mode::write: return "write";
                case sdb::stoppoint_mode::read_write: return "read_write";
                default: sdb::error::send("Invalid stoppoint mode");
            }
        };

        if (process.watchpoints().empty()) {
            fmt::println("No watchpoints set");
        } else {
            fmt::println("Current watchpoints:");
            process.watchpoints().for_each([&](auto& point) {
                fmt::println("{}: address = {:#x}, mode = {}, size = {}, {}",
                point.id(), point.address().addr(),
                stoppoint_mode_to_string(point.mode()), point.size(),
                point.is_enabled() ? "enabled" : "disabled");
            });
        }
    }

    void handle_watchpoint_set(sdb::process& process,
        const std::vector<std::string>& args) {
        if (args.size() != 5) {
            print_help({ "help", "watchpoint" });
            return;
        }

        auto address = sdb::to_integral<std::uint64_t>(args[2], 16);
        auto& mode_text = args[3];
        auto size = sdb::to_integral<std::size_t>(args[4]);

        if (!address or !size or
            !(mode_text == "write" or mode_text == "rw" or
            mode_text == "execute")) {
            print_help({ "help", "watchpoint" });
            return;
        }

        sdb::stoppoint_mode mode;
        if (mode_text == "write") mode = sdb::stoppoint_mode::write;
        else if (mode_text == "rw") mode = sdb::stoppoint_mode::read_write;
        else if (mode_text == "execute") mode = sdb::stoppoint_mode::execute;

        process.create_watchpoint(
            sdb::virt_addr{ *address }, mode, *size).enable();
    }

    void handle_watchpoint_command(sdb::process& process,
        const std::vector<std::string>& args) {
        if (args.size() < 2) {
            print_help({ "help", "watchpoint" });
            return;
        }

        auto& command = args[1];

        if (is_prefix(command, "list")) {
            handle_watchpoint_list(process, args);
            return;
        }

        if (is_prefix(command, "set")) {
            handle_watchpoint_set(process, args);
            return;
        }

        if (args.size() < 3) {
            print_help({ "help", "watchpoint"} );
            return;
        }

        auto id = sdb::to_integral<sdb::watchpoint::id_type>(args[2]);
        if (!id) {
            std::cerr << "Command expects watchpoint id";
            return;
        }

        if (is_prefix(command, "enable")) {
            process.watchpoints().get_by_id(*id).enable();
        } else if (is_prefix(command, "disable")) {
            process.watchpoints().get_by_id(*id).disable();
        } else if (is_prefix(command, "delete")) {
            process.watchpoints().remove_by_id(*id);
        }
    }

    void handle_command(std::unique_ptr<sdb::process>& process,
        std::string_view line) {
        auto args = split(line, ' ');
        const auto& command = args[0];

        if (is_prefix(command, "continue")) {
            process->resume();
            auto reason = process->wait_on_signal();
            handle_stop(*process, reason);
        } else if (is_prefix(command, "help")) {
            print_help(args);
        } else if (is_prefix(command, "register")) {
            handle_register_command(*process, args);
        } else if (is_prefix(command, "breakpoint")) {
            handle_breakpoint_command(*process, args);
        } else if (is_prefix(command, "step")) {
            auto reason = process->step_instruction();
            handle_stop(*process, reason);
        } else if (is_prefix(command, "memory")) {
            handle_memory_command(*process, args);
        } else if (is_prefix(command, "disassemble")) {
            handle_disassemble_command(*process, args);
        } else if (is_prefix(command, "watchpoint")) {
            handle_watchpoint_command(*process, args);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
        }
    }

    // Process user commands from debug console
    void main_loop(std::unique_ptr<sdb::process>& process) {
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
                try {
                    handle_command(process, line_str);
                } catch (const sdb::error& err) {
                    std::cout << err.what() << std::endl;
                }
            }
        }
    }
}

int main(int argc, const char **argv) {
    if (argc == 1) {
        std::cerr << "No arguments given" << std::endl;
        return -1;
    }

    try {
        auto process = attach(argc, argv);
        main_loop(process);
    } catch (const sdb::error& err) {
        std::cout << err.what() << std::endl;
    }
}
