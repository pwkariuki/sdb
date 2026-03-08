//
// Created by root on 3/8/26.
//

#ifndef SDB_REGISTER_INFO_H
#define SDB_REGISTER_INFO_H

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <sys/user.h>

namespace sdb {
    // Unique enumerator to identify registers
    enum class register_id {
        #define DEFINE_REGISTER(name, dwarf_id, size, offset, type, format) name
        #include <libsdb/detail/registers.inc>
        #undef DEFINE_REGISTER
    };

    // GPR, a subregister of GPR, an FPR, or debug register
    enum class register_type {

    };

    // Different ways of interpreting a register
    enum class register_format {

    };

    struct register_info {
        register_id id;
        std::string_view name;
        std::int32_t dwarf_id;
        std::size_t size;       // register size in bytes
        std::size_t offset;     // byte offset into the user struct
        register_type type;
        register_format format;
    };

    // Global array of the information for every register in the system
    inline constexpr const register_info g_register_info[] = {
        #define DEFINE_REGISTER(name, dwarf_id, size, offset, type, format) \
            {register_id::name, #name, dwarf_id, size, offset, type, format }
        #include <libsdb/detail/registers.inc>
        #undef DEFINE_REGISTER
    };
}

#endif //SDB_REGISTER_INFO_H