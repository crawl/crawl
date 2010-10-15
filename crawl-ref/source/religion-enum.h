#ifndef RELIGION_ENUM_H
#define RELIGION_ENUM_H

enum piety_gain_t
{
    PIETY_NONE, PIETY_SOME, PIETY_LOTS,
    NUM_PIETY_GAIN
};

enum harm_protection_type
{
    HPT_NONE = 0,
    HPT_PRAYING,
    HPT_ANYTIME,
    HPT_PRAYING_PLUS_ANYTIME,
    HPT_RELIABLE_PRAYING_PLUS_ANYTIME,
    NUM_HPTS
};
#endif
