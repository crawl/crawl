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
#include "defines.h"

class bit_vector
{
public:
    bit_vector(unsigned long size = 0);
    ~bit_vector();

    void reset();

    bool get(unsigned long index) const;
    void set(unsigned long index, bool value = true);

    bit_vector& operator |= (const bit_vector& other);
    bit_vector& operator &= (const bit_vector& other);
    bit_vector  operator & (const bit_vector& other) const;

protected:
    unsigned long size;
    int nwords;
    unsigned long *data;
};

#define LONGSIZE (sizeof(unsigned long)*8)
#ifndef ULONG_MAX
#define ULONG_MAX ((unsigned long)(-1))
#endif

template <unsigned int SIZE> class FixedBitVector
{
protected:
    unsigned long data[(SIZE + LONGSIZE - 1) / LONGSIZE];
public:
    void reset()
    {
        for (unsigned int i = 0; i < ARRAYSZ(data); i++)
            data[i] = 0;
    }

    FixedBitVector()
    {
        reset();
    }

    inline bool get(unsigned int i) const
    {
#ifdef ASSERTS
        // printed as signed, as in FixedVector
        if (i >= SIZE)
            die("bit vector range error: %d / %u", (int)i, SIZE);
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
            die("bit vector range error: %d / %u", (int)i, SIZE);
#endif
        if (value)
            data[i / LONGSIZE] |= 1UL << i % LONGSIZE;
        else
            data[i / LONGSIZE] &= ~(1UL << i % LONGSIZE);
    }

    inline FixedBitVector<SIZE>& operator|=(const FixedBitVector<SIZE>&x)
    {
        for (unsigned int i = 0; i < ARRAYSZ(data); i++)
            data[i] |= x.data[i];
        return *this;
    }

    inline FixedBitVector<SIZE>& operator&=(const FixedBitVector<SIZE>&x)
    {
        for (unsigned int i = 0; i < ARRAYSZ(data); i++)
            data[i] &= x.data[i];
        return *this;
    }

    void init(bool value)
    {
        long fill = value ? ~(long)0 : 0;
        for (unsigned int i = 0; i < ARRAYSZ(data); i++)
            data[i] = fill;
    }
};

template <unsigned int SIZEX, unsigned int SIZEY> class FixedBitArray
{
protected:
    unsigned long data[(SIZEX*SIZEY + LONGSIZE - 1) / LONGSIZE];
public:
    void reset()
    {
        for (unsigned int i = 0; i < ARRAYSZ(data); i++)
            data[i] = 0;
    }

    void init(bool def)
    {
        for (unsigned int i = 0; i < ARRAYSZ(data); i++)
            data[i] = def ? ULONG_MAX : 0;
    }

    FixedBitArray()
    {
        reset();
    }

    FixedBitArray(bool def)
    {
        init(def);
    }

    inline bool get(int x, int y) const
    {
#ifdef ASSERTS
        // printed as signed, as in FixedArray
        if (x < 0 || y < 0 || x >= (int)SIZEX || y >= (int)SIZEY)
            die("bit array range error: %d,%d / %u,%u", x, y, SIZEX, SIZEY);
#endif
        unsigned int i = y * SIZEX + x;
        return data[i / LONGSIZE] & 1UL << i % LONGSIZE;
    }

    template<class Indexer> inline bool get(const Indexer &i) const
    {
        return get(i.x, i.y);
    }

    inline bool operator () (int x, int y) const
    {
        return get(x, y);
    }

    template<class Indexer> inline bool operator () (const Indexer &i) const
    {
        return get(i.x, i.y);
    }

    inline void set(int x, int y, bool value = true)
    {
#ifdef ASSERTS
        if (x < 0 || y < 0 || x >= (int)SIZEX || y >= (int)SIZEY)
            die("bit array range error: %d,%d / %u,%u", x, y, SIZEX, SIZEY);
#endif
        unsigned int i = y * SIZEX + x;
        if (value)
            data[i / LONGSIZE] |= 1UL << i % LONGSIZE;
        else
            data[i / LONGSIZE] &= ~(1UL << i % LONGSIZE);
    }

    template<class Indexer> inline void set(const Indexer &i, bool value = true)
    {
        return set(i.x, i.y, value);
    }

    inline FixedBitArray<SIZEX, SIZEY>& operator|=(const FixedBitArray<SIZEX, SIZEY>&x)
    {
        for (unsigned int i = 0; i < ARRAYSZ(data); i++)
            data[i] |= x.data[i];
        return *this;
    }

    inline FixedBitArray<SIZEX, SIZEY>& operator&=(const FixedBitArray<SIZEX, SIZEY>&x)
    {
        for (unsigned int i = 0; i < ARRAYSZ(data); i++)
            data[i] &= x.data[i];
        return *this;
    }
};

#endif
