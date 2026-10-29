//
// Created by root on 8/8/26.
//

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libsdb/bit.h>
#include <libsdb/elf.h>
#include <libsdb/error.h>

sdb::elf::elf(const std::filesystem::path &path) {
    path_ = path;

    if ((fd_ = open(path.c_str(), O_RDONLY)) < 0) {
        error::send_errno("Could not open ELF file");
    }

    struct stat stats;
    if (fstat(fd_, &stats) < 0) {
        error::send_errno("Could not retrieve ELF file stats");
    }
    file_size_ = stats.st_size;

    void* ret;
    if ((ret = mmap(0, file_size_, PROT_READ, MAP_SHARED, fd_, 0)) == MAP_FAILED) {
        close(fd_);
        error::send_errno("Could not mmap ELF file");
    }
    data_ = reinterpret_cast<std::byte*>(ret);

    std::copy(data_, data_ + sizeof(header_), as_bytes(header_));
    parse_section_headers();
    build_section_map();
}

sdb::elf::~elf() {
    munmap(data_, file_size_);
    close(fd_);
}

std::string_view sdb::elf::get_section_name(std::size_t index) const {
    auto& section = section_headers_[header_.e_shstrndx];
    return { reinterpret_cast<char*>(data_) + section.sh_offset + index };
}

std::optional<const Elf64_Shdr *>
sdb::elf::get_section(std::string_view name) const {
    if (section_map_.count(name) == 0) {
        return std::nullopt;
    }
    return section_map_.at(name);
}

sdb::span<const std::byte>
sdb::elf::get_section_contents(std::string_view name) const {
    if (auto sect = get_section(name); sect) {
        return { data_ + sect.value()->sh_offset, sect.value()->sh_size };
    }
    return { nullptr, std::size_t(0) };
}

std::optional<sdb::file_addr>
sdb::elf::get_section_start_address(std::string_view name) const {
    if (auto sect = get_section(name); sect) {
        return file_addr{ *this, sect.value()->sh_addr };
    }
    return std::nullopt;
}

std::string_view sdb::elf::get_string(std::size_t index) const {
    auto opt_strtab = get_section(".strtab");
    if (!opt_strtab) {
        opt_strtab = get_section(".dynstr");
        if (!opt_strtab) return "";
    }
    return reinterpret_cast<char*>(data_) + opt_strtab.value()->sh_offset + index;
}

const Elf64_Shdr * sdb::elf::get_section_containing_address(file_addr addr) const {
    if (addr.elf_file() != this) return nullptr;
    for (auto& section : section_headers_) {
        if (section.sh_addr <= addr.addr() and
            section.sh_addr + section.sh_size > addr.addr()) {
            return &section;
        }
    }
    return nullptr;
}

const Elf64_Shdr * sdb::elf::get_section_containing_address(virt_addr addr) const {
    for (auto& section : section_headers_) {
        if (load_bias_ + section.sh_addr <= addr and
            load_bias_ + section.sh_addr + section.sh_size > addr) {
            return &section;
        }
    }
    return nullptr;
}

void sdb::elf::parse_section_headers() {
    auto n_headers = header_.e_shnum;
    // file specifies the number of headers to be 0 but also specifies section
    // header size element, there are more than 0xff00 sections. the real number
    // of headers is contained in the `sh_size` field of the first header.
    if (n_headers == 0 and header_.e_shentsize != 0) {
        n_headers = from_bytes<Elf64_Shdr>(data_ + header_.e_shoff).sh_size;
    }

    section_headers_.resize(n_headers);
    std::copy(data_ + header_.e_shoff,
        data_ + header_.e_shoff + sizeof(Elf64_Shdr) * n_headers,
        reinterpret_cast<std::byte*>(section_headers_.data()));
}

void sdb::elf::build_section_map() {
    for (auto& section : section_headers_) {
        section_map_[get_section_name(section.sh_name)] = &section;
    }
}
