/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef DLIST_HPP__
#define DLIST_HPP__

#include <limits.h>
#include <api/api.hpp>

// Doubly-Linked List workload
class DList
{
    // Node is a node in the DList
    struct Node
    {
        int   m_val;
        Node* m_prev;
        Node* m_next;

        // basic constructor
        Node(int val) : m_val(val), m_prev(), m_next() { }
    };

  public:

    // the dlist keeps head and tail pointers, for bidirectional traversal
    Node* head;
    Node* tail;

    DList();

    // insert a node if it doesn't already exist
    TM_CALLABLE
    void insert(int val TM_ARG);

    // true iff val is in the data structure
    TM_CALLABLE
    bool lookup(int val TM_ARG) const;

    // remove a node if its value = val
    TM_CALLABLE
    void remove(int val TM_ARG);

    // make sure the list is in sorted order
    bool isSane() const;

    // increment all elements, moving forward
    TM_CALLABLE
    void increment_forward(TM_ARG_ALONE);

    // increment all elements, moving in reverse
    TM_CALLABLE
    void increment_backward(TM_ARG_ALONE);

    // increment every seqth element, starting with start, moving forward
    TM_CALLABLE
    void increment_forward_pattern(int start, int seq TM_ARG);

    // increment every seqth element, starting with start, moving backward
    TM_CALLABLE
    void increment_backward_pattern(int start, int seq TM_ARG);

    // read the whole list, then increment every element in the chunk
    // starting at chunk_num*chunk_size
    TM_CALLABLE
    void increment_chunk(int chunk_num, int chunk_size TM_ARG);
};


// constructor: head and tail have extreme values, point to each other
DList::DList() : head(new Node(-1)), tail(new Node(INT_MAX))
{
    head->m_next = tail;
    tail->m_prev = head;
}

// simple sanity check: make sure all elements of the list are in sorted order
bool DList::isSane(void) const
{
    bool sane = false;
    sane = true;

    // forward traversal
    const Node* prev(head);
    const Node* curr((prev->m_next));
    while (curr != NULL) {
        // ensure sorted order
        if ((prev->m_val) >= (curr->m_val)) {
            sane = false;
            break;
        }
        // ensure curr->prev->next == curr
        const Node* p ((curr->m_prev));
        if ((p->m_next) != curr) {
            sane = false;
            break;
        }

        prev = curr;
        curr = (curr->m_next);
    }

    // backward traversal
    prev = tail;
    curr = (prev->m_prev);
    while (curr != NULL) {
        // ensure sorted order
        if ((prev->m_val) < (curr->m_val)) {
            sane = false;
            break;
        }
        // ensure curr->next->prev == curr
        const Node* n ((curr->m_next));
        if ((n->m_prev) != curr) {
            sane = false;
            break;
        }

        prev = curr;
        curr = (curr->m_prev);
    }
    return sane;
}

// insert method; find the right place in the list, add val so that it is in
// sorted order; if val is already in the list, exit without inserting
TM_CALLABLE
void DList::insert(int val TM_ARG)
{
    // traverse the list to find the insertion point
    const Node* prev(head);
    const Node* curr(TM_READ(prev->m_next));

    while (curr != NULL) {
        if (TM_READ(curr->m_val) >= val)
            break;

        prev = curr;
        curr = TM_READ(prev->m_next);
    }

    // now insert new_node between prev and curr
    if (TM_READ(curr->m_val) > val) {
        Node* before = const_cast<Node*>(prev);
        Node* after = const_cast<Node*>(curr);

        // create the node
        Node* between = (Node*)TM_ALLOC(sizeof(Node));
        between->m_val = val;
        between->m_prev = before;
        between->m_next = after;

        TM_WRITE(before->m_next, between);
        TM_WRITE(after->m_prev, between);
    }
}

// search for a value
TM_CALLABLE
bool DList::lookup(int val TM_ARG) const
{
    bool found = false;

    const Node* curr(head);
    curr = TM_READ(curr->m_next);
    while (curr != NULL) {
        if (TM_READ(curr->m_val) >= val)
            break;
        curr = TM_READ(curr->m_next);
    }
    found = ((curr != NULL) && (TM_READ(curr->m_val) == val));

    return found;
}

// remove a node if its value == val
TM_CALLABLE
void DList::remove(int val TM_ARG)
{
    // find the node whose val matches the request
    const Node* prev(head);
    const Node* curr(TM_READ(prev->m_next));

    while (curr != NULL) {
        // if we find the node, disconnect it and end the search
        if (TM_READ(curr->m_val) == val) {
            Node* before = const_cast<Node*>(prev);
            Node* after(TM_READ(curr->m_next));
            TM_WRITE(before->m_next, after);
            TM_WRITE(after->m_prev, before);

            // delete curr...
            TM_FREE(const_cast<Node*>(curr));
            break;
        }
        else if (TM_READ(curr->m_val) > val) {
            // this means the search failed
            break;
        }

        prev = curr;
        curr = TM_READ(prev->m_next);
    }

}

TM_CALLABLE
void DList::increment_forward(TM_ARG_ALONE)
{
    // forward traversal
    const Node* prev(head);
    Node* curr(TM_READ(prev->m_next));
    while (curr != tail) {
        // increment curr
        TM_WRITE(curr->m_val, 1 + TM_READ(curr->m_val));
        curr = TM_READ(curr->m_next);
    }
}

TM_CALLABLE
void DList::increment_backward(TM_ARG_ALONE)
{
    // backward traversal
    const Node* prev(tail);
    Node* curr(TM_READ(prev->m_prev));
    while (curr != head) {
        // increment curr
        TM_WRITE(curr->m_val, 1 + TM_READ(curr->m_val));
        curr = TM_READ(curr->m_prev);
    }
}

// increment every seqth element, starting with start, moving forward
TM_CALLABLE
void DList::increment_forward_pattern(int start, int seq TM_ARG)
{
    int sum = 0;
    // forward traversal to element # start
    const Node* prev(head);
    const Node* curr(TM_READ(prev->m_next));
    for (int i = 0; i < start; i++) {
        curr = TM_READ(curr->m_next);
    }
    // now do the remainder of the traversal, incrementing every seqth
    // element
    int ctr = seq;
    while (curr != tail) {
        // increment the seqth element
        if (ctr == seq) {
            ctr = 0;
            Node* cw = const_cast<Node*>(curr);
            TM_WRITE(cw->m_val, 1 + TM_READ(cw->m_val));
            curr = cw;
        }
        ctr++;
        sum += TM_READ(curr->m_val);
        curr = TM_READ(curr->m_next);
    }
}

// increment every element, starting with start, moving backward
TM_CALLABLE
void DList::increment_backward_pattern(int start, int seq TM_ARG)
{
    int sum = 0;
    // backward traversal to element # start
    const Node* prev(tail);
    const Node* curr(TM_READ(prev->m_prev));
    for (int i = 0; i < start; i++) {
        curr = TM_READ(curr->m_prev);
    }
    // now do the remainder of the traversal, incrementing every seqth
    // element
    int ctr = seq;
    while (curr != head) {
        // increment the seqth element
        if (ctr == seq) {
            ctr = 0;
            Node* cw = const_cast<Node*>(curr);
            TM_WRITE(cw->m_val, 1 + TM_READ(cw->m_val));
            curr = cw;
        }
        ctr++;
        sum += TM_READ(curr->m_val);
        curr = TM_READ(curr->m_prev);
    }
}

// increment every seqth element, starting with start, moving forward
TM_CALLABLE
void DList::increment_chunk(int chunk_num, int chunk_size TM_ARG)
{
    int startpoint = chunk_num * chunk_size;

    int sum = 0;
    Node* chunk_start(NULL);
    int ctr = 0;

    // forward traversal to compute sum and to find chunk_start
    const Node* prev(head);
    const Node* curr(TM_READ(prev->m_next));
    while (curr != tail) {
        // if this is the start of our chunk, save the pointer
        if (ctr++ == startpoint)
            chunk_start = const_cast<Node*>(curr);
        // add this value to the sum
        sum += TM_READ(curr->m_val);
        // move to next node
        curr = TM_READ(curr->m_next);
    }
    // OK, at this point we should have the ID of the chunk we're going to
    // work on.  increment everything in our chunk
    if (chunk_start != NULL) {
        // avoid TLS overhead on every loop iteration:
        Node* wr(chunk_start);
        // increment /chunk_size/ elements
        for (int i = 0; i < chunk_size; i++) {
            // don't increment if we reach the tail
            if (chunk_start == tail)
                break;
            // increment, move to next
            TM_WRITE(wr->m_val, 1 + TM_READ(wr->m_val));
            wr = TM_READ(wr->m_next);
        }
    }
}

#endif // DLIST_HPP__
