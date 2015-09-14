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
#include "Scope.h"
#include "CheckOffsets.h"

using namespace itm2stm;
using std::pair;

inline void**
Scope::ThrownObject::begin() const {
    return first;
}

inline void**
Scope::ThrownObject::end() const {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(first);
    bytes += second;
    return reinterpret_cast<void**>(bytes);
}

inline void**
Scope::LoggedWord::begin() const {
    return reinterpret_cast<void**>(address_);
}

inline void**
Scope::LoggedWord::end() const {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(address_);
    bytes += bytes_;
    return reinterpret_cast<void**>(bytes);
}

inline void
Scope::LoggedWord::clip(void** lower, void** upper) {
    // no intersection
    if (end() <= lower || begin() >= upper)
        return;

    // complete intersection
    if (begin() >= lower && end() <= upper) {
        bytes_ = 0;
        return;
    }

    // clip end()
    if (begin() < lower && end() < upper) {
        bytes_ = (uint8_t*)lower - (uint8_t*)begin();
    }

    // clip begin()
    if (begin() > lower && end () > upper) {
        // shift value, since we always start writing at it.
        value_ = (void**)(((uintptr_t)value_) >> (((uint8_t*)upper -
                                                   (uint8_t*)begin()) * 8));
        bytes_ = (uint8_t*)end() - (uint8_t*)upper;
        address_ = upper;
    }

    // uh-oh, unhandle-able case
    if ((begin() > lower && end() < (void**)((uint8_t*)upper - 1)))
        assert(false && "Logged value has incompatible range intersection");

    // uh-oh, unreachable
    assert(false && "unreachable");
}

inline void
Scope::LoggedWord::undo(ThrownObject& thrown, void** protected_stack_end)
{
    // first-off we need to protect the stack from dangerous writes.
    void* top_of_stack;
    clip(&top_of_stack, protected_stack_end);

    // and protect the exception object
    clip(thrown.begin(), thrown.end());

    // memcpy is tolerant of different "bytes_" values, we could also perform
    // static dispatch to a correctly-cast write, but we're working on the
    // assumption that undo performance doesn't matter, and this is easier.
    __builtin_memcpy(address_, &value_, bytes_);
}

Scope::Scope(_ITM_transaction& owner)
    : Checkpoint(),
      aborted_(true),
      flags_(0),
      id_(_ITM_NoTransactionId),
      thrown_(),
      do_on_rollback_(),
      undo_on_rollback_(),
      do_on_commit_(),
      owner_(owner) {
#if defined(SCOPE_ABORTED_)
    ASSERT_OFFSET(offsetof(Scope, aborted_), SCOPE_ABORTED_);
#endif
}

std::pair<void**, size_t>&
Scope::rollback(void** protected_stack_bound) {
    // 1) Undo all of the logged words.
    for (UndoList::iterator i = undo_on_rollback_.begin(),
                            e = undo_on_rollback_.end(); i != e; ++i)
        i->undo(thrown_, protected_stack_bound);

    // 2) Perform the user's registered onAbort callbacks, in FIFO order
    for (RollbackList::iterator i = do_on_rollback_.begin(),
                                e = do_on_rollback_.end(); i != e; ++i)
        i->eval();

    // 3) Clear the commit callbacks.
    do_on_commit_.clear();

    // 4) Mark that we're an aborted scope... we'll need to be Scope::enter-ed
    //    to be used again.
    aborted_ = true;

    // 5) Tell the caller about the protected address range.
    return thrown_;
}

void
Scope::setThrownObject(void** addr, size_t length) {
    assert(thrown_.first == NULL && "Only one thrown object expected "
                                    "per-scope");
    thrown_.first = addr;
    thrown_.second = length;
}

void
Scope::clearThrownObject() {
    thrown_.reset();
}
