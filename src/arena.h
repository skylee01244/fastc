#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>

class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t bytes)
            : m_size(bytes),
            m_buffer(static_cast<std::byte*>(std::malloc(bytes))),
            m_offset(m_buffer)
    {}


    template<typename T>
    T* alloc() {
        size_t align = alignof(T);
        auto current_addr = reinterpret_cast<uintptr_t>(m_offset);
        size_t padding = (align - (current_addr % align)) % align;

        m_offset += padding;
        void* offset = m_offset;
        m_offset += sizeof(T);
        return new (offset) T;
    }

    ArenaAllocator(const ArenaAllocator& other) = delete;

    ArenaAllocator& operator=(const ArenaAllocator& other) = delete;

    ~ArenaAllocator() {
        std::free(m_buffer);
    }

private:
    size_t m_size;
    std::byte* m_buffer;
    std::byte* m_offset;
};