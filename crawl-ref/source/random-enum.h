#ifndef RANDOM_ENUM_H
#define RANDOM_ENUM_H

// A type for a generalized "bool random" parameter.
enum random_type
{
    R_RANDOM,   // actual random value, e.g. random2(n)
    R_EXPECTED, // expected value            (n-1)/2
    R_MAXIMUM,  // maximal value             n-1
    R_MINIMUM   // minimal value             0
};

#endif
