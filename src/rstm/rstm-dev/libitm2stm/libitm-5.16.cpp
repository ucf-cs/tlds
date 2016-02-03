/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

// 5.16  Logging functions

#include "libitm.h"
#include "Transaction.h"
#include "Scope.h"
using namespace itm2stm;

namespace {
template <typename T, size_t W = sizeof(T) / sizeof(void*)>
struct INST {
    static void log(Scope* scope, const T* addr) {
        void** address = reinterpret_cast<void**>(const_cast<T*>(addr));
        for (size_t i = 0; i < W; ++i)
            scope->log(address, *address, sizeof(void*));
    }
};

template <typename T>
struct INST<T, 0u> {
    static void log(Scope* scope, const T* addr) {
        void** address = reinterpret_cast<void**>(const_cast<T*>(addr));
        union {
            T val;
            void* word;
        } cast = { *addr };
        scope->log(address, cast.word, sizeof(T));
    }
};
}

void
_ITM_LB(_ITM_transaction* td, const void* addr, size_t bytes) {
    void**  address = reinterpret_cast<void**>(const_cast<void*>(addr));
    Scope* scope = td->inner();

    // read and log as many words as we can
    for (size_t i = 0, e = bytes / sizeof(void*); i < e; ++i)
        scope->log(address + i, address[i], sizeof(void*));

    // read all of the remaining bytes and log them
    if (size_t e = bytes % sizeof(void*)) {
        const uint8_t* address8 = reinterpret_cast<const uint8_t*>(addr);
        address8 += bytes - e; // move cursor to the last word

        union {
            uint8_t bytes[sizeof(void*)];
            void* word;
        } buffer = {{0}};

        for (size_t i = 0; i < e; ++i)
            buffer.bytes[i] = address8[i];

        scope->log(reinterpret_cast<void**>(const_cast<uint8_t*>(address8)),
                   buffer.word, e);
    }
}

#define GENERATE_LOG(TYPE, EXT)                                 \
    void                                                        \
    _ITM_L##EXT(_ITM_transaction* td, const TYPE* address) {    \
        INST<TYPE>::log(td->inner(), address);                  \
    }

GENERATE_LOG(uint8_t, U1)
GENERATE_LOG(uint16_t, U2)
GENERATE_LOG(uint32_t, U4)
GENERATE_LOG(uint64_t, U8)
GENERATE_LOG(float, F)
GENERATE_LOG(double, D)
GENERATE_LOG(long double, E)
GENERATE_LOG(__m64, M64)
GENERATE_LOG(__m128, M128)
GENERATE_LOG(__m256, M256)
GENERATE_LOG(_Complex float, CF)
GENERATE_LOG(_Complex double, CD)
GENERATE_LOG(_Complex long double, CE)
