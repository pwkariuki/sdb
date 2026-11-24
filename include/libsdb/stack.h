//
// Created by root on 11/20/26.
//

#ifndef SDB_STACK_H
#define SDB_STACK_H

#include <libsdb/dwarf.h>

namespace sdb {
    class target;

    // Inlined function stack
    class stack {
    public:
        stack(target* tgt) : target_(tgt) {}
        void reset_inline_height();
        std::vector<die> inline_stack_at_pc() const;
        std::uint32_t inline_height() const { return inline_height_; }
        const target& get_target() const { return *target_; }

        // Pretend to step in to an inlined function
        void simulate_inlined_step_in() {
            --inline_height_;
        }

    private:
        target* target_ = nullptr;
        std::uint32_t inline_height_ = 0;
    };
}

#endif //SDB_STACK_H
