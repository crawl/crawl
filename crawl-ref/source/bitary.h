/**
 * @file
 * @brief Bit array data type.
 *
 * Just contains the operations required by los.cc
 * for the moment.
**/

#ifndef BITARY_H
#define BITARY_H

#include "debug.h"

class bit_array
{
public:
    bit_array(unsigned long size = 0);
    ~bit_array();

    void reset();

    bool get(unsigned long index) const;
    void set(unsigned long index, bool value = true);

    bit_array& operator |= (const bit_array& other);
    bit_array& operator &= (const bit_array& other);
    bit_array  operator & (const bit_array& other) const;

protected:
    unsigned long size;
    int nwords;
    unsigned long *data;
};

#define LONGSIZE (sizeof(unsigned long)*8)

template <unsigned int SIZE> class FixedBitArray
{
protected:
    unsigned long data[(SIZE + LONGSIZE - 1) / LONGSIZE];
public:
    void reset()
    {
        for (unsigned int i = 0; i < sizeof(data) / sizeof(unsigned long); i++)
            data[i] = 0;
    }

    FixedBitArray()
    {
        reset();
    }

    inline bool get(unsigned int i) const
    {
#ifdef ASSERTS
        // printed as signed, as in FixedVector
        if (i >= SIZE)
            die("bit array range error: %d / %u", (int)i, SIZE);
#endif
        return data[i / LONGSIZE] & 1UL << i % LONGSIZE;
    }

    inline bool operator[](unsigned int i) const
    {
        return get(i);
    }

    inline void set(unsigned int i, bool value = true)
    {
#ifdef ASSERTS
        if (i >= SIZE)
            die("bit array range error: %d / %u", (int)i, SIZE);
#endif
        if (value)
            data[i / LONGSIZE] |= 1UL << i % LONGSIZE;
        else
            data[i / LONGSIZE] &= ~(1UL << i % LONGSIZE);
    }

    inline FixedBitArray<SIZE>& operator|=(const FixedBitArray<SIZE>&x)
    {
        for (unsigned int i = 0; i < sizeof(data) / sizeof(unsigned long); i++)
            data[i] |= x.data[i];
        return *this;
    }

    inline FixedBitArray<SIZE>& operator&=(const FixedBitArray<SIZE>&x)
    {
        for (unsigned int i = 0; i < sizeof(data) / sizeof(unsigned long); i++)
            data[i] &= x.data[i];
        return *this;
    }
};

#endif
