#ifndef COLOUR_H
#define COLOUR_H

// various elemental colour schemes... used for abstracting random
// short lists. When adding colours, please also add their names in
// str_to_colour!
enum element_type
{
    ETC_FIRE = 32,      // fiery colours (must be first and > highest colour)
    ETC_ICE,            // icy colours
    ETC_EARTH,          // earthy colours
    ETC_ELECTRICITY,    // electrical side of air
    ETC_AIR,            // non-electric and general air magic
    ETC_POISON,         // used only for venom mage and stalker stuff
    ETC_WATER,          // used only for the elemental
    ETC_MAGIC,          // general magical effect
    ETC_MUTAGENIC,      // transmute, poly, radiation effects
    ETC_WARP,           // teleportation and anything similar
    ETC_ENCHANT,        // magical enhancements
    ETC_HEAL,           // holy healing (not necromantic stuff)
    ETC_HOLY,           // general "good" god effects
    ETC_DARK,           // darkness
    ETC_DEATH,          // currently only assassin (and equal to ETC_NECRO)
    ETC_NECRO,          // necromancy stuff
    ETC_UNHOLY,         // demonology stuff
    ETC_VEHUMET,        // vehumet's oddball colours
    ETC_BEOGH,          // Beogh altar colours
    ETC_CRYSTAL,        // colours of crystal
    ETC_BLOOD,          // colours of blood
    ETC_SMOKE,          // colours of smoke
    ETC_SLIME,          // colours of slime
    ETC_JEWEL,          // colourful
    ETC_ELVEN,          // used for colouring elf fabric items
    ETC_DWARVEN,        // used for colouring dwarf fabric items
    ETC_ORCISH,         // used for colouring orc fabric items
    ETC_GILA,           // gila monster colours
    ETC_KRAKEN,         // kraken colours
    ETC_FLOOR,          // colour of the area's floor
    ETC_ROCK,           // colour of the area's rock
    ETC_STONE,          // colour of the area's stone
    ETC_MIST,           // colour of mist
    ETC_SHIMMER_BLUE,   // shimmering colours of blue
    ETC_DECAY,          // colour of decay/swamp
    ETC_SILVER,         // colour of silver
    ETC_GOLD,           // colour of gold
    ETC_IRON,           // colour of iron
    ETC_BONE,           // colour of bone
    ETC_RANDOM,         // any colour (except BLACK)
};

int str_to_colour(const std::string &str, int default_colour = -1,
                  bool accept_number = true);
const std::string colour_to_str(unsigned char colour);
unsigned int str_to_tile_colour(std::string colour);

unsigned char random_colour();
unsigned char random_uncommon_colour();
unsigned char make_low_colour(unsigned char colour);
unsigned char make_high_colour(unsigned char colour);
bool is_element_colour(int col);
int  element_colour(int element, bool no_random = false);

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS) || defined(USE_TILE)
unsigned short dos_brand( unsigned short colour,
                          unsigned brand = CHATTR_REVERSE);
#endif

// Applies ETC_ colour substitutions and brands.
unsigned real_colour(unsigned raw_colour);

#endif
