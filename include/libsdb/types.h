//
// Created by pkariuk1 on 3/11/206.
//

#ifndef SDB_TYPES_H
#define SDB_TYPES_H

#include <array>
#include <cstddef>

namespace sdb {
	using byte64 = std::array<std::byte, 8>;
	using byte128 = std::array<std::byte, 16>;
}

#endif //SDB_TYPES_H
