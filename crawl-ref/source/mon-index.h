#ifndef MON_INDEX_H
#define MON_INDEX_H

struct auto_mindex
{
private:
    int index;

    void retain();
    void release();

public:
    operator int() const { return index; }

    auto_mindex(int n = MHITNOT): index(n) { retain(); }
    auto_mindex(const auto_mindex &other): index(other.index) { retain(); }
    ~auto_mindex() { release(); }

    void operator=(const auto_mindex &other) { *this = other.index; }
    void operator=(const int n)
    {
        if(index != n)
        {
            release();
            index = n;
            retain();
        }
    }

    void replace(int n) { index = n; }
};

#endif

