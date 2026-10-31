//
// Created by root on 10/29/26.
//

#ifndef SDB_DWARF_H
#define SDB_DWARF_H

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <libsdb/detail/dwarf.h>
#include <libsdb/types.h>

namespace sdb {
    class elf;
    class compile_unit;
    class die;

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

        // Obtain the root DIE for the compile unit
        die root() const;

    private:
        dwarf* parent_;
        span<const std::byte> data_; // unit's data in `.debug_info` section
        std::size_t abbrev_offset_; // unit's offset to the abbreviation table
    };

    // Debugging Information Entry
    class die {
    public:
        explicit die(const std::byte* next) : next_(next) {}
        die(const std::byte* pos, const compile_unit* cu, const abbrev* abbrev,
            std::vector<const std::byte*> attr_locs, const std::byte* next) :
            pos_(pos), cu_(cu), abbrev_(abbrev),
            attr_locs_(std::move(attr_locs)), next_(next) {}

        const compile_unit* cu() const { return cu_; }
        const abbrev* abbrev_entry() const { return abbrev_; }
        const std::byte* position() const { return pos_; }
        const std::byte* next() const { return next_; }

        // Range type to iterate over children of DIEs
        class children_range;
        children_range children() const;

    private:
        const std::byte* pos_ = nullptr;
        const compile_unit* cu_ = nullptr; // own compile unit
        const abbrev* abbrev_ = nullptr; // own abbreviation table entry
        const std::byte* next_ = nullptr; // DIE immediately after (child/sibling)
        std::vector<const std::byte*> attr_locs_; // locations of DIE attributes
    };

    class die::children_range {
    public:
        children_range(die die) : die_(std::move(die)) {}

        class iterator {
        public:
            using value_type = die;
            using reference = const die&;
            using pointer = const die*;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            iterator() = default;
            iterator(const iterator&) = default;
            iterator& operator=(const iterator&) = default;

            explicit iterator(const die& die);

            const die& operator*() const { return *die_; }
            const die* operator->() const { return &die_.value(); }

            iterator& operator++();
            iterator operator++(int);

            bool operator==(const iterator& rhs) const;
            bool operator!=(const iterator& rhs) const {
                return !(*this == rhs);
            }

        private:
            std::optional<die> die_;
        };

        iterator begin() const {
            if (die_.abbrev_->has_children) {
                return iterator{ die_ };
            }
            return end();
        }
        iterator end() const { return iterator{}; }

    private:
        die die_;
    };
}

#endif //SDB_DWARF_H
