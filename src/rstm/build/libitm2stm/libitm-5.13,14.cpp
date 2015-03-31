/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <cassert>
#include "libitm.h"
#include "Transaction.h"
#include "BlockOperations.h"
#include "Utilities.h"
using stm::TxThread;
using itm2stm::BlockReader;
using itm2stm::BlockWriter;
using itm2stm::add_bytes;
using itm2stm::offset_of;

namespace {
// -----------------------------------------------------------------------------
// The memcpy template is parameterized by the actual algorithms that we want to
// use on the read side and the write set. Essentially, this just manages a
// buffer, copying chunks from the read-side into it, and copying it out into
// the write-side.
//
// The buffer isn't a circular buffer due to the difficulty of dealing with
// wrapping transactional accesses, so after each iteration there may be a
// residual number of bytes left in the buffer (due to mis-alignment) that we
// need to copy to the front of the buffer. This amount is bounded by
// sizeof(void*) though, so its just a constant operation.
// -----------------------------------------------------------------------------
template <typename R, typename W>
inline void
memcpy(void* to, const void* from, size_t len, R reader, W writer) {
    // Allocate our buffer.
    const size_t capacity = 8 * sizeof(void*);
    uint8_t buffer[capacity];
    size_t size = 0;

    // main loop copies chunks through the buffer
    while (len) {
        // get the minimum that we can read
        const size_t to_read = (capacity - size < len) ? capacity - size : len;

        // might not read requested amount due to alignement
        const size_t read = reader(buffer + size, from, to_read);
        size += read; // update our buffer size
        len -= read;  // the remaining bytes to read
        add_bytes(from, read); // and the from cursor

        // might not write requested amount due to alignment
        const size_t wrote = writer(to, buffer, size);
        size -= wrote; // update our buffer size
        add_bytes(to, wrote); // and the to cursor

        // copy the unwritten bytes into the beginning of the buffer
        for (size_t i = 0; i < size; ++i)
            buffer[i] = buffer[wrote + i];
    }

    // force the writer to write out the rest of the buffer (it could have
    // refused if there was an alignment issue in the loop)
    if (len) {
        assert(offset_of(to) == 0 && "unexpected unaligned \"to\" cursor");
        assert(len < sizeof(void*) && "unexpected length remaining");
        len -= writer(to, buffer, size);
        assert(len == 0 && "draining write not performed");
    }
}

// ----------------------------------------------------------------------------
// Our memcpy loop needs a slightly different interface than the libc memcpy
// provides. It needs the function to return the number of bytes actually read
// (because our transactional versions might refuse to write some bytes due to
// alignment issues).
// ----------------------------------------------------------------------------
inline size_t
builtin_memcpy_wrapper(void* to, const void* from, size_t n) {
    __builtin_memcpy(to, from, n);
    return n;
}

// ----------------------------------------------------------------------------
// Our memmove loop needs a slightly different interface than the libc memmove
// provides. It needs the function to return the number of bytes actually read
// (because our transactional versions might refuse to write some bytes due to
// alignment issues).
// ----------------------------------------------------------------------------
inline size_t
builtin_memmove_wrapper(void* to, const void* from, size_t n) {
    __builtin_memmove(to, from, n);
    return n;
}
}

// ----------------------------------------------------------------------------
// 5.13  Transactional memory copies ------------------------------------------
// ----------------------------------------------------------------------------
void
_ITM_memcpyRnWt(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockWriter writer(td->handle());
    memcpy(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memcpyRnWtaR(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockWriter writer(td->handle());
    memcpy(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memcpyRnWtaW(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockWriter writer(td->handle());
    memcpy(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memcpyRtWn(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockWriter writer(td->handle());
    memcpy(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memcpyRtWt(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtWtaR(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtWtaW(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaRWn(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    memcpy(to, from, n, reader, builtin_memcpy_wrapper);
}

void
_ITM_memcpyRtaRWt(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaRWtaR(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaRWtaW(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaWWn(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    memcpy(to, from, n, reader, builtin_memcpy_wrapper);
}

void
_ITM_memcpyRtaWWt(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaWWtaR(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaWWtaW(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());
    memcpy(to, from, n, reader, writer);
}

// ----------------------------------------------------------------------------
// 5.14  Transactional versions of memmove ------------------------------------
// ----------------------------------------------------------------------------
void
_ITM_memmoveRnWt(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, builtin_memcpy_wrapper, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRnWtaR(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, builtin_memcpy_wrapper, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRnWtaW(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, builtin_memcpy_wrapper, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtWn(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, builtin_memcpy_wrapper);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtWt(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtWtaR(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtWtaW(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtaRWn(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, builtin_memcpy_wrapper);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtaRWt(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtaRWtaR(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtaRWtaW(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtaWWn(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, builtin_memcpy_wrapper);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtaWWt(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtaWWtaR(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}

void
_ITM_memmoveRtaWWtaW(_ITM_transaction* td, void* to, const void* from, size_t n)
{
    BlockReader reader(td->handle());
    BlockWriter writer(td->handle());

    uint8_t* target = static_cast<uint8_t*>(to);
    uint8_t* target_end = target + n;

    const uint8_t* source = static_cast<const uint8_t*>(from);
    const uint8_t* source_end = source + n;

    if (source_end < target && target_end < source)
        memcpy(to, from, n, reader, writer);

    assert(false && "memmove not yet implemented for overlapping regions.");
}
