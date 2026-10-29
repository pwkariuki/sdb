//
// Created by root on 8/8/26.
//

#ifndef SDB_ELF_H
#define SDB_ELF_H

#include <elf.h>
#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <libsdb/types.h>

namespace sdb {
    // ELF files parser. The debugger needs to parse ELF files relevant to
    // the running process so it can retrieve debug information, locate the
    // code for a given function, and more.
    class elf {
    public:
        elf(const std::filesystem::path& path);
        ~elf();

        elf(const elf&) = delete;
        elf& operator=(const elf&) = delete;

        std::filesystem::path path() const { return path_; }
        const Elf64_Ehdr& get_header() const { return header_; }

        // Retrieve section names from the section name string table
        std::string_view get_section_name(std::size_t index) const;

        // Retrieve section information by section name
        std::optional<const Elf64_Shdr*> get_section(std::string_view name) const;
        span<const std::byte> get_section_contents(std::string_view name) const;

        // Retrieve the start file address of a section by name
        std::optional<file_addr> get_section_start_address(std::string_view name) const;

        // Retrieve a string from the general string table given an index
        // in the `.strtab` or `.dynstr` section
        std::string_view get_string(std::size_t index) const;

        virt_addr load_bias() const { return load_bias_; }
        void notify_loaded(virt_addr address) {
            load_bias_ = address;
        }

        // Retrieve the section to which a given file or virtual address belongs
        const Elf64_Shdr* get_section_containing_address(file_addr addr) const;
        const Elf64_Shdr* get_section_containing_address(virt_addr addr) const;

    private:
        int fd_;                      // open file descriptor
        std::filesystem::path path_;  // path to file
        std::size_t file_size_;       // file size
        std::byte* data_;             // pointer to file bytes
        virt_addr load_bias_;         // ELF load address for virtual address translation
        Elf64_Ehdr header_;           // header information
        std::vector<Elf64_Shdr> section_headers_; // section headers
        std::unordered_map<std::string_view, Elf64_Shdr*> section_map_; // section names -> section headers

        void parse_section_headers();
        void build_section_map();
    };
}

#endif //SDB_ELF_H
