/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  queues.cc
 *
 *  Sequential and concurrent queues.
 *  Element type should always be a pointer.
 */

#include <stdlib.h>
#include <sys/mman.h>
#include "common.hpp"
#include "edge.hpp"
#include "queues.hpp"

//////////////////////////////
//
//  Get memory from the OS directly

inline void* alloc_mmap(unsigned size)
{
    void* mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
                     -1, 0);
    assert(mem != MAP_FAILED);
    return mem;
}

inline void free_mmap(void*)
{
    // NB: Should do something here, but we don't have a size to munmap with.
    std::cerr << "Ignoring a request to munmap.\n";
}

//////////////////////////////
//
//  Block Pool

template<typename T>
struct qnode_t {
    T t;
    counted_ptr next;
};

// atomic 64-bit load
//
static inline unsigned long long a_ld(volatile unsigned long long* src) {
    union {
        volatile unsigned long long* from;
        volatile double* to;
    } p;
    p.from = src;
    union {
        volatile double from;
        volatile unsigned long long to;
    } v;
    v.from = *p.to;
    return v.to;
}

template<typename T>
class block_pool
{
    struct shared_block_t
    {
        struct shared_block_t* volatile next;
        struct shared_block_t* volatile next_group;
        volatile qnode_t<T> payload;
    };

    struct block_head_node_t
    {
        shared_block_t* volatile top;    // top of stack
        shared_block_t* volatile nth;    // ptr to GROUP_SIZE+1-th node, if
                                         // any, counting up from the bottom of
                                         // the stack
        volatile unsigned long count;    // number of nodes in list
    } __attribute__((aligned(CACHELINE_BYTES)));

    // add and remove blocks from/to global pool in clumps of this size
    static const unsigned long GROUP_SIZE = 8;

    static const int blocksize = sizeof(shared_block_t);

    // [?] why don't we make this a static member, align it, make it volatile,
    // and then pass its address to CAS?
    // MLS: not safe if there's more than one block pool
    counted_ptr* global_pool;

    block_head_node_t* head_nodes;  // one per thread

    // The following is your typical memory allocator hack.  Callers care about
    // payloads, which may be of different sizes in different pools.  Pointers
    // returned from alloc_block and passed to free_block point to the payload.
    // The bookkeeping data (next and next_group) are *in front of* the
    // payload.  We find them via pointer arithmetic.
    static inline shared_block_t* make_shared_block_t(void* block)
    {
        shared_block_t dum;
        return (shared_block_t*) (((unsigned char*)block) -
                                  ((unsigned char*)&(dum.payload) - (unsigned char*)&dum));
    }

public:

    // make sure nothing else is in the same line as this object
    static void* operator new(size_t size) { return alloc_mmap(size); }

    static void operator delete(void* ptr) { return free_mmap(ptr); }

    //  Create and return a pool to hold blocks of a specified size, to be shared
    //  by a specified number of threads.  Return value is an opaque pointer.  This
    //  routine must be called by one thread only.
    //
    block_pool<T>(int _numthreads)
    {
        // get memory for the head nodes
        head_nodes = (block_head_node_t*)
            memalign(CACHELINE_BYTES, _numthreads * sizeof(block_head_node_t));
        // get memory for the global pool
        global_pool = (counted_ptr*)memalign(CACHELINE_BYTES, CACHELINE_BYTES);

        // make sure the allocations worked
        assert(head_nodes != 0 && global_pool != 0);

        // zero out the global pool
        global_pool->p.ptr = 0;
        global_pool->p.sn = 0;       // not really necessary

        // configure the head nodes
        for (int i = 0; i < _numthreads; i++) {
            block_head_node_t* hn = &head_nodes[i];
            hn->top = hn->nth = 0;
            hn->count = 0;
        }
    }

    void* alloc_block(int tid)
    {
        block_head_node_t* hn = &head_nodes[tid];
        shared_block_t* b = hn->top;
        if (b) {
            hn->top = b->next;
            hn->count--;
            if (b == hn->nth)
                hn->nth = 0;
        }
        else {
            // local pool is empty
            while (true) {
                counted_ptr oldp;
                oldp.all = a_ld(&global_pool->all);
                if ((b = (shared_block_t*)oldp.p.ptr)) {
                    if (cp_cas(&global_pool->all, oldp.p.ptr, oldp.p.sn, b->next_group)) {
                        // successfully grabbed group from global pool
                        hn->top = b->next;
                        hn->count = GROUP_SIZE-1;
                        break;
                    }
                    // else somebody else got into timing window; try again
                }
                else {
                    // global pool is empty
                    b = (shared_block_t*)memalign(CACHELINE_BYTES, blocksize);
                    assert(b != 0);
                    break;
                }
            }
            // In real-time code I might want to limit the number of iterations of
            // the above loop, and go ahead and malloc a new node when there is
            // very heavy contention for the global pool.  In practice I don't
            // expect a starvation problem.  Note in particular that the code as
            // written is preemption safe.
        }
        return (void*)&b->payload;
    }

    void free_block(void* block, int tid)
    {
        block_head_node_t* hn = &head_nodes[tid];
        shared_block_t* b = make_shared_block_t(block);

        b->next = hn->top;
        hn->top = b;
        hn->count++;
        if (hn->count == GROUP_SIZE+1) {
            hn->nth = hn->top;
        }
        else if (hn->count == GROUP_SIZE * 2) {
            // got a lot of nodes; move some to global pool
            shared_block_t* ng = hn->nth->next;
            while (true) {
                counted_ptr oldp;
                oldp.all = a_ld(&global_pool->all);
                ng->next_group = (shared_block_t*)oldp.p.ptr;
                if (cp_cas(&global_pool->all, oldp.p.ptr, oldp.p.sn, ng)) break;
                // else somebody else got into timing window; try again
            }
            // In real-time code I might want to limit the number of
            // iterations of the above loop, and let my local pool grow
            // bigger when there is very heavy contention for the global
            // pool.  In practice I don't expect a problem.  Note in
            // particular that the code as written is preemption safe.
            hn->nth->next = 0;
            hn->nth = 0;
            hn->count -= GROUP_SIZE;
        }
    }
};

//////////////////////////////
//
//  Templated Non-Blocking Queue

static block_pool<edge*>* bp = 0;
    // Shared by all queue instances.  Note that all queue nodes are
    // the same size, because all payloads are pointers.

//  Following constructor should be called by only one thread, and there
//  should be a synchronization barrier (or something else that forces
//  a memory barrier) before the queue is used by other threads.
//  Paremeter is id of calling thread, whose subpool should be used to
//  allocate the initial dummy queue node.
//
template<typename T>
MS_queue<T>::MS_queue(const int tid)
{
    if (bp == 0) {
        // need to create block pool
        static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&lock);
        if (!bp)
            bp = new block_pool<T>(num_workers);
        pthread_mutex_unlock(&lock);
    }
    qnode_t<T>* qn = (qnode_t<T>*)bp->alloc_block(tid);
    qn->next.p.ptr = 0;
    // leave qn->next.p.sn where it is!
    head.p.ptr = tail.p.ptr = qn;
    // leave head.p.sn and tail.p.sn where they are!
}
// explicit instantiation:
template MS_queue<edge*>::MS_queue(const int tid);

//  M&S lock-free queue
//
template<typename T>
void MS_queue<T>::enqueue(T t, const int tid)
{
    qnode_t<T>* qn = (qnode_t<T>*)bp->alloc_block(tid);
    counted_ptr my_tail;
    counted_ptr my_tail2;

    qn->t = t;
    qn->next.p.ptr = 0;
    // leave sn where it is!
    while (true) {
        counted_ptr my_next;
        my_tail.all = a_ld(&tail.all);
        my_next.all = a_ld(&((qnode_t<T>*)my_tail.p.ptr)->next.all);
        my_tail2.all = a_ld(&tail.all);
        if (my_tail.all == my_tail2.all) {
            // my_tail and my_next are mutually consistent
            if (my_next.p.ptr == 0) {
                // last node; try to link new node after this
                if (cp_cas(&((qnode_t<T>*)my_tail.p.ptr)->next.all,
                           my_next.p.ptr, my_next.p.sn, qn)) {
                    break;              // enqueue worked
                }
            }
            else {
                // try to swing B->tail to next node
                (void) cp_cas(&tail.all, my_tail.p.ptr, my_tail.p.sn, my_next.p.ptr);
            }
        }
    }
    // try to swing B->tail to newly inserted node
    (void) cp_cas(&tail.all, my_tail.p.ptr, my_tail.p.sn, qn);
}
// explicit instantiation:
template void MS_queue<edge*>::enqueue(edge* t, const int tid);

// Returns 0 if queue was empty.  Since payloads are required to be
// pointers, this is ok.
//
template<typename T>
T MS_queue<T>::dequeue(const int tid)
{
    counted_ptr my_head, my_tail;
    qnode_t<T>* my_next;
    T rtn;

    while (true) {
        my_head.all = a_ld(&head.all);
        my_tail.all = a_ld(&tail.all);
        my_next = (qnode_t<T>*)((qnode_t<T>*)my_head.p.ptr)->next.p.ptr;
        counted_ptr my_head2;
        my_head2.all = a_ld(&head.all);
        if (my_head.all == my_head2.all) {
            // head, tail, and next are mutually consistent
            if (my_head.p.ptr != my_tail.p.ptr) {
                // Read value out of node before CAS. Otherwise another dequeue
                // might free the next node.
                rtn = my_next->t;
                // try to swing head to next node
                if (cp_cas(&head.all, my_head.p.ptr, my_head.p.sn, my_next)) {
                    break;                  // dequeue worked
                }
            }
            else {
                // queue is empty, or tail is falling behind
                if (my_next == 0)
                    // queue is empty
                    return T(0);
                // try to swing tail to next node
                (void) cp_cas(&tail.all, my_tail.p.ptr, my_tail.p.sn, my_next);
            }
        }
    }
    bp->free_block((void*)my_head.p.ptr, tid);
    return rtn;
}
// explicit instantiation:
template edge* MS_queue<edge*>::dequeue(const int tid);
