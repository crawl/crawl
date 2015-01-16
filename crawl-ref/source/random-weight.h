#ifndef RANDOM_WEIGHT_H
#define RANDOM_WEIGHT_H

#include "random.h"

/**
 * Get a random weighted choice.
 *
 * Weights are assumed to be non-negative, but are allowed to be zero.
 * @tparam  V  A map, vector of pairs, etc., with the values of the
 *             map or the second elements of the pairs being integer
 *             weights.
 *
 * @param   choices  The collection of choice-weight pairs to choose from.
 *
 * @return  A pointer to the item in the chosen pair, or nullptr if all
 *          weights are zero.  The pointer is const only if necessary.
 */
template <typename V>
auto random_choose_weighted(V &choices) -> decltype(&(begin(choices)->first))
{
    int total = 0;
    for (const auto &entry : choices)
        total += entry.second;
    int r = random2(total);
    int sum = 0;
    for (auto &entry : choices)
    {
        sum += entry.second;
        if (sum > r)
            return &entry.first;
    }
    return nullptr;
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
int random_choose_weighted(const FixedVector<T, SIZE>& choices)
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
