//
// Created by root on 10/29/26.
//

#ifndef SDB_DWARF_H
#define SDB_DWARF_H

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <libsdb/detail/dwarf.h>

namespace sdb {
    class elf;
    class compile_unit;

    // Type to store the values held in an abbreviation table entry:
    // an abbreviation code, a tag, a flag indicating if the DIE has children,
    // and a list of attribute specifications.
    struct attr_spec {
        std::uint64_t attr;
        std::uint64_t form;
    };
    struct abbrev {
        std::uint64_t code;
        std::uint64_t tag;
        bool has_children;
        std::vector<attr_spec> attr_specs;
    };

    // Debugger's DWARF parser
    class dwarf {
    public:
        dwarf(const elf& parent);
        const elf* elf_file() const { return elf_; }

        // Parse each abbreviation table when it's first requested
        // and cache for future lookups
        const std::unordered_map<std::uint64_t, abbrev>& get_abbrev_table(
            std::size_t offset);

        const std::vector<std::unique_ptr<compile_unit>>& compile_units() const {
            return compile_units_;
        }

    private:
        const elf* elf_;
        // Map offsets to the `.debug_abbrev` tables to another map,
        // of integers to abbreviation entries
        std::unordered_map<std::size_t,
            std::unordered_map<std::uint64_t, abbrev>> abbrev_tables_;
        std::vector<std::unique_ptr<compile_unit>> compile_units_;
    };

    // Unit involved in the compilation of a program
    class compile_unit {
    public:
        compile_unit(dwarf& parent, span<const std::byte> data,
            std::size_t abbrev_offset) : parent_(&parent), data_(data),
                                         abbrev_offset_(abbrev_offset) {}

        const dwarf* dwarf_info() const { return parent_; }
        span<const std::byte> data() const { return data_; }

        // Retrieve abbreviation table for the compile unit
        const std::unordered_map<std::uint64_t, abbrev>&  abbrev_table() const;

    private:
        dwarf* parent_;
        span<const std::byte> data_; // unit's data in `.debug_info` section
        std::size_t abbrev_offset_; // unit's offset to the abbreviation table
    };
}

#endif //SDB_DWARF_H
