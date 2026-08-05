//
// Created by root on 8/4/26.
//

#ifndef SDB_SYSCALLS_H
#define SDB_SYSCALLS_H

#include <string_view>

namespace sdb {
    std::string_view syscall_id_to_name(int id);
    int syscall_name_to_id(std::string_view name);
}

#endif //SDB_SYSCALLS_H
