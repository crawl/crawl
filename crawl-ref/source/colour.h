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
    ETC_ELVEN_BRICK,    // colour of the walls in the Elven Halls
    ETC_WAVES,          // cyan, with regularly occurring lightcyan waves
    ETC_TREE,           // colour of trees
    ETC_RANDOM,         // any colour (except BLACK)
    ETC_FIRST_LUA = 96, // colour indices have to be <128
};

typedef int (*element_colour_calculator)(int, const coord_def&);

struct element_colour_calc
{
    element_type type;
    std::string name;

    element_colour_calc(element_type _type, std::string _name,
                        element_colour_calculator _calc)
        : type(_type), name(_name), calc(_calc)
        {};

    virtual int get(const coord_def& loc = coord_def(),
                    bool non_random = false);

protected:
    int rand(bool non_random);

    element_colour_calculator calc;
};

int str_to_colour(const std::string &str, int default_colour = -1,
                  bool accept_number = true);
const std::string colour_to_str(uint8_t colour);
unsigned int str_to_tile_colour(std::string colour);

void init_element_colours();
void add_element_colour(element_colour_calc *colour);
uint8_t random_colour();
uint8_t random_uncommon_colour();
uint8_t make_low_colour(uint8_t colour);
uint8_t make_high_colour(uint8_t colour);
bool is_element_colour(int col);
int  element_colour(int element, bool no_random = false,
                    const coord_def& loc = coord_def());

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS) || defined(USE_TILE)
unsigned short dos_brand( unsigned short colour,
                          unsigned brand = CHATTR_REVERSE);
#endif

// Applies ETC_ colour substitutions and brands.
unsigned real_colour(unsigned raw_colour, const coord_def& loc = coord_def());

#endif
