#include "AppHdr.h"

#include "colour.h"

#include "env.h"
#include "libutil.h"
#include "options.h"
#include "player.h"
#include "random.h"

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

static int _etc_elven_brick(const coord_def& loc)
{
    if ((loc.x + loc.y) % 2)
        return LIGHTGREEN;
    else
        return LIGHTBLUE;
}

int element_colour( int element, bool no_random, const coord_def& loc )
{
    // Doing this so that we don't have to do recursion here at all
    // (these were the only cases which had possible double evaluation):
    if (element == ETC_FLOOR)
        element = env.floor_colour;
    else if (element == ETC_ROCK)
        element = env.rock_colour;

    // pass regular colours through for safety.
    if (!is_element_colour( element ))
        return (element);

    int ret = BLACK;

    // Setting no_random to true will get the first colour in the cases
    // below.  This is potentially useful for calls to this function
    // which might want a consistent result.
    int tmp_rand = (no_random ? 0 : random2(120));

    // Strip COLFLAGs just in case.
    element &= 0x007f;

    switch (element)
    {
    case ETC_FIRE:
        ret = (tmp_rand < 40) ? RED :
              (tmp_rand < 80) ? YELLOW
                              : LIGHTRED;
        break;

    case ETC_ICE:
        ret = (tmp_rand < 40) ? LIGHTBLUE :
              (tmp_rand < 80) ? BLUE
                              : WHITE;
        break;

    case ETC_EARTH:
        ret = (tmp_rand < 70) ? BROWN : GREEN;
        break;

    case ETC_AIR:
        ret = (tmp_rand < 60) ? LIGHTGREY : WHITE;
        break;

    case ETC_ELECTRICITY:
        ret = (tmp_rand < 40) ? LIGHTCYAN :
              (tmp_rand < 80) ? LIGHTBLUE
                              : CYAN;
        break;

    case ETC_POISON:
        ret = (tmp_rand < 60) ? LIGHTGREEN : GREEN;
        break;

    case ETC_WATER:
        ret = (tmp_rand < 60) ? BLUE : CYAN;
        break;

    case ETC_MAGIC:
        ret = (tmp_rand < 30) ? LIGHTMAGENTA :
              (tmp_rand < 60) ? LIGHTBLUE :
              (tmp_rand < 90) ? MAGENTA
                              : BLUE;
        break;

    case ETC_MUTAGENIC:
    case ETC_WARP:
        ret = (tmp_rand < 60) ? LIGHTMAGENTA : MAGENTA;
        break;

    case ETC_ENCHANT:
        ret = (tmp_rand < 60) ? LIGHTBLUE : BLUE;
        break;

    case ETC_HEAL:
        ret = (tmp_rand < 60) ? LIGHTBLUE : YELLOW;
        break;

    case ETC_BLOOD:
        ret = (tmp_rand < 60) ? RED : DARKGREY;
        break;

    case ETC_DEATH:      // assassin
    case ETC_NECRO:      // necromancer
        ret = (tmp_rand < 80) ? DARKGREY : MAGENTA;
        break;

    case ETC_UNHOLY:     // ie demonology
        ret = (tmp_rand < 80) ? DARKGREY : RED;
        break;

    case ETC_DARK:
        ret = (tmp_rand < 80) ? DARKGREY : LIGHTGREY;
        break;

    case ETC_HOLY:
        ret = (tmp_rand < 60) ? YELLOW : WHITE;
        break;

    case ETC_VEHUMET:
        ret = (tmp_rand < 40) ? LIGHTRED :
              (tmp_rand < 80) ? LIGHTMAGENTA
                              : LIGHTBLUE;
        break;

    case ETC_BEOGH:
        ret = (tmp_rand < 60) ? LIGHTRED // plain Orc colour
                              : BROWN;   // Orcish mines wall/idol colour
        break;

    case ETC_CRYSTAL:
        ret = (tmp_rand < 40) ? LIGHTGREY :
              (tmp_rand < 80) ? GREEN
                              : LIGHTRED;
        break;

    case ETC_SLIME:
        ret = (tmp_rand < 40) ? GREEN :
              (tmp_rand < 80) ? BROWN
                              : LIGHTGREEN;
        break;

    case ETC_SMOKE:
        ret = (tmp_rand < 30) ? LIGHTGREY :
              (tmp_rand < 60) ? DARKGREY :
              (tmp_rand < 90) ? LIGHTBLUE
                              : MAGENTA;
        break;

    case ETC_JEWEL:
        ret = (tmp_rand <  12) ? WHITE :
              (tmp_rand <  24) ? YELLOW :
              (tmp_rand <  36) ? LIGHTMAGENTA :
              (tmp_rand <  48) ? LIGHTRED :
              (tmp_rand <  60) ? LIGHTGREEN :
              (tmp_rand <  72) ? LIGHTBLUE :
              (tmp_rand <  84) ? MAGENTA :
              (tmp_rand <  96) ? RED :
              (tmp_rand < 108) ? GREEN
                               : BLUE;
        break;

    case ETC_ELVEN:
        ret = (tmp_rand <  40) ? LIGHTGREEN :
              (tmp_rand <  80) ? GREEN :
              (tmp_rand < 100) ? LIGHTBLUE
                               : BLUE;
        break;

    case ETC_ELVEN_BRICK:
        ret = _etc_elven_brick(loc);
        break;

    case ETC_DWARVEN:
        ret = (tmp_rand <  40) ? BROWN :
              (tmp_rand <  80) ? LIGHTRED :
              (tmp_rand < 100) ? LIGHTGREY
                               : CYAN;
        break;

    case ETC_ORCISH:
        ret = (tmp_rand <  40) ? DARKGREY :
              (tmp_rand <  80) ? RED :
              (tmp_rand < 100) ? BROWN
                               : MAGENTA;
        break;

    case ETC_GILA:
        ret = (tmp_rand <  30) ? LIGHTMAGENTA :
              (tmp_rand <  60) ? MAGENTA :
              (tmp_rand <  90) ? YELLOW :
              (tmp_rand < 105) ? LIGHTRED
                               : RED;
        break;

    case ETC_KRAKEN:
        ret = (tmp_rand <  15) ? GREEN :
              (tmp_rand <  30) ? LIGHTGREEN :
              (tmp_rand <  45) ? LIGHTCYAN :
              (tmp_rand <  60) ? LIGHTBLUE :
              (tmp_rand <  75) ? RED :
              (tmp_rand <  90) ? LIGHTRED :
              (tmp_rand < 105) ? MAGENTA
                               : LIGHTMAGENTA;
        break;

    case ETC_STONE:
        if (player_in_branch(BRANCH_HALL_OF_ZOT))
            ret = env.rock_colour;
        else
            ret = LIGHTGREY;
        break;

    case ETC_MIST:
        ret = (tmp_rand < 100) ? CYAN : BLUE;
        break;

    case ETC_SHIMMER_BLUE:
        ret = (tmp_rand <  90) ? BLUE :
              (tmp_rand < 110) ? LIGHTBLUE
                               : CYAN;
        break;

    case ETC_DECAY:
        ret = (tmp_rand < 60) ? BROWN : GREEN;
        break;

    case ETC_SILVER:
        ret = (tmp_rand < 90) ? LIGHTGREY : WHITE;
        break;

    case ETC_GOLD:
        ret = (tmp_rand < 60) ? YELLOW : BROWN;
        break;

    case ETC_IRON:
        ret = (tmp_rand < 40) ? CYAN :
              (tmp_rand < 80) ? LIGHTGREY :
                                DARKGREY;
        break;

    case ETC_BONE:
        ret = (tmp_rand < 90) ? WHITE : LIGHTGREY;
        break;

    case ETC_RANDOM:
        ret = random_colour();              // always random
        break;

    case ETC_FLOOR: // should already be handled
    case ETC_ROCK:  // should already be handled
    default:
        break;
    }

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

    static const std::string element_cols[] =
    {
        "fire", "ice", "earth", "electricity", "air", "poison", "water",
        "magic", "mutagenic", "warp", "enchant", "heal", "holy", "dark",
        "death", "necro", "unholy", "vehumet", "beogh", "crystal",
        "blood", "smoke", "slime", "jewel", "elven", "dwarven",
        "orcish", "gila", "kraken", "floor", "rock", "stone", "mist",
        "shimmer_blue", "decay", "silver", "gold", "iron", "bone", "elven_brick",
        "random"
    };

    ASSERT(ARRAYSZ(element_cols) == (ETC_RANDOM - ETC_FIRE) + 1);
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
        for (unsigned i = 0; i < sizeof(element_cols) / sizeof(*element_cols);
                ++i)
        {
            if (str == element_cols[i])
            {
                // Ugh.
                ret = element_type(ETC_FIRE + i);
                break;
            }
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
