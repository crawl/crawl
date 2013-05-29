#ifndef RANDOM_WEIGHT_H
#define RANDOM_WEIGHT_H

/*
 * Weighted choice.
 *
 * Weights are assumed to be non-negative, but are allowed to be zero.
 * Returns NULL if nothing found, i.e., if all weights are zero.
 */
template <typename T>
T* random_choose_weighted(vector<pair<T, int> >& choices)
{
    int total = 0;
    for (unsigned int i = 0; i < choices.size(); i++)
        total += choices[i].second;
    int r = random2(total);
    int sum = 0;
    for (unsigned int i = 0; i < choices.size(); i++)
    {
        sum += choices[i].second;
        if (sum > r)
            return &choices[i].first;
    }
    return NULL;
}

/*
 * Same as above, but weights are in a FixedVector. Entries with a weight <= 0
 * are skipped. Returns the index of the chosen entry, or -1 if nothing found.
 */
template <typename T, int SIZE>
int random_choose_weighted(FixedVector<T, SIZE>& choices)
{
    int total = 0;
    for (int i = 0; i < SIZE; ++i)
        if (choices[i] > 0)
            total += choices[i];

    int r = random2(total);
    int sum = 0;
    for (int i = 0; i < SIZE; ++i)
    {
        if (choices[i] <= 0)
            continue;

        sum += choices[i];
        if (sum > r)
            return i;
    }
    return -1;
}

#endif
