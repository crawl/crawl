#pragma once

enum piety_gain_t
{
    PIETY_NONE, PIETY_SOME, PIETY_LOTS,
    NUM_PIETY_GAIN
};

#if TAG_MAJOR_VERSION == 34
enum nemelex_gift_types
{
    NEM_GIFT_ESCAPE = 0,
    NEM_GIFT_DESTRUCTION,
    NEM_GIFT_SUMMONING,
    NEM_GIFT_WONDERS,
    NUM_NEMELEX_GIFT_TYPES,
};
#endif

#define ACQUIRE_KEY "acquired" // acquirement source prop on acquired items

/// the name of the ally hepliaklqana granted the player
#define HEPLIAKLQANA_ALLY_NAME_KEY "hepliaklqana_ally_name"
/// ancestor gender
#define HEPLIAKLQANA_ALLY_GENDER_KEY "hepliaklqana_ally_gender"
/// chosen ancestor class (monster_type)
#define HEPLIAKLQANA_ALLY_TYPE_KEY "hepliaklqana_ally_type"

/// custom monster gender
#define MON_GENDER_KEY "mon_gender"
