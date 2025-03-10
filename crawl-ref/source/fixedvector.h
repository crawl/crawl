/**
 * @file
 * @brief Fixed size vector class that asserts if you do something bad.
**/

#pragma once

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <vector>

#include "debug.h"

using std::vector;

// TODO: this class precedes std::array and is quite redundant with it, except
// for the bounds check in operator[] (which std::array only has on `at`).
// Should we replace / subclass / use as an underlying implementation?

template <class TYPE, int SIZE> class FixedVector
{

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

public:
    // ----- Size -----
    bool empty() const { return SIZE == 0; }
    size_t size() const { return SIZE; }

    // ----- Access -----
    // why don't these use size_t??
    // these are more like std::array::at than std::array::operator[]
    TYPE& operator[](unsigned long index)
    {
#ifdef ASSERTS
        if (index >= SIZE)
        {
            // Intentionally printed as signed, it's very, very unlikely we'd
            // have a genuine big number here, but underflows are common.
            die_noline("range check error (%ld / %d)", (signed long)index,
                SIZE);
        }
#endif
        return mData[index];
    }

    const TYPE& operator[](unsigned long index) const
    {
#ifdef ASSERTS
        if (index >= SIZE)
        {
            die_noline("range check error (%ld / %d)", (signed long)index,
                SIZE);
        }
#endif
        return mData[index];
    }

    const TYPE* buffer() const { return mData; }
    TYPE* buffer() { return mData; }

    // ----- Iterating -----
    iterator begin() { return mData; }
    const_iterator begin() const { return mData; }

    iterator end() { return begin() + size(); }
    const_iterator end() const { return begin() + size(); }
    void init(const TYPE& def);

    // std::array api
    void fill(const TYPE& def) { init(def); }

protected:
    TYPE    mData[SIZE];
};

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
    std::fill(std::begin(mData), std::end(mData), def);
}
