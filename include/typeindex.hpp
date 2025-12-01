#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace nbody {
namespace internal {
    namespace typeindex_impl {
        template <typename N>
        inline N next_type_index() noexcept {
            static std::atomic<N> counter{0};
            return counter.fetch_add(1, std::memory_order::relaxed);
        }
    }

    template<typename T>
    std::size_t type_id_uint8() noexcept {
        static const uint8_t index = typeindex_impl::next_type_index<uint8_t>();
        return index;
    }
}
}  // namespace nbody
