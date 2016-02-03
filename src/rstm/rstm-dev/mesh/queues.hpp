/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  queues.hpp
 *
 *  Sequential and concurrent queues.
 *  Element type should always be (the same size as) a pointer.
 */

#ifndef QUEUES_HPP__
#define QUEUES_HPP__

#include <cstdlib>
#include <cassert>
#include <queue>
using std::queue;

#include "common.hpp"
#include "counted_ptr.hpp"

template<typename T>
class simple_queue {
  public:
    virtual void enqueue(const T item, const int tid) = 0;
    virtual T dequeue(const int tid) = 0;
    virtual ~simple_queue() {}
};

template<typename T>
class sequential_queue : public simple_queue<T> {
  private:
    queue<T> queue_impl;
  public:
    virtual void enqueue(const T item, const int) {
        assert(item != 0);
        queue_impl.push(item);
    }
    virtual T dequeue(int) {
        if (queue_impl.empty()) return T(0);
        T rtn = queue_impl.front();
        queue_impl.pop();
        return rtn;
    }
    virtual ~sequential_queue() {}
};

//  Class exists only as a (non-templated) base for concurrent_queue<T>
//
template<typename T>
class MS_queue {
    counted_ptr head;
    counted_ptr tail;
  protected:
    void enqueue(T item, const int tid);
    T dequeue(const int tid);
    MS_queue(const int tid);
    virtual ~MS_queue() { assert(false); }
    // Destruction of concurrent queue not currently supported.
};

template<typename T>
class concurrent_queue : public simple_queue<T>, private MS_queue<T>
{
    concurrent_queue(const concurrent_queue&);
    // no implementation; forbids passing by value
    concurrent_queue& operator=(const concurrent_queue&);
    // no implementation; forbids copying

  public:
    virtual void enqueue(T item, const int tid) {
        MS_queue<T>::enqueue(item, tid);
    }
    virtual T dequeue(const int tid) {
        return MS_queue<T>::dequeue(tid);
    }

    // make sure that the queue is cache-line aligned
    static void* operator new(const size_t size) {
        return memalign(CACHELINE_BYTES, size);
    }
    // the C++ runtime's delete operator should do this the same way,
    // but just in case...
    static void operator delete(void *ptr) {free(ptr);}

    concurrent_queue(const int tid) : MS_queue<T>(tid) {
        assert(sizeof(T) == sizeof(void*));
    }
    virtual ~concurrent_queue() { }
};

#endif // QUEUES_HPP__
