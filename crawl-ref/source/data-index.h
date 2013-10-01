/**
 * @file
 * @brief Functions for managing indexed tables of data attached to enums
**/

#ifndef DATAINDEX_H
#define DATAINDEX_H

template <typename TKey, typename TVal, int NMax>
class data_index
{
public:
    data_index(const TVal* _pop) : pop(_pop), index_created(false)
    { }

    const TVal* operator[](TKey key)
    {
        ASSERT_RANGE(key, 0, NMax);
        check_index();
        return &pop[index[key]];
    }

    bool contains(TKey key)
    {
        ASSERT_RANGE(key, 0, NMax);
        check_index();
        return (index[key] >= 0);
    }

protected:
    const TVal* pop;
    bool index_created;
    int index[NMax];
    virtual TKey map(const TVal* val) = 0;

    void check_index()
    {
        if (!index_created)
        {
            memset(index, -1, sizeof(index));
            int i = 0;
            for (const TVal* p = pop; map(p); p++)
            {
                index[map(p)] = i;
                i++;
            }
            index_created = true;
        }
    }
};

#endif
