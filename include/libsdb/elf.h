//
// Created by root on 8/8/26.
//

#ifndef SDB_ELF_H
#define SDB_ELF_H

#include <elf.h>
#include <filesystem>

namespace sdb {
    // ELF files parser
    class elf {
    public:
        elf(const std::filesystem::path& path);
        ~elf();

        elf(const elf&) = delete;
        elf& operator=(const elf&) = delete;

        std::filesystem::path path() const { return path_; }
        const Elf64_Ehdr& get_header() const { return header_; }

    private:
        int fd_;                      // open file descriptor
        std::filesystem::path path_;  // path to file
        std::size_t file_size_;       // file size
        std::byte* data_;             // pointer to file bytes
        Elf64_Ehdr header_;           // header information
    };
}

#endif //SDB_ELF_H
