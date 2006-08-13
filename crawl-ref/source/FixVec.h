/*
 *  File:       FixVec.h
 *  Summary:    Fixed size vector class that asserts if you do something bad.
 *  Written by: Jesse Jones
 *
 *  Change History (most recent first):    
 *
 *         <2>     7/29/00    JDJ        Added a variable argument ctor.
 *         <1>     6/16/00    JDJ        Created
 */

#ifndef FIXVEC_H
#define FIXVEC_H

#include <stdarg.h>

#include "debug.h"


// ==========================================================================
//    class FixedVector
// ==========================================================================

template <class TYPE, int SIZE> class FixedVector {

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

#if 0
#if MSVC >= 1100
    typedef std::reverse_iterator<const_iterator, const TYPE>    const_reverse_iterator;
    typedef std::reverse_iterator<iterator, TYPE>                reverse_iterator;
#else
    typedef std::reverse_iterator<const_iterator>    const_reverse_iterator;
    typedef std::reverse_iterator<iterator>          reverse_iterator;
#endif
#endif

//-----------------------------------
//    Initialization/Destruction
//
public:
    ~FixedVector()                           {}

    FixedVector()                            {}

    FixedVector(TYPE value0, TYPE value1, ...);
    // Allows for something resembling C array initialization, eg
    // instead of "int a[3] = {0, 1, 2}" you'd use "FixedVector<int, 3>
    // a(0, 1, 2)". Note that there must be SIZE arguments.

// public:
//                        FixedVector(const FixedVector& rhs);
//
//            FixedVector& operator=(const FixedVector& rhs);

//-----------------------------------
//    API
//
public:
    // ----- Size -----
            bool        empty() const                          {return SIZE == 0;}
            int         size() const                           {return SIZE;}

    // ----- Access -----
            TYPE&       operator[](unsigned long index)        {ASSERT(index < SIZE); return mData[index];}
            const TYPE& operator[](unsigned long index) const  {ASSERT(index < SIZE); return mData[index];}
            
            TYPE&       front()                                {ASSERT(SIZE > 0); return mData[0];}
            const TYPE& front() const                          {ASSERT(SIZE > 0); return mData[0];}

            TYPE&       back()                                 {ASSERT(SIZE > 0); return mData[SIZE - 1];}
            const TYPE& back() const                           {ASSERT(SIZE > 0); return mData[SIZE - 1];}
            
            TYPE*       buffer()                               {return mData;}
            const TYPE* buffer() const                         {return mData;}

    // ----- Iterating -----
            iterator                 begin()                   {return mData;}
            const_iterator           begin() const             {return mData;}

            iterator                 end()                     {return this->begin() + this->size();}
            const_iterator           end() const               {return this->begin() + this->size();}

//          reverse_iterator         rbegin()                  {return reverse_iterator(this->end());}
//          const_reverse_iterator   rbegin() const            {return const_reverse_iterator(this->end());}
                        
//          reverse_iterator         rend()                    {return reverse_iterator(this->begin());}
//          const_reverse_iterator   rend() const              {return const_reverse_iterator(this->begin());}

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

        for (int index = 2; index < SIZE; index++) {
                TYPE value = va_arg(ap, TYPE);

                mData[index] = value;
        }

        va_end(ap);
}

#endif    // FIXVEC_H
