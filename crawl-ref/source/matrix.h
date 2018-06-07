/**
 * @file
 * @brief Two-dimensional array class.
**/

#pragma once

template <typename Z>
class Matrix
{
public:
    Matrix(int _width, int _height)
        : mwidth(_width), mheight(_height), size(_width * _height),
          data(new Z[size])
    {
    }
    Matrix(int _width, int _height, const Z &initial)
        : Matrix(_width, _height)
    {
        init(initial);
    }

    void init(const Z &initial)
    {
        fill(data.get(), data.get() + size, initial);
    }

    Z &operator () (int x, int y)
    {
        return data[x + y * mwidth];
    }
    Z &operator () (coord_def c)
    {
        return (*this)(c.x, c.y);
    }
    const Z &operator () (int x, int y) const
    {
        return data[x + y * mwidth];
    }
    const Z &operator () (coord_def c) const
    {
        return (*this)(c.x, c.y);
    }

    int width() const { return mwidth; }
    int height() const { return mheight; }

private:
    int mwidth, mheight, size;
    unique_ptr<Z[]> data;
};
