#pragma once

#include "tag-version.h"

struct monster_info;

// various elemental colour schemes... used for abstracting random
// short lists. When adding colours, please also add their names in
// str_to_colour!
enum element_type
{
    ETC_FIRE = 32,      // fiery colours (must be first and > highest colour)
    ETC_FIRST = ETC_FIRE,
    ETC_ICE,            // icy colours
    ETC_EARTH,          // earthy colours
    ETC_ELECTRICITY,    // electrical side of air
    ETC_AIR,            // non-electric and general air magic
    ETC_POISON,         // used for snakey things
    ETC_WATER,          // used only for the elemental
    ETC_MAGIC,          // general magical effect
    ETC_MUTAGENIC,      // transmute, poly, radiation effects
    ETC_WARP,           // teleportation and anything similar
    ETC_ENCHANT,        // magical enhancements
    ETC_HEAL,           // holy healing (not necromantic stuff)
    ETC_HOLY,           // general "good" god effects
    ETC_DARK,           // darkness
    ETC_DEATH,          // assassin/necromancy stuff
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
    ETC_FLASH,          // flashy colours
    ETC_FLOOR,          // colour of the area's floor
    ETC_ROCK,           // colour of the area's rock
    ETC_MIST,           // colour of mist
    ETC_SHIMMER_BLUE,   // shimmering colours of blue
    ETC_DECAY,          // colour of decay/swamp
    ETC_SILVER,         // colour of silver
    ETC_GOLD,           // colour of gold
    ETC_IRON,           // colour of iron
    ETC_BONE,           // colour of bone
    ETC_ELVEN_BRICK,    // colour of the walls in the Elven Halls
    ETC_WAVES,          // cyan, with regularly occurring lightcyan waves
    ETC_TREE,           // colour of trees on land
    ETC_RANDOM,         // any colour (except BLACK)
    ETC_VORTEX,         // twisting swirls of grey
    ETC_LIQUEFIED,      // ripples of yellow and brown.
    ETC_MANGROVE,       // colour of trees on water
    ETC_ORB_GLOW,       // halo coming from the Orb of Zot
    ETC_DISJUNCTION,    // halo from Disjunction
    ETC_DITHMENOS,      // Dithmenos altar colours
    ETC_ELEMENTAL,      // Cycling elemental colours
    ETC_INCARNADINE,    // Draining clouds coloured like raw flesh
    ETC_FOUL_FLAME,     // Foul flame
    ETC_MOUNTAIN,       // Mountain boots
    ETC_WU_JIAN,        // Wu Jian Chinese-inspired colours
    ETC_AWOKEN_FOREST,  // Angry trees.
    ETC_CANDLES,        // Ignis flickering candles
    ETC_STEEL,          // a brighter iron
    ETC_GLITTER,        // Klown pie decoration, all the bright colours
    ETC_DISCO = 96,
    ETC_FIRST_LUA = ETC_DISCO, // colour indices have to be <128

    NUM_COLOURS
};

typedef int (*element_colour_calculator)(int, const coord_def&);

struct base_colour_calc
{
    base_colour_calc(element_type _type, string _name)
        : type(_type), name(_name) {}
    virtual ~base_colour_calc() {}

    element_type type;
    string name;

    virtual int get(const coord_def& loc = coord_def(),
                    bool non_random = false) const = 0;
    /// If this is a sequence of colours, get the nth one (modulo), ignoring
    /// weights. Otherwise, pick a random colour for the player's position.
    virtual int get_nth(int n) const = 0;

protected:
    int rand_max {120}; // 0-119 is the range of randomness promised to
                        // Lua colour functions.
    int rand(bool non_random) const;
};


struct element_colour_calc : public base_colour_calc
{
    element_colour_calc(element_type _type, string _name,
                        element_colour_calculator _calc)
        : base_colour_calc(_type, _name), calc(_calc) {}
    virtual ~element_colour_calc() {}

    int get(const coord_def& loc = coord_def(),
            bool non_random = false) const override;
    int get_nth(int n) const override;

protected:
    element_colour_calculator calc;
};

int str_to_colour(const string &str, int default_colour = -1,
                  bool accept_number = true, bool accept_elemental = false);
const string colour_to_str(colour_t colour, bool human_readable=false);

void init_element_colours();
void add_element_colour(base_colour_calc *colour);
colour_t random_colour(bool ui_rand = false);
colour_t random_uncommon_colour();
bool is_low_colour(colour_t colour) IMMUTABLE;
bool is_high_colour(colour_t colour) IMMUTABLE;
colour_t make_low_colour(colour_t colour) IMMUTABLE;
colour_t make_high_colour(colour_t colour) IMMUTABLE;
int  element_colour(int element, bool no_random = false,
                    const coord_def& loc = coord_def());
int get_disjunct_phase(const coord_def& loc);
bool get_vortex_phase(const coord_def& loc);
bool get_orb_phase(const coord_def& loc);
int dam_colour(const monster_info&);
colour_t rune_colour(int type);

// Applies ETC_ colour substitutions
unsigned real_colour(unsigned raw_colour, const coord_def& loc = coord_def());

string colourize_str(string base, colour_t col);
