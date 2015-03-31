/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  tm_set.hpp
 *
 *  Simple interface for sets.
 */

#ifndef TM_SET_HPP__
#define TM_SET_HPP__

template<typename T>
class tm_set {
  public:
    virtual void insert(const T item) = 0;
    virtual void remove(const T item) = 0;
    virtual bool lookup(const T item) = 0;

    virtual void apply_to_all(void (*f)(T item)) = 0;

    virtual ~tm_set() { }
};

#endif // TM_SET_HPP__
