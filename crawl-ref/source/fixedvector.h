/*
 *  File:       fixedvector.h
 *  Summary:    Fixed size vector class that asserts if you do something bad.
 *  Written by: Jesse Jones
 */

#ifndef FIXVEC_H
#define FIXVEC_H

#include <cstdarg>
#include <cstring>

#include "debug.h"
// ==========================================================================
//    class FixedVector
// ==========================================================================

template <class TYPE, int SIZE> class FixedVector
{

//-----------------------------------
//    Types
//
public:
    typedef TYPE            value_type;
    typedef TYPE&           reference;
    typedef const TYPE&     const_reference;
    typedef TYPE*           pointer;
    typedef const TYPE*     const_pointer;

    typedef unsigned long   size_type;
    typedef long            difference_type;

    typedef TYPE*           iterator;
    typedef const TYPE*     const_iterator;

//-----------------------------------
//    Initialization/Destruction
//
public:
    ~FixedVector()                           {}

    FixedVector()                            {}

    FixedVector(TYPE def) : mData()
    {
        init(def);
    }

    FixedVector(TYPE value0, TYPE value1, ...);
    // Allows for something resembling C array initialization, eg
    // instead of "int a[3] = {0, 1, 2}" you'd use "FixedVector<int, 3>
    // a(0, 1, 2)". Note that there must be SIZE arguments.

//-----------------------------------
//    API
//
public:
    // ----- Size -----
    bool empty() const { return SIZE == 0; }
    size_t size() const { return SIZE; }

    // ----- Access -----
    TYPE& operator[](unsigned long index) {
        ASSERT(index < SIZE);
        return mData[index];
    }

    const TYPE& operator[](unsigned long index) const {
        ASSERT(index < SIZE);
        return mData[index];
    }

    const TYPE* buffer() const { return mData; }
    TYPE* buffer() { return mData; }

    // ----- Iterating -----
    iterator begin() { return mData; }
    const_iterator begin() const { return mData; }

    iterator end() { return this->begin() + this->size(); }
    const_iterator end() const { return this->begin() + this->size(); }
    void init(const TYPE& def);

//-----------------------------------
//    Member Data
//
protected:
    TYPE    mData[SIZE];
};


// ==========================================================================
//    Outlined Methods
// ==========================================================================
template <class TYPE, int SIZE>
FixedVector<TYPE, SIZE>::FixedVector(TYPE value0, TYPE value1, ...)
{
    mData[0] = value0;
    mData[1] = value1;

    va_list ap;
    va_start(ap, value1);   // second argument is last fixed parameter

    for (int index = 2; index < SIZE; index++)
    {
        TYPE value = va_arg(ap, TYPE);
        mData[index] = value;
    }

    va_end(ap);
}

template <class TYPE, int SIZE>
void FixedVector<TYPE, SIZE>::init(const TYPE& def)
{
    for (int i = 0; i < SIZE; ++i)
        mData[i] = def;
}

#endif    // FIXVEC_H
