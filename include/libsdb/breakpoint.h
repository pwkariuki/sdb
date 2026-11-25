//
// Created by root on 11/24/26.
//

#ifndef SDB_BREAKPOINT_H
#define SDB_BREAKPOINT_H

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <libsdb/breakpoint_site.h>
#include <libsdb/stoppoint_collection.h>
#include <libsdb/types.h>

namespace sdb {
    class target;

    // Source-level breakpoint
    class breakpoint {
    public:
        virtual ~breakpoint() = default;

        breakpoint() = delete;
        breakpoint(const breakpoint&) = delete;
        breakpoint& operator=(const breakpoint&) = delete;

        using id_type = std::uint32_t;
        id_type id() const { return id_; }

        void enable();
        void disable();

        bool is_enabled() const { return is_enabled_; }
        bool is_hardware() const { return is_hardware_; }
        bool is_internal() const { return is_internal_; }

        virtual void resolve() = 0;

        stoppoint_collection<breakpoint_site, false>& breakpoint_sites() {
            return breakpoint_sites_;
        }
        const stoppoint_collection<breakpoint_site, false>&
            breakpoint_sites() const {
            return breakpoint_sites_;
        }

        bool at_address(virt_addr addr) const {
            return breakpoint_sites_.contains_address(addr);
        }
        bool in_range(virt_addr low, virt_addr high) const {
            return !breakpoint_sites_.get_in_region(low, high).empty();
        }

    protected:
        friend target;
        breakpoint(
            target& tgt, bool is_hardware = false, bool is_internal = false);

        id_type id_;
        target* target_;
        bool is_enabled_ = false;
        bool is_hardware_ = false;
        bool is_internal_ = false;
        stoppoint_collection<breakpoint_site, false> breakpoint_sites_;
        breakpoint_site::id_type next_site_id_ = 1;
    };

    // Breakpoint set on a function name
    class function_breakpoint : public breakpoint {
    public:
        void resolve() override;
        std::string_view function_name() const { return function_name_; }

    private:
        friend target;
        function_breakpoint(
            target& tgt, std::string function_name,
            bool is_hardware = false, bool is_internal = false)
                : breakpoint(tgt, is_hardware, is_internal),
                function_name_(std::move(function_name)) {
            resolve();
        }

        std::string function_name_;
    };

    // Breakpoint set on a line of a source file
    class line_breakpoint : public breakpoint {
    public:
        void resolve() override;
        const std::filesystem::path file() const { return  file_; }
        std::size_t line() const { return line_; }

    private:
        friend target;
        line_breakpoint(target& tgt,
            std::filesystem::path file,
            std::size_t line,
            bool is_hardware = false, bool is_internal = false)
                : breakpoint(tgt, is_hardware, is_internal),
                file_(std::move(file)), line_(line) {
            resolve();
        }

        std::filesystem::path file_;
        std::size_t line_;
    };

    // Breakpoint set on a virtual memory address
    class address_breakpoint : public breakpoint {
    public:
        void resolve() override;
        virt_addr address() const { return address_; }

    private:
        friend target;
        address_breakpoint(target& tgt, virt_addr address,
            bool is_hardware = false, bool is_internal = false)
                : breakpoint(tgt, is_hardware, is_internal),
                address_(address) {
            resolve();
        }

        virt_addr address_;
    };
}

#endif //SDB_BREAKPOINT_H
