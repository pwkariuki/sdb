//
// Created by root on 7/24/26.
//

#ifndef SDB_BREAKPOINT_SITE_H
#define SDB_BREAKPOINT_SITE_H

#include <cstdint>
#include <cstddef>
#include <libsdb/types.h>

namespace sdb {
    class process;

    // Physical, hardware-level breakpoint
    class breakpoint_site {
    public:
        breakpoint_site() = delete;
        breakpoint_site(const breakpoint_site&) = delete;
        breakpoint_site& operator=(const breakpoint_site&) = delete;

        using id_type = std::int32_t; // unique breakpoint identifier type
        id_type id() const { return id_; }

        void enable();
        void disable();

        bool is_enabled() const { return is_enabled_; }
        virt_addr address() const { return address_; }

        bool at_address(virt_addr addr) const {
            return address_ == addr;
        }
        bool in_range(virt_addr low, virt_addr high) const {
            return low <= address_ and high > address_;
        }

    private:
        breakpoint_site(process& proc, virt_addr address);
        friend process;

        id_type id_;           // unique breakpoint identifier
        process* process_;     // process that owns breakpoint
        virt_addr address_;    // breakpoint site virtual address
        bool is_enabled_;      // breakpoint site enabled/disabled flag
        std::byte saved_data_; // data replaced with int3 to set breakpoint
    };
}

#endif //SDB_BREAKPOINT_SITE_H
