#ifndef RELIGION_ENUM_H
#define RELIGION_ENUM_H

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

/// Weapon for monster generation
#define IEOH_JIAN_WEAPON "ieoh-jian-weapon"
#define IEOH_JIAN_POWER "ieoh-jian-power"
/// What slot a weapon occupies when it was "gifted" by the IJC
/// A lack of this value indicates the weapon belongs to you
#define IEOH_JIAN_SLOT "ieoh-jian-slot"
/// Whether a IJC driven weapon swap is taking place. This supresses
/// some warnings and dangerous brand effects.
#define IEOH_JIAN_SWAPPING "ieoh-jian-swapping"
/// The time left for IJC divine weapons.
#define IEOH_JIAN_DIVINE_DEGREE "ieoh-jian-divine"
/// The momentum bonus for divine weapons.
#define IEOH_JIAN_DIVINE_MOMENTUM "ieoh-jian-momentum"
/// Whether the weapon is being projected
#define IEOH_JIAN_PROJECTED "ieoh-jian-projected"

#endif
