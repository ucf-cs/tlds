/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_BLOCK_OPERATIONS_H
#define STM_ITM2STM_BLOCK_OPERATIONS_H

#include <cstddef>
#include <stdint.h>

// -----------------------------------------------------------------------------
// These block operations are used in the implementations of all of the memcpy,
// memmove, and memset operations. They have a special interface, they return
// the number of bytes that they actually read or wrote.
//
// There are some guarantees.
//
// 1) The return value will always be smaller than a word.
// 2) If ((uint8_t*)from) + bytes fits in an aligned word, all of the bytes
//    will be written.
//
// This essentially means that, if you update your to and from cursors based on
// the returned size, you can succeed using the following loop.
//
// TxThread& tx;
// size_t block_size;
// void* to;
// const void* from;
// while (block_size) {
//   size_t bytes_handled = block_write/read(tx, to, from, block_size);
//   block_size -= bytes_handled;
//   (uint8_t*)to += bytes_handled;
//   (uint8_t*)from += bytes_handled;
// }
//
// -----------------------------------------------------------------------------

namespace stm {
struct TxThread;
}

namespace itm2stm {
size_t block_write(stm::TxThread&, void* target, const void* source, size_t bytes)
    __attribute__((nonnull));
size_t block_read(stm::TxThread&, void* target, const void* source, size_t bytes)
    __attribute__((nonnull));
void block_set(stm::TxThread&, void* target, uint8_t c, size_t bytes)
    __attribute__((nonnull));

// -----------------------------------------------------------------------------
// Used as a "binder" for the stm::TxThread& parameter in block_read.
// -----------------------------------------------------------------------------
struct BlockReader {
    stm::TxThread& tx_;

    BlockReader(stm::TxThread& tx) : tx_(tx) {
    }

    inline size_t
    operator()(void* to, const void* from, size_t length) {
        return block_read(tx_, to, from, length);
    }
};

// -----------------------------------------------------------------------------
// Used as a "binder" for the stm::TxThread& parameter in block_write.
// -----------------------------------------------------------------------------
struct BlockWriter {
    stm::TxThread& tx_;
    BlockWriter(stm::TxThread& tx) : tx_(tx) {
    }

    inline size_t
    operator()(void* to, const void* from, size_t length) {
        return block_write(tx_, to, from, length);
    }
};
}

#endif // STM_ITM2STM_BLOCK_OPERATIONS_H
