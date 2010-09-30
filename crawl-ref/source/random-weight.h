#ifndef RANDOM_WEIGHT_H
#define RANDOM_WEIGHT_H

/*
 * Weighted choice.
 *
 * Weights are assumed to be non-negative, but are allowed to be zero.
 * Returns NULL if nothing found, i.e., if all weights are zero.
 */
template <typename T>
T* random_choose_weighted(std::vector<std::pair<T, int> >& choices)
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
            return (&choices[i].first);
    }
    return (NULL);
}

#endif
