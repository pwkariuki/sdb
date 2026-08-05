//
// Created by root on 8/4/26.
//

#include <unordered_map>
#include <libsdb/error.h>
#include <libsdb/syscalls.h>

namespace {
    const std::unordered_map<std::string_view, int> g_syscall_name_map = {
        #define DEFINE_SYSCALL(name,id) { #name, id },
        #include "include/syscalls.inc"
        #undef DEFINE_SYSCALL
    };
}

std::string_view sdb::syscall_id_to_name(int id) {
    switch (id) {
        #define DEFINE_SYSCALL(name,id) case id: return #name;
        #include "include/syscalls.inc"
        #undef DEFINE_SYSCALL
    default: error::send("No such syscall");
    }
}

int sdb::syscall_name_to_id(std::string_view name) {
    if (g_syscall_name_map.count(name) != 1) {
        error::send("No such syscall");
    }
    return g_syscall_name_map.at(name);
}
