//
// Created by root on 3/7/26.
//

#ifndef SDB_PIPE_H
#define SDB_PIPE_H

#include <vector>
#include <cstddef>

namespace sdb {
    // Wrapper around pipes for interprocess communication
    class pipe {
    public:
        explicit pipe(bool close_on_exec);
        ~pipe();

        int get_read() const { return fds_[read_fd]; }
        int get_write() const { return fds_[write_fd]; }
        int release_read();
        int release_write();
        void close_read();
        void close_write();

        std::vector<std::byte> read();
        void write(std::byte* from, std::size_t bytes);

    private:
        static constexpr unsigned read_fd = 0;
        static constexpr unsigned write_fd = 1;
        // fds_[0] is the read end of pipe, fds_[1] is the write end of pipe
        int fds_[2]{};
    };
}

#endif //SDB_PIPE_H
