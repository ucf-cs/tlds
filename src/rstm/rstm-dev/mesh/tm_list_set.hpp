/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  tm_list_set.hpp
 *
 *  Simple list-based set.
 *  Element type must be the same size as unsigned long.
 */

#ifndef TM_LIST_SET_HPP__
#define TM_LIST_SET_HPP__

#include <cassert>
#include "common.hpp"
#include "tm_set.hpp"

template<typename T>
class tm_list_set;

// LLNode is a single node in a sorted linked list
//
template<class T>
class LLNode
{
    friend class tm_list_set<T>;   // all fields are nominally private

    const T val;
    LLNode<T>* next_node;

    TRANSACTION_SAFE
    LLNode(const T val, LLNode<T>* next) : val(val), next_node(next) { }
};

template<typename T>
class tm_list_set : public tm_set<T>
{
    LLNode<T>* head_node;

    // no implementation; forbids passing by value
    tm_list_set(const tm_list_set&);
    // no implementation; forbids copying
    tm_list_set& operator=(const tm_list_set&);

  public:
    // note that assert in constructor guarantees that items and
    // unsigned ints are the same size

    TRANSACTION_SAFE void insert(const T item)
    {
        // traverse the list to find the insertion point
        LLNode<T>* prev = head_node;
        LLNode<T>* curr = prev->next_node;
        while (curr != 0) {
            if (curr->val == item) return;
            prev = curr;
            curr = prev->next_node;
        }
        prev->next_node = new LLNode<T>(item, curr);
    }

    TRANSACTION_SAFE void remove(const T item)
    {
        // find the node whose val matches the request
        LLNode<T>* prev = head_node;
        LLNode<T>* curr = prev->next_node;
        while (curr != 0) {
            // if we find the node, disconnect it and end the search
            if (curr->val == item) {
                prev->next_node = curr->next_node;
                delete curr;
                break;
            }
            prev = curr;
            curr = prev->next_node;
        }
    }

    // find out whether item is in list
    bool lookup(const T item)
    {
        LLNode<T>* curr = head_node;
        curr = curr->next_node;
        while (curr != 0) {
            if (curr->val == item) return true;
            curr = curr->next_node;
        }
        return false;
    }

    // apply a function to every element of the list
    void apply_to_all(void (*f)(T item))
    {
        LLNode<T>* curr = head_node;
        curr = curr->next_node;
        while (curr != 0) {
            f(curr->val);
            curr = curr->next_node;
        }
    }

    tm_list_set() : head_node(new LLNode<T>(0, 0))
    {
        assert(sizeof(T) == sizeof(unsigned long));
    }

    // Destruction not currently supported.
    virtual ~tm_list_set() { assert(false); }

    // count elements in the list
    int size() const
    {
        int rtn = 0;
        LLNode<T>* curr = head_node->next_node;
        while (curr != 0) {
            rtn++;
            curr = curr->next_node;
        }
        return rtn;
    }
};

#endif // TM_LIST_SET_HPP__
