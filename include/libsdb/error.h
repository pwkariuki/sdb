//
// Created by root on 3/7/26.
//

#ifndef SDB_ERROR_H
#define SDB_ERROR_H

#include <stdexcept>
#include <cstring>

namespace sdb {
    class error : public std::runtime_error {
    public:
        [[noreturn]]
        static void send(const std::string& what) { throw error(what); }
        [[noreturn]]
        static void send_errno(const std::string& prefix) {
            throw error(prefix + ": " + std::strerror(errno));
        }

    private:
        error(const std::string& what) : std::runtime_error(what) {}
    };
}

#endif //SDB_ERROR_H