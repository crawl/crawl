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
    Matrix(int width, int height, const Z &initial);
    Matrix(int width, int height);
    ~Matrix();

    void init(const Z &initial);
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
    Z *data;
    int mwidth, mheight, size;
};

template <typename Z>
Matrix<Z>::Matrix(int _width, int _height, const Z &initial)
    : data(nullptr), mwidth(_width), mheight(_height), size(_width * _height)
{
    data = new Z [ size ];
    init(initial);
}

template <typename Z>
Matrix<Z>::Matrix(int _width, int _height)
    : data(nullptr), mwidth(_width), mheight(_height), size(_width * _height)
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

#endif    // MATRIX_H
