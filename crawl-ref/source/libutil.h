#ifndef LIBUTIL_H
#define LIBUTIL_H

template <typename Z>
static void erase_any(std::vector<Z> &vec, unsigned long which)
{
    if (which != vec.size() - 1)
        vec[which] = vec[vec.size() - 1];
    vec.pop_back();
}

static inline int sqr(int x)
{
    return x * x;
}

int term_width();
#endif
