/*
 *  File:       FixAry.h
 *  Summary:    Fixed size 2D vector class that asserts if you do something bad.
 *  Written by: Jesse Jones
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):    
 *
 *         <1>     6/16/00    JDJ        Created 
 */

#ifndef FIXARY_H
#define FIXARY_H

#include "FixVec.h"


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


#endif    // FIXARY_H
