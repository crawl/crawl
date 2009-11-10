/*
 * File:      grid.h
 * Summary:   Base class for grid implementations.
 */

template <class TYPE>
class grid_def
{
    grid_def();
    grid_def(const TYPE& def);

    TYPE& operator[](const coord_def& c);
    const TYPE& operator[](const coord_def& c) const;

    void init(const TYPE& def);

    const rect_def& 
