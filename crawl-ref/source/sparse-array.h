/*
 * File:     sparse-array.h
 * Summary:  Sparse analog to FixedArray.
 *
 * This is meant for efficiently storing sparse grids like
 * the monster grid, item grid, cloud grid. It is meant
 * to provide...
 */

template <class TYPE>
struct tree_node
{
    rect_def bbox;


// pass default value as template parameter? (TYPE DEF)
template <class TYPE, rect_def BBOX>
class SparseArray
{
public:
    SparseArray(const TYPE& def);

    const TYPE& operator[](const coord_def& c) const;
    TYPE& operator[](const coord_def& c);

    const rect_def& bbox();

    iterator iter(const rect_def& box=BBOX);
    const_iterator iter(const rect_def& box=BBOX);

private:
    const TYPE& def;

    tree_node<TYPE>* root;
};
