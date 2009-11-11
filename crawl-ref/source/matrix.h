/*
 * File:     matrix.h
 * Summary:  Two-dimensional array class.
 */

#ifndef MATRIX_H
#define MATRIX_H

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

#endif    // MATRIX_H
