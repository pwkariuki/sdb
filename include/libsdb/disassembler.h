//
// Created by root on 7/27/26.
//

#ifndef SDB_DISASSEMBLER_H
#define SDB_DISASSEMBLER_H

#include <optional>
#include <libsdb/process.h>

namespace sdb {
    class disassembler {
        // Instruction type holds a string representation of the instruction and
        // the memory address where the corresponding binary instructio to is storedn is stored
        struct instruction {
            virt_addr address;
            std::string text;
        };

    public:
        disassembler(process& proc) : process_(&proc) {}

        std::vector<instruction> disassemble(
            std::size_t n_instructions,
            std::optional<virt_addr> address = std::nullopt);

    private:
        process* process_;
    };
}

#endif //SDB_DISASSEMBLER_H
