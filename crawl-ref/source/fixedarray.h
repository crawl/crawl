/**
 * @file
 * @brief Fixed size 2D vector class that asserts if you do something bad.
**/

#pragma once

#include <algorithm>

#include "fixedvector.h"

template <class TYPE, int WIDTH, int HEIGHT> class FixedArray
{
public:
    typedef TYPE            value_type;
    typedef TYPE&           reference;
    typedef const TYPE&     const_reference;
    typedef TYPE*           pointer;
    typedef const TYPE*     const_pointer;

    typedef unsigned long   size_type;
    typedef long            difference_type;

    // operator[] should return one of these to avoid breaking
    // client code (if inlining is on there won't be a speed hit)
    typedef FixedVector<TYPE, HEIGHT> Column;

public:
    ~FixedArray()                           {}

    FixedArray()                            {}

    FixedArray(TYPE def)
    {
        init(def);
    }

public:
    // ----- Size -----
    bool empty() const { return WIDTH == 0 || HEIGHT == 0; }
    int size() const { return WIDTH*HEIGHT; }
    int width() const { return WIDTH; }
    int height() const { return HEIGHT; }

    // ----- Access -----
    Column& operator[](unsigned long index) { return mData[index]; }
    const Column& operator[](unsigned long index) const
    {
        return mData[index];
    }

    template<class Indexer>
    TYPE& operator () (const Indexer &i)
    {
        return mData[i.x][i.y];
    }

    template<class First, class Second>
    TYPE& operator () (const pair<First,Second> &p)
    {
        return mData[p.first][p.second];
    }

    template<class Indexer>
    const TYPE& operator () (const Indexer &i) const
    {
        return mData[i.x][i.y];
    }

    template<class First, class Second>
    const TYPE& operator () (const pair<First,Second> &p) const
    {
        return mData[p.first][p.second];
    }

    void init(const TYPE& def)
    {
        for (auto &col : mData)
            col.init(def);
    }

protected:
    FixedVector<Column, WIDTH> mData;
};

// A fixed array centered around the origin.
template <class TYPE, int RADIUS> class SquareArray
{
public:
    typedef TYPE            value_type;
    typedef TYPE&           reference;
    typedef const TYPE&     const_reference;
    typedef TYPE*           pointer;
    typedef const TYPE*     const_pointer;

    typedef unsigned long   size_type;
    typedef long            difference_type;

public:
    ~SquareArray()                           {}

    SquareArray()                            {}

    SquareArray(TYPE def)
    {
        init(def);
    }

public:
    // ----- Size -----
    bool empty() const { return data.empty(); }
    int size() const { return data.size(); }
    int width() const { return data.width(); }
    int height() const { return data.height(); }

    // ----- Access -----
    template<class Indexer> TYPE& operator () (const Indexer &i)
    {
        return data[i.x+RADIUS][i.y+RADIUS];
    }

    template<class Indexer> const TYPE& operator () (const Indexer &i) const
    {
        return data[i.x+RADIUS][i.y+RADIUS];
    }

    void init(const TYPE& def)
    {
        data.init(def);
    }

protected:
    FixedArray<TYPE, 2*RADIUS+1, 2*RADIUS+1> data;
};
