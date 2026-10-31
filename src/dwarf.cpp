//
// Created by root on 10/30/26.
//

#include <algorithm>
#include <libsdb/bit.h>
#include <libsdb/dwarf.h>
#include <libsdb/error.h>
#include <libsdb/elf.h>

namespace {
    // Points to a location in the DWARF information for parsing
    class cursor {
    public:
        explicit cursor(sdb::span<const std::byte> data)
            : data_(data), pos_(data.begin()) {}

        cursor& operator++() { ++pos_; return *this; }
        cursor& operator+=(std::size_t size) { pos_ += size; return *this; }

        const std::byte* position() const { return pos_; }

        bool is_finished() const {
            return pos_ >= data_.end();
        }

        // Parse any fixed-width integer
        template <class T>
        T fixed_int() {
            auto t = sdb::from_bytes<T>(pos_);
            pos_ += sizeof(T);
            return t;
        }

        std::uint8_t u8() { return fixed_int<std::uint8_t>(); }
        std::uint16_t u16() { return fixed_int<std::uint16_t>(); }
        std::uint32_t u32() { return fixed_int<std::uint32_t>(); }
        std::uint64_t u64() { return fixed_int<std::uint64_t>(); }
        std::int8_t s8() { return fixed_int<std::int8_t>(); }
        std::int16_t s16() { return fixed_int<std::int16_t>(); }
        std::int32_t s32() { return fixed_int<std::int32_t>(); }
        std::int64_t s64() { return fixed_int<std::int64_t>(); }

        // Parse strings
        std::string_view string() {
            auto null_terminator = std::find(pos_, data_.end(), std::byte{0});
            std::string_view ret(reinterpret_cast<const char *>(pos_),
                null_terminator - pos_);
            pos_ = null_terminator + 1;
            return ret;
        }

        // Parse unsigned int Little Endian Base 128
        std::uint64_t uleb128() {
            std::uint64_t res = 0;
            int shift = 0;
            std::uint8_t byte = 0;
            do {
                byte = u8();
                auto masked = static_cast<uint64_t>(byte & 0x7f);
                res |= masked << shift;
                shift += 7;
            } while ((byte & 0x80) != 0);
            return res;
        }

        // Parse signed int Little Endian Base 128
        std::uint64_t sleb128() {
            std::uint64_t res = 0;
            int shift = 0;
            std::uint8_t byte = 0;
            do {
                byte = u8();
                auto masked = static_cast<uint64_t>(byte & 0x7f);
                res |= masked << shift;
                shift += 7;
            } while ((byte & 0x80) != 0);

            // Check if we filled the result integer. If not, fill remaining
            // high bits of the result
            if ((shift < sizeof(res) * 8) and (byte & 0x40)) {
                res |= (~static_cast<std::uint64_t>(0) << shift);
            }
            return res;
        }

        // DWARF form skipping
        void skip_form(std::uint64_t form) {
            switch (form) {
                // Fixed-width forms
                case DW_FORM_flag_present:
                    break;

                case DW_FORM_data1:
                case DW_FORM_ref1:
                case DW_FORM_flag:
                    pos_ += 1; break;

                case DW_FORM_data2:
                case DW_FORM_ref2:
                    pos_ += 2; break;

                case DW_FORM_data4:
                case DW_FORM_ref4:
                case DW_FORM_ref_addr:
                case DW_FORM_sec_offset:
                case DW_FORM_strp:
                    pos_ += 4; break;

                case DW_FORM_data8:
                case DW_FORM_addr:
                    pos_ += 8; break;

                // ULEB128 and SLED128 integer forms
                case DW_FORM_sdata:
                    sleb128(); break;
                case DW_FORM_udata:
                case DW_FORM_ref_udata:
                    uleb128(); break;

                // Explicitly sized forms
                case DW_FORM_block1:
                    pos_ += u8();
                    break;
                case DW_FORM_block2:
                    pos_ += u16();
                    break;
                case DW_FORM_block4:
                    pos_ += u32();
                    break;
                case DW_FORM_block:
                case DW_FORM_exprloc:
                    pos_ += uleb128();
                    break;

                // Strings (DW_FORM_string)
                case DW_FORM_string:
                    while (!is_finished() && *pos_ != std::byte(0)) {
                        ++pos_;
                    }
                    ++pos_; // past null terminator
                    break;

                // Indirect forms (encoded as ULEB128 that gives it actual form)
                case DW_FORM_indirect:
                    skip_form(uleb128());
                    break;

                default:
                    sdb::error::send("Unrecognized DWARF form");
            }
        }

    private:
        sdb::span<const std::byte> data_;
        const std::byte* pos_;
    };

    std::unordered_map<std::uint64_t, sdb::abbrev>
    parse_abbrev_table(const sdb::elf& obj, std::size_t offset) {
        cursor cur(obj.get_section_contents(".debug_abbrev"));
        cur += offset;

        std::unordered_map<std::uint64_t, sdb::abbrev> table;
        std::uint64_t code = 0;
        do {
            code = cur.uleb128();
            auto tag = cur.uleb128();
            auto has_children = static_cast<bool>(cur.u8());

            std::vector<sdb::attr_spec> attr_specs;
            std::uint64_t attr = 0;
            do {
                attr = cur.uleb128();
                auto form = cur.uleb128();
                if (attr != 0) {
                    attr_specs.push_back(sdb::attr_spec{ attr, form });
                }
            } while (attr != 0);

            if (code != 0) {
                table.emplace(code,
                    sdb::abbrev{ code, tag, has_children, std::move(attr_specs) });
            }
        } while (code != 0);

        return table;
    }

    std::unique_ptr<sdb::compile_unit> parse_compile_unit(
          sdb::dwarf& dwarf, const sdb::elf& obj, cursor cur) {
        auto start = cur.position();
        auto size = cur.u32();
        auto version = cur.u16();
        auto abbrev = cur.u32();
        auto address_size = cur.u8();

        if (size == 0xffffffff) {
            sdb::error::send("Only DWARF32 is supported");
        }
        if (version != 4) {
            sdb::error::send("Only DWARF version 4 is supported");
        }
        if (address_size != 8) {
            sdb::error::send("Invalid address size for DWARF");
        }

        size += sizeof(std::uint32_t);

        sdb::span<const std::byte> data = { start, size };
        return std::make_unique<sdb::compile_unit>(dwarf, data, abbrev);
    }

    std::vector<std::unique_ptr<sdb::compile_unit>> parse_compile_units(
            sdb::dwarf& dwarf, const sdb::elf& obj) {
        auto debug_info = obj.get_section_contents(".debug_info");
        cursor cur(debug_info);

        std::vector<std::unique_ptr<sdb::compile_unit>> units;
        while (!cur.is_finished()) {
            auto unit = parse_compile_unit(dwarf, obj, cur);
            cur += unit->data().size();
            units.push_back(std::move(unit));
        }

        return units;
    }

    sdb::die parse_die(const sdb::compile_unit& cu, cursor cur) {
        auto pos = cur.position();
        auto abbrev_code = cur.uleb128();

        if (abbrev_code == 0) { // null DIE
            auto next = cur.position();
            return sdb::die{ next };
        }

        // Get abbreviation entry
        auto& abbrev_table = cu.abbrev_table();
        auto& abbrev = abbrev_table.at(abbrev_code);

        // Find location of the next DIE and precompute the locations for
        // each attribute of this DIE
        std::vector<const std::byte*> attr_locs;
        attr_locs.reserve(abbrev.attr_specs.size());
        for (auto& attr : abbrev.attr_specs) {
            attr_locs.push_back(cur.position());
            cur.skip_form(attr.form);
        }

        auto next = cur.position();
        return {pos, &cu, &abbrev, std::move(attr_locs), next};
    }
}

sdb::dwarf::dwarf(const elf &parent) : elf_(&parent) {
    compile_units_ = parse_compile_units(*this, parent);
}

const std::unordered_map<std::uint64_t, sdb::abbrev> &
sdb::dwarf::get_abbrev_table(std::size_t offset) {
    if (!abbrev_tables_.count(offset)) {
        abbrev_tables_.emplace(offset, parse_abbrev_table(*elf_, offset));
    }
    return abbrev_tables_.at(offset);
}

const std::unordered_map<std::uint64_t, sdb::abbrev> &
sdb::compile_unit::abbrev_table() const {
    return parent_->get_abbrev_table(abbrev_offset_);
}

sdb::die sdb::compile_unit::root() const {
    std::size_t header_size = 11;
    cursor cur({ data_.begin() + header_size, data_.end() });
    return parse_die(*this, cur);
}

sdb::die::children_range sdb::die::children() const {
    return {*this};
}

sdb::die::children_range::iterator::iterator(const die &die) {
    cursor next_cur({ die.next_, die.cu_->data().end() });
    die_ = parse_die(*die.cu_, next_cur);
}

sdb::die::children_range::iterator &
sdb::die::children_range::iterator::operator++() { // pre-increment
    if (!die_.has_value() or !die_->abbrev_) return *this;

    if (!die_->abbrev_->has_children) { // next DIE is next sibling
        cursor next_cur({ die_->next_, die_->cu_->data().end() });
        die_ = parse_die(*die_->cu_, next_cur);
    } else { // next DIE after children entries is the next sibling
        iterator sub_children(*die_);
        while (sub_children->abbrev_) ++sub_children;
        cursor next_cur({ sub_children->next_, die_->cu_->data().end() });
        die_ = parse_die(*die_->cu_, next_cur);
    }
    return *this;
}

sdb::die::children_range::iterator
sdb::die::children_range::iterator::operator++(int) { // post-increment
    auto tmp = *this;
    ++(*this);
    return tmp;
}

bool sdb::die::children_range::iterator::operator==(const iterator& rhs) const {
    auto lhs_null = !die_.has_value() or !die_->abbrev_entry();
    auto rhs_null = !rhs.die_.has_value() or !rhs.die_->abbrev_entry();
    if (lhs_null and rhs_null) return true;
    if (lhs_null or rhs_null) return false;

    return die_->abbrev_ == rhs->abbrev_ and die_->next() == rhs->next();
}
