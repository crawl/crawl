/*
 *  File:       fixary.h
 *  Summary:    Fixed size 2D vector class that asserts if you do something bad.
 *  Written by: Jesse Jones
 */

#ifndef FIXARY_H
#define FIXARY_H

#include "fixvec.h"


// ==========================================================================
//    class FixedArray
// ==========================================================================
template <class TYPE, int WIDTH, int HEIGHT> class FixedArray {

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

    // operator[] should return one of these to avoid breaking
    // client code (if inlining is on there won't be a speed hit)
    typedef FixedVector<TYPE, HEIGHT> Column;

//-----------------------------------
//    API
//
public:
    // ----- Size -----
    bool empty() const { return WIDTH == 0 || HEIGHT == 0; }
    int size() const { return WIDTH*HEIGHT; }
    int width() const { return WIDTH; }
    int height() const { return HEIGHT; }

    // ----- Access -----
    Column& operator[](unsigned long index) { return mData[index]; }
    const Column& operator[](unsigned long index) const {
        return mData[index];
    }

    template<class Indexer> TYPE& operator () (const Indexer &i) {
        return mData[i.x][i.y];
    }

    template<class Indexer> const TYPE& operator () (const Indexer &i) const {
        return mData[i.x][i.y];
    }

    void init(const TYPE& def) {
        for ( int i = 0; i < WIDTH; ++i )
            mData[i].init(def);
    }

protected:
    FixedVector<Column, WIDTH> mData;
};

template <typename Z>
class Matrix {
public:
    Matrix(int width, int height, const Z &initial);
    Matrix(int width, int height);
    ~Matrix();

    void init(const Z &initial);
    Z &operator () (int x, int y)
    {
        return data[x + y * width];
    }
    const Z &operator () (int x, int y) const
    {
        return data[x + y * width];
    }

private:
    Z *data;
    int width, height, size;
};

template <typename Z>
Matrix<Z>::Matrix(int _width, int _height, const Z &initial)
    : data(NULL), width(_width), height(_height), size(_width * _height)
{
    data = new Z [ size ];
    init(initial);
}

template <typename Z>
Matrix<Z>::Matrix(int _width, int _height)
    : data(NULL), width(_width), height(_height), size(_width * _height)
{
    data = new Z [ size ];
}

template <typename Z>
Matrix<Z>::~Matrix()
{
    delete [] data;
}

template <typename Z>
void Matrix<Z>::init(const Z &initial)
{
    for (int i = 0; i < size; ++i)
        data[i] = initial;
}

#endif    // FIXARY_H
