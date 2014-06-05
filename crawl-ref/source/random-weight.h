#ifndef RANDOM_WEIGHT_H
#define RANDOM_WEIGHT_H

/**
 * Get a random weighted choice.
 *
 * Weights are assumed to be non-negative, but are allowed to be zero.
 * @param   choices  The vector of choice-weight pairs to choose from.
 *
 * @return  A pointer to the item in the chosen pair, or NULL if all
 *          weights are zero.
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

/**
 * Get an index for a random weighted choice using a fixed vector of
 * weights.
 *
 * Entries with a weight <= 0 are skipped.
 * @param choices The fixed vector with weights for each item.
 *
 * @return  A index corresponding to the selected item, or -1 if all
 *          weights were skipped.
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
