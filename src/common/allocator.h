#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstdint>
#include <malloc.h>
#include <atomic>
#include <common/assert.h>

#include <iostream>

template<typename DataType>
class Allocator 
{
public:
    Allocator(uint64_t totalBytes, uint64_t threadCount, uint64_t typeSize)
    {
        // std::cout << totalBytes << "bytes requested" << std::endl;
        m_totalBytes = totalBytes;
        m_threadCount = threadCount;
        m_typeSize = typeSize;
        m_ticket = 0;
        m_pool = (char*)memalign(m_typeSize, totalBytes);

        ASSERT(m_pool, "Memory pool initialization failed.");
    }

    ~Allocator()
    {
        free(m_pool);
    }

    //Every thread need to call init once before any allocation
    void Init()
    {
        uint64_t threadId = __sync_fetch_and_add(&m_ticket, 1);
        ASSERT(threadId < m_threadCount, "ThreadId specified should be smaller than thread count.");

        m_base = m_pool + threadId * m_totalBytes / m_threadCount;
        m_freeIndex = 0;
    }

    void Uninit()
    { }

    DataType* Alloc()
    {
        ASSERT(m_freeIndex < m_totalBytes / m_threadCount, "out of capacity.");
        char* ret = m_base + m_freeIndex;
        m_freeIndex += m_typeSize;

        return (DataType*)ret;
    }

private:
    char* m_pool;
    uint64_t m_totalBytes;      //number of elements T in the pool
    uint64_t m_threadCount;
    uint64_t m_ticket;
    uint64_t m_typeSize;

    static __thread char* m_base;
    static __thread uint64_t m_freeIndex;
};

template<typename T>
__thread char* Allocator<T>::m_base;

template<typename T>
__thread uint64_t Allocator<T>::m_freeIndex;

#endif /* end of include guard: ALLOCATOR_H */
