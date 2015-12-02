/**
 * @file
 * @brief Two-dimensional array class.
**/

#ifndef MATRIX_H
#define MATRIX_H

template <typename Z>
class Matrix
{
public:
    Matrix(int width, int height)
        : mwidth(width), mheight(height), size(width * height),
          data(new Z[size])
    {
    }
    Matrix(int width, int height, const Z &initial)
        : Matrix(width, height)
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
#endif    // MATRIX_H
