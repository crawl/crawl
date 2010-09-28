#include "AppHdr.h"

#include "colour.h"

#include "dgn-height.h"
#include "env.h"
#include "libutil.h"
#include "options.h"
#include "player.h"
#include "random.h"

#include <utility>

static std::map<element_type, element_colour_calc*> element_colours;
static std::map<std::string, element_colour_calc*> element_colours_str;

typedef std::vector< std::pair<int, int> > random_colour_map;
typedef int (*randomized_element_colour_calculator)(int, const coord_def&,
                                                    random_colour_map);

static int _randomized_element_colour(int, const coord_def&, random_colour_map);

struct random_element_colour_calc : public element_colour_calc
{
    random_element_colour_calc(element_type _type, std::string _name,
                               std::vector< std::pair<int, int> > _rand_vals)
        : element_colour_calc(_type, _name, (element_colour_calculator)_randomized_element_colour),
          rand_vals(_rand_vals)
        {};

    virtual int get(const coord_def& loc = coord_def(),
                    bool non_random = false);

protected:
    random_colour_map rand_vals;
};

int element_colour_calc::rand(bool non_random)
{
    return non_random ? 0 : random2(120);
}

int element_colour_calc::get(const coord_def& loc, bool non_random)
{
    return (*calc)(rand(non_random), loc);
}

int random_element_colour_calc::get(const coord_def& loc, bool non_random)
{
    // casting function pointers from other function pointers is guaranteed
    // to be safe, but calling them on pointers not of their type isn't, so
    // assert here to be safe - add to this assert if something different is
    // needed
    ASSERT((randomized_element_colour_calculator)calc ==
                _randomized_element_colour);
    randomized_element_colour_calculator real_calc =
        (randomized_element_colour_calculator)calc;
    return (*real_calc)(rand(non_random), loc, rand_vals);
}

uint8_t random_colour(void)
{
    return (1 + random2(15));
}

uint8_t random_uncommon_colour()
{
    uint8_t result;

    do
        result = random_colour();
    while (result == LIGHTCYAN || result == CYAN || result == BROWN);

    return (result);
}

uint8_t make_low_colour(uint8_t colour)
{
    if (colour >= 8 && colour <= 15)
        return (colour - 8);

    return (colour);
}

uint8_t make_high_colour(uint8_t colour)
{
    if (colour <= 7)
        return (colour + 8);

    return (colour);
}

// returns if a colour is one of the special element colours (ie not regular)
bool is_element_colour( int col )
{
    // stripping any COLFLAGS (just in case)
    return ((col & 0x007f) >= ETC_FIRE);
}

static int _randomized_element_colour(int rand, const coord_def&,
                                      random_colour_map rand_vals)
{
    int accum = 0;
    for (random_colour_map::const_iterator it = rand_vals.begin();
         it != rand_vals.end();
         ++it) {
        if ((accum += it->first) > rand)
            return it->second;
    }

    return BLACK;
}

static int _etc_floor(int, const coord_def& loc)
{
    return element_colour(env.floor_colour, false, loc);
}

static int _etc_rock(int, const coord_def& loc)
{
    return element_colour(env.rock_colour, false, loc);
}

static int _etc_stone(int, const coord_def& loc)
{
    if (player_in_branch(BRANCH_HALL_OF_ZOT))
        return element_colour(env.rock_colour, false, loc);
    else
        return LIGHTGREY;
}

static int _etc_elven_brick(int, const coord_def& loc)
{
    if ((loc.x + loc.y) % 2)
    {
        return LIGHTGREY;
    }
    else
    {
        if ((loc.x / 2 + loc.y / 2) % 2)
            return LIGHTGREEN;
        else
            return LIGHTBLUE;
    }
}

static int _etc_waves(int, const coord_def& loc)
{
    short height = dgn_height_at(loc);
    int cycle_point = you.num_turns % 20;
    int min_height = -90 + 5 * cycle_point,
        max_height = -80 + 5 * cycle_point;
    if (height > min_height && height < max_height)
        return LIGHTCYAN;
    else
        return CYAN;
}

static int _etc_tree(int, const coord_def& loc)
{
    uint32_t h = loc.x;
    h+=h<<10; h^=h>>6;
    h += loc.y;
    h+=h<<10; h^=h>>6;
    h+=h<<3; h^=h>>11; h+=h<<15;
    return (h>>30)                        ? GREEN
         : player_in_branch(BRANCH_SWAMP) ? BROWN
         :                                  LIGHTGREEN;
}

static int _etc_random(int, const coord_def&)
{
    return random_colour();
}

static element_colour_calc *_create_random_element_colour_calc(element_type type, std::string name, ...)
{
    random_colour_map rand_vals;
    va_list ap;

    va_start(ap, name);

    for (;;)
    {
        int prob = va_arg(ap, int);
        if (!prob)
            break;

        int colour = va_arg(ap, int);

        rand_vals.push_back(std::make_pair(prob, colour));
    }

    va_end(ap);

    return new random_element_colour_calc(type, name, rand_vals);
}

void add_element_colour(element_colour_calc *colour)
{
    // or else lookups won't work: we strip high bits (because of colflags)
    ASSERT(colour->type < 128);
    ASSERT(colour->type >= ETC_FIRST_LUA ||
           element_colours.find(colour->type) == element_colours.end());
    ASSERT(colour->type >= ETC_FIRST_LUA ||
           element_colours_str.find(colour->name) == element_colours_str.end());
    element_colours[colour->type] = colour;
    element_colours_str[colour->name] = colour;
}

void init_element_colours()
{
    add_element_colour(_create_random_element_colour_calc(
                            ETC_FIRE, "fire",
                            40,  RED,
                            40,  YELLOW,
                            40,  LIGHTRED,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ICE, "ice",
                            40,  LIGHTBLUE,
                            40,  BLUE,
                            40,  WHITE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_EARTH, "earth",
                            70,  BROWN,
                            50,  GREEN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ELECTRICITY, "electricity",
                            40,  LIGHTCYAN,
                            40,  LIGHTBLUE,
                            40,  CYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_AIR, "air",
                            60,  LIGHTGREY,
                            60,  WHITE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_POISON, "poison",
                            60,  LIGHTGREEN,
                            60,  GREEN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_WATER, "water",
                            60,  BLUE,
                            60,  CYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_MAGIC, "magic",
                            30,  LIGHTMAGENTA,
                            30,  LIGHTBLUE,
                            30,  MAGENTA,
                            30,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_MUTAGENIC, "mutagenic",
                            60,  LIGHTMAGENTA,
                            60,  MAGENTA,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_WARP, "warp",
                            60,  LIGHTMAGENTA,
                            60,  MAGENTA,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ENCHANT, "enchant",
                            60,  LIGHTBLUE,
                            60,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_HEAL, "heal",
                            60,  LIGHTBLUE,
                            60,  YELLOW,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_HOLY, "holy",
                            60,  YELLOW,
                            60,  WHITE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DARK, "dark",
                            80,  DARKGREY,
                            40,  LIGHTGREY,
                        0));
    // assassin
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DEATH, "death",
                            80,  DARKGREY,
                            40,  MAGENTA,
                        0));
    // necromancer
    add_element_colour(_create_random_element_colour_calc(
                            ETC_NECRO, "necro",
                            80,  DARKGREY,
                            40,  MAGENTA,
                        0));
    // ie demonology
    add_element_colour(_create_random_element_colour_calc(
                            ETC_UNHOLY, "unholy",
                            80,  DARKGREY,
                            40,  RED,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_VEHUMET, "vehumet",
                            40,  LIGHTRED,
                            40,  LIGHTMAGENTA,
                            40,  LIGHTBLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_BEOGH, "beogh",
                            // plain Orc colour
                            60,  LIGHTRED,
                            // Orcish mines wall/idol colour
                            60,  BROWN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_CRYSTAL, "crystal",
                            40,  LIGHTGREY,
                            40,  GREEN,
                            40,  LIGHTRED,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_BLOOD, "blood",
                            60,  RED,
                            60,  DARKGREY,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_SMOKE, "smoke",
                            30,  LIGHTGREY,
                            30,  DARKGREY,
                            30,  LIGHTBLUE,
                            30,  MAGENTA,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_SLIME, "slime",
                            40,  GREEN,
                            40,  BROWN,
                            40,  LIGHTGREEN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_JEWEL, "jewel",
                            12,  WHITE,
                            12,  YELLOW,
                            12,  LIGHTMAGENTA,
                            12,  LIGHTRED,
                            12,  LIGHTGREEN,
                            12,  LIGHTBLUE,
                            12,  MAGENTA,
                            12,  RED,
                            12,  GREEN,
                            12,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ELVEN, "elven",
                            40,  LIGHTGREEN,
                            40,  GREEN,
                            20,  LIGHTBLUE,
                            20,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DWARVEN, "dwarven",
                            40,  BROWN,
                            40,  LIGHTRED,
                            20,  LIGHTGREY,
                            20,  CYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ORCISH, "orcish",
                            40,  DARKGREY,
                            40,  RED,
                            20,  BROWN,
                            20,  MAGENTA,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_GILA, "gila",
                            30,  LIGHTMAGENTA,
                            30,  MAGENTA,
                            30,  YELLOW,
                            15,  LIGHTRED,
                            15,  RED,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_KRAKEN, "kraken",
                            15,  GREEN,
                            15,  LIGHTGREEN,
                            15,  LIGHTCYAN,
                            15,  LIGHTBLUE,
                            15,  RED,
                            15,  LIGHTRED,
                            15,  MAGENTA,
                            15,  LIGHTMAGENTA,
                        0));
    add_element_colour(new element_colour_calc(
                            ETC_FLOOR, "floor", _etc_floor
                         ));
    add_element_colour(new element_colour_calc(
                            ETC_ROCK, "rock", _etc_rock
                         ));
    add_element_colour(new element_colour_calc(
                            ETC_STONE, "stone", _etc_stone
                         ));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_MIST, "mist",
                            100, CYAN,
                            20,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_SHIMMER_BLUE, "shimmer_blue",
                            90,  BLUE,
                            20,  LIGHTBLUE,
                            10,  CYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DECAY, "decay",
                            60,  BROWN,
                            60,  GREEN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_SILVER, "silver",
                            90,  LIGHTGREY,
                            30,  WHITE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_GOLD, "gold",
                            60,  YELLOW,
                            60,  BROWN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_IRON, "iron",
                            40,  CYAN,
                            40,  LIGHTGREY,
                            40,  DARKGREY,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_BONE, "bone",
                            90,  WHITE,
                            30,  LIGHTGREY,
                        0));
    add_element_colour(new element_colour_calc(
                            ETC_ELVEN_BRICK, "elven_brick", _etc_elven_brick
                         ));
    add_element_colour(new element_colour_calc(
                            ETC_WAVES, "waves", _etc_waves
                         ));
    add_element_colour(new element_colour_calc(
                            ETC_TREE, "tree", _etc_tree
                         ));
    add_element_colour(new element_colour_calc(
                            ETC_RANDOM, "random", _etc_random
                         ));
}

void clear_colours_on_exit()
{
    for (std::map<element_type, element_colour_calc*>::const_iterator it = element_colours.begin(); it != element_colours.end(); ++it)
        delete it->second;

    element_colours.clear();
    element_colours_str.clear();
}

int element_colour( int element, bool no_random, const coord_def& loc )
{
    // pass regular colours through for safety.
    if (!is_element_colour( element ))
        return (element);

    // Strip COLFLAGs just in case.
    element &= 0x007f;

    std::map<element_type, element_colour_calc*>::const_iterator colour_it
        = element_colours.find((element_type)element);
    ASSERT(colour_it != element_colours.end());
    ASSERT(colour_it->second);

    int ret = colour_it->second->get(loc, no_random);

    ASSERT(!is_element_colour(ret));

    return ((ret == BLACK) ? GREEN : ret);
}

#ifdef USE_TILE
static std::string tile_cols[24] =
{
    "black", "darkgrey", "grey", "lightgrey", "white",
    "blue", "lightblue", "darkblue",
    "green", "lightgreen", "darkgreen",
    "cyan", "lightcyan", "darkcyan",
    "red", "lightred", "darkred",
    "magenta", "lightmagenta", "darkmagenta",
    "yellow", "lightyellow", "darkyellow", "brown"
};

unsigned int str_to_tile_colour(std::string colour)
{
    if (colour.empty())
        return (0);

    lowercase(colour);

    if (colour == "darkgray")
        colour = "darkgrey";
    else if (colour == "gray")
        colour = "grey";
    else if (colour == "lightgray")
        colour = "lightgrey";

    for (unsigned int i = 0; i < 24; i++)
    {
         if (tile_cols[i] == colour)
             return (i);
    }
    return (0);
}
#endif

const std::string cols[16] =
{
    "black", "blue", "green", "cyan", "red", "magenta", "brown",
    "lightgrey", "darkgrey", "lightblue", "lightgreen", "lightcyan",
    "lightred", "lightmagenta", "yellow", "white"
};

const std::string colour_to_str(uint8_t colour)
{
    if ( colour >= 16 )
        return "lightgrey";
    else
        return cols[colour];
}

// Returns -1 if unmatched else returns 0-15.
int str_to_colour( const std::string &str, int default_colour,
                   bool accept_number )
{
    int ret;

    for (ret = 0; ret < 16; ++ret)
    {
        if (str == cols[ret])
            break;
    }

    // Check for alternate spellings.
    if (ret == 16)
    {
        if (str == "lightgray")
            ret = 7;
        else if (str == "darkgray")
            ret = 8;
    }

    if (ret == 16)
    {
        // Maybe we have an element colour attribute.
        std::map<std::string, element_colour_calc*>::const_iterator it
            = element_colours_str.find(str);
        if (it != element_colours_str.end())
        {
            ASSERT(it->second);
            ret = it->second->type;
        }
    }

    if (ret == 16 && accept_number)
    {
        // Check if we have a direct colour index.
        const char *s = str.c_str();
        char *es = NULL;
        const int ci = static_cast<int>(strtol(s, &es, 10));
        if (s != es && es && ci >= 0 && ci < 16)
            ret = ci;
    }

    return ((ret == 16) ? default_colour : ret);
}

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS) || defined(USE_TILE)
static unsigned short _dos_reverse_brand(unsigned short colour)
{
    if (Options.dos_use_background_intensity)
    {
        // If the console treats the intensity bit on background colours
        // correctly, we can do a very simple colour invert.

        // Special casery for shadows.
        if (colour == BLACK)
            colour = (DARKGREY << 4);
        else
            colour = (colour & 0xF) << 4;
    }
    else
    {
        // If we're on a console that takes its DOSness very seriously the
        // background high-intensity bit is actually a blink bit. Blinking is
        // evil, so we strip the background high-intensity bit. This, sadly,
        // limits us to 7 background colours.

        // Strip off high-intensity bit.  Special case DARKGREY, since it's the
        // high-intensity counterpart of black, and we don't want black on
        // black.
        //
        // We *could* set the foreground colour to WHITE if the background
        // intensity bit is set, but I think we've carried the
        // angry-fruit-salad theme far enough already.

        if (colour == DARKGREY)
            colour |= (LIGHTGREY << 4);
        else if (colour == BLACK)
            colour = LIGHTGREY << 4;
        else
        {
            // Zap out any existing background colour, and the high
            // intensity bit.
            colour  &= 7;

            // And swap the foreground colour over to the background
            // colour, leaving the foreground black.
            colour <<= 4;
        }
    }

    return (colour);
}

static unsigned short _dos_hilite_brand(unsigned short colour,
                                        unsigned short hilite)
{
    if (!hilite)
        return (colour);

    if (colour == hilite)
        colour = 0;

    colour |= (hilite << 4);
    return (colour);
}

unsigned short dos_brand( unsigned short colour,
                          unsigned brand)
{
    if ((brand & CHATTR_ATTRMASK) == CHATTR_NORMAL)
        return (colour);

    colour &= 0xFF;

    if ((brand & CHATTR_ATTRMASK) == CHATTR_HILITE)
        return _dos_hilite_brand(colour, (brand & CHATTR_COLMASK) >> 8);
    else
        return _dos_reverse_brand(colour);
}
#endif

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS) || defined(USE_TILE)
static unsigned _colflag2brand(int colflag)
{
    switch (colflag)
    {
    case COLFLAG_ITEM_HEAP:
        return (Options.heap_brand);
    case COLFLAG_FRIENDLY_MONSTER:
        return (Options.friend_brand);
    case COLFLAG_NEUTRAL_MONSTER:
        return (Options.neutral_brand);
    case COLFLAG_WILLSTAB:
        return (Options.stab_brand);
    case COLFLAG_MAYSTAB:
        return (Options.may_stab_brand);
    case COLFLAG_FEATURE_ITEM:
        return (Options.feature_item_brand);
    case COLFLAG_TRAP_ITEM:
        return (Options.trap_item_brand);
    default:
        return (CHATTR_NORMAL);
    }
}
#endif

unsigned real_colour(unsigned raw_colour, const coord_def& loc)
{
    // This order is important - is_element_colour() doesn't want to see the
    // munged colours returned by dos_brand, so it should always be done
    // before applying DOS brands.
    const int colflags = raw_colour & 0xFF00;

    // Evaluate any elemental colours to guarantee vanilla colour is returned
    if (is_element_colour( raw_colour ))
        raw_colour = colflags | element_colour( raw_colour, false, loc );

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS) || defined(USE_TILE)
    if (colflags)
    {
        unsigned brand = _colflag2brand(colflags);
        raw_colour = dos_brand(raw_colour & 0xFF, brand);
    }
#endif

#ifndef USE_COLOUR_OPTS
    // Strip COLFLAGs for systems that can't do anything meaningful with them.
    raw_colour &= 0xFF;
#endif

    return (raw_colour);
}
