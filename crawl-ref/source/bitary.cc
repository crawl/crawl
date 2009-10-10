/*
 *  File:         bitary.cc
 *  Summary:      Bit array data type.
 *  Created by:   Robert Vollmert
 */

#include "AppHdr.h"

#include "bitary.h"

#include "debug.h"

#define LONGSIZE (sizeof(unsigned long)*8)

bit_array::bit_array(unsigned long s)
    : size(s)
{
    nwords = (int) ((size + LONGSIZE - 1) / LONGSIZE);
    data = new unsigned long[nwords];
    reset();
}

bit_array::~bit_array()
{
    delete[] data;
}

void bit_array::reset()
{
    for (int w = 0; w < nwords; ++w)
        data[w] = 0;
}

bool bit_array::get(unsigned long index) const
{
    ASSERT(index < size);
    int w = index / LONGSIZE;
    int b = index % LONGSIZE;
    return (data[w] & (1UL << b));
}

void bit_array::set(unsigned long index, bool value)
{
    ASSERT(index < size);
    int w = index / LONGSIZE;
    int b = index % LONGSIZE;
    if (value)
        data[w] |= (1UL << b);
    else
        data[w] &= ~(1UL << b);
}

bit_array& bit_array::operator |= (const bit_array& other)
{
    ASSERT(size == other.size);
    for (int w = 0; w < nwords; ++w)
        data[w] |= other.data[w];
    return (*this);
}

bit_array& bit_array::operator &= (const bit_array& other)
{
    ASSERT(size == other.size);
    for (int w = 0; w < nwords; ++w)
        data[w] &= other.data[w];
    return (*this);
}

bit_array bit_array::operator & (const bit_array& other) const
{
    ASSERT(size == other.size);
    bit_array res = bit_array(size);
    for (int w = 0; w < nwords; ++w)
        res.data[w] = data[w] & other.data[w];
    return (res);
}
