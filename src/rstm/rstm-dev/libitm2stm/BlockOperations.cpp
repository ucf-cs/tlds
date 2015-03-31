/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include "BlockOperations.h"
#include "Utilities.h"
#include "stm/txthread.hpp"
using stm::TxThread;
using namespace itm2stm;

namespace {
inline size_t
read_subword(TxThread& tx, void** base, uint8_t* to, size_t i, size_t j) {
    assert(i < j && j <= sizeof(void*) && "range incorrect");
    assert(offset_of(base) == 0 && "base should be aligned");

    // load the subword transactionally using a union so that we can copy only
    // the bytes that we want into the "to" pointer
    const union {
        void* word;
        uint8_t bytes[sizeof(void*)];
    } align = { tx.tmread(&tx, base, make_mask(i, j)) };

    // copy the bytes on-by-one, note the offset into the align union
    const size_t length = (j - i);
    for (size_t k = 0; k < length; ++k)
        to[k] = align.bytes[i + k];

    // return the number of bytes we read
    return length;
}

inline size_t
write_subword(TxThread& tx, void** base, const uint8_t* from, size_t i,
              size_t j)
{
    assert(i < j && j <= sizeof(void*) && "range incorrect");
    assert(offset_of(base) == 0 && "base should be aligned");

    // read the bytes into a union with the correct offset
    union {
        void* word;
        uint8_t bytes[sizeof(void*)];
    } buffer = {0};

    const size_t length = j - i;
    for (size_t k = 0; k < length; ++k)
        buffer.bytes[i + k] = from[k];

    // perform the write
    tx.tmwrite(&tx, base, buffer.word, make_mask(i, j));

    // return the number of bytes we wrote
    return length;
}

}

// -----------------------------------------------------------------------------
// In this code the source pointer is being read from transactionally, and the
// target pointer is being written to non-transactionally. We don't care about
// the target pointer alignment on platforms that don't require aligned
// accesses, but we do care about the alignment of the source pointer because
// our transactional accesses must be done on word-aligned boundaries.
//
// If there is an unaligned prefix, we do a read_word of the prefix, and then
// read as many aligned words as we can. We *do not* read unaligned postfix
// words in this case. We only read unaligned postfix words when the source +
// length all fits into one word. This means that the caller must be prepared to
// deal with unsatisfied bytes. See BlockOperations.h for an example.
// -----------------------------------------------------------------------------
size_t
itm2stm::block_read(TxThread& tx, void* target, const void* source,
                        size_t length) {
    assert(target && "block_read target is null");
    assert(source && "block_read source is null");

    if (!length) return 0;

    // We may not read the entire length requested, we remember how much we
    // actually read.
    size_t read = 0;

    // We need special handling for unaligned transactional loads.
    const size_t offset = offset_of(source);

    // get the base for the source address, this is a waste if source is
    // aligned, but it does give us the right type too, so we don't really care
    void** base = base_of(source);

    // if there is an offset, or the whole amount fits into 1 read, read it as a
    // subword
    if (offset || offset + length <= sizeof(void*)) {
        // compute the upper bound index for the subword read
        size_t end = offset + length;
        if (end > sizeof(void*))
            end = sizeof(void*);

        uint8_t* const to = reinterpret_cast<uint8_t*>(target);
        read = read_subword(tx, base, to, offset, end);
        if (read == length)
            return read;

        // update our cursors
        add_bytes(target, read);
        ++base;
    }

    // read as many word-sized chunks as we can from the remaining bytes
    const size_t words = (length - read) / sizeof(void*);
    const uintptr_t mask = make_mask(0, sizeof(void*));

    // use the target pointers as a pointer to a word---this might not be
    // aligned, but that's ok because the write to "target" is nontransactional
    void** to = reinterpret_cast<void**>(target);

    for (size_t i = 0; i < words; ++i, read += sizeof(void*))
        to[i] = tx.tmread(&tx, base + i, mask);

    // return the number of bytes we've read
    return read;
}

// -----------------------------------------------------------------------------
// In this code the target address is written transactionally, while the source
// address is read non-transactionally. We don't care about the alignment of the
// source pointer (on platforms where it may be unaligned), but we *do* care
// about the alignment of the target pointer because we can only do word-aligned
// writes.
//
// If there is an unaligned prefix that we are supposed to write to, we do that
// write using write_word, and then proceed to write as many aligned words as we
// can. We *do not* write an unaligned postfix here, so the caller must be
// prepared to deal with unsatisfied bytes (see BlockOperations.h for an
// example). We will write an unaligned postfix if to + length fits in one
// word.
// -----------------------------------------------------------------------------
size_t
itm2stm::block_write(TxThread& tx, void* target, const void* source,
                         size_t length) {
    assert(target && "block_read target is null");
    assert(source && "block_read source is null");

    if (!length) return 0;

    // We might not write everything, so we keep track of how much we should
    // write.
    size_t written = 0;

    // We need special handling for unaligned transactional stores
    const size_t offset = offset_of(target);

    // Get the base address for the target address, this is a waste if the
    // target address is already aligned, but it gives us the right type too.
    void** base = base_of(target);

    // if there is an offset, or we're going to be able to do the whole
    // operation with one writer, use the write_subword implementation
    if (offset || offset + length <= sizeof(void*)) {
        size_t end = offset + length;
        if (end > sizeof(void*))
            end = sizeof(void*);

        const uint8_t* const from = reinterpret_cast<const uint8_t*>(source);
        written = write_subword(tx, base, from, offset, end);
        if (written == length)
            return written;

        // update our cursors
        add_bytes(source, written);
        ++base;
    }

    // write as many word-sized chunks as we can from the remaining bytes
    const size_t words = (length - written) / sizeof(void*);
    const uintptr_t mask = make_mask(0, sizeof(void*));

    // use the source address as a pointer to a word---this might not be
    // aligned, but that is ok because the read from the "source" is
    // nontransactional
    void* const * const from = reinterpret_cast<void* const *>(source);

    for (size_t i = 0; i < words; ++i, written += sizeof(void*))
        tx.tmwrite(&tx, base + i, from[i], mask);

    // return the number of bytes we've written
    return written;
}

void
itm2stm::block_set(TxThread& tx, void* target, uint8_t c, size_t length) {
    assert(target && "block_set target should not be null");

    if (length == 0)
        return;

    // all of the words come from this array
    const union {
        uint8_t bytes[sizeof(void*)];
        void* word;
    } from = {{c}};

    void** base = base_of(target);
    size_t offset = offset_of(target);

    // deal with any prefix bytes (also handles subword block_set)
    if (offset || offset + length <= sizeof(void*)) {
        size_t end = offset + length;
        if (end > sizeof(void*))
            end = sizeof(void*); // saturated add

        length -= write_subword(tx, base, from.bytes, offset, end);
        if (length == 0)
            return;

        ++base; // update target address
    }

    // write as many word-sized chunks as we can from the remaining bytes
    const size_t words = length / sizeof(void*);
    const uintptr_t mask = make_mask(0, sizeof(void*));

    for (size_t i = 0; i < words; ++i, length -= sizeof(void*))
        tx.tmwrite(&tx, base + i, from.word, mask);

    // deal with any postfix bytes
    if (length)
        length -= write_subword(tx, base + words, from.bytes, 0, length);

    assert(length == 0 && "should not be any remaining bytes.");
}
