/**
 * @file
 * @brief Bit array data type.
**/

#include "AppHdr.h"

#include "bitary.h"

bit_vector::bit_vector(unsigned long s)
    : size(s)
{
    nwords = static_cast<int>((size + LONGSIZE - 1) / LONGSIZE);
    data = new unsigned long[nwords];
    reset();
}

bit_vector::bit_vector(const bit_vector& other) : size(other.size)
{
    nwords = static_cast<int>((size + LONGSIZE - 1) / LONGSIZE);
    data = new unsigned long[nwords];
    for (int w = 0; w < nwords; ++w)
        data[w] = other.data[w];
}

bit_vector::~bit_vector()
{
    delete[] data;
}

void bit_vector::reset()
{
    for (int w = 0; w < nwords; ++w)
        data[w] = 0;
}

bool bit_vector::get(unsigned long index) const
{
    ASSERT(index < size);
    int w = index / LONGSIZE;
    int b = index % LONGSIZE;
    return data[w] & (1UL << b);
}

void bit_vector::set(unsigned long index, bool value)
{
    ASSERT(index < size);
    int w = index / LONGSIZE;
    int b = index % LONGSIZE;
    if (value)
        data[w] |= (1UL << b);
    else
        data[w] &= ~(1UL << b);
}

bit_vector& bit_vector::operator |= (const bit_vector& other)
{
    ASSERT(size == other.size);
    for (int w = 0; w < nwords; ++w)
        data[w] |= other.data[w];
    return *this;
}

bit_vector& bit_vector::operator &= (const bit_vector& other)
{
    ASSERT(size == other.size);
    for (int w = 0; w < nwords; ++w)
        data[w] &= other.data[w];
    return *this;
}

bit_vector bit_vector::operator & (const bit_vector& other) const
{
    ASSERT(size == other.size);
    bit_vector res = bit_vector(size);
    for (int w = 0; w < nwords; ++w)
        res.data[w] = data[w] & other.data[w];
    return res;
}
