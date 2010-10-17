#include "AppHdr.h"

#include "viewchar.h"

#include "feature.h"
#include "options.h"
#include "state.h"

// For order and meaning of symbols, see dungeon_char_type in enum.h.
static const unsigned dchar_table[ NUM_CSET ][ NUM_DCHAR_TYPES ] =
{
    // CSET_ASCII
    {
        '#', '*', '.', ',', '\'', '+', '^', '>', '<',  // wall .. stairs up
        '#', '_', '\\', '}', '~', '8', '{', '{',       // grate .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        ':', '|', '}', '%', '$', '"', '#', '7',        // book .. tree
        ' ', '!', '#', '%', ':', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_IBM - this is ANSI 437
    {
        177, 176, 249, 250, '\'', 254, '^', '>', '<',  // wall .. stairs up
        '#', 220, 239, 244, 247, '8', '{', '{',        // grate .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        236, '\\', '}', '%', '$', '"', '#', 234,       // book .. tree
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_DEC - remember: 224-255 are mapped to shifted 96-127
    // It's better known as "vt100 line drawing characters".
    {
        225, 224, 254, ':', '\'', 238, '^', '>', '<',  // wall .. stairs up
        '#', 251, 182, 167, 187, '8', 171, 168,        // grate .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        ':', '\\', '}', '%', '$', '"', '#', '7',       // book .. tree
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_UNICODE
    /* Beware, some popular terminals (PuTTY, xterm) are incapable of coping with
       the lack of a character in the chosen font, and most popular fonts have a
       quite limited repertoire.  A subset that is reasonably likely to be present
       is http://en.wikipedia.org/wiki/WGL4; we could provide a richer alternate
       set for those on more capable terminals (including for example Thai 0xEB0
       for clouds), but that would require decoupling encoding from charset.
    */
    {
        0x2592, 0x2591, 0xB7, 0x25E6, '\'', 0x25FC, '^', '>', '<',
        '#', '_', 0x2229, 0x2320, 0x2248, '8', '{', 0x2206,
        '0', ')', '[', '/', '%', '?', '=', '!', '(',
        0x221E, '|', '}', '%', '$', '"', '#', 0x2663,
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },
};

dungeon_char_type dchar_by_name(const std::string &name)
{
    const char *dchar_names[] =
    {
        "wall", "wall_magic", "floor", "floor_magic", "door_open",
        "door_closed", "trap", "stairs_down", "stairs_up",
        "grate", "altar", "arch", "fountain", "wavy", "statue",
        "invis_exposed", "item_detected",
        "item_orb", "item_weapon", "item_armour", "item_wand", "item_food",
        "item_scroll", "item_ring", "item_potion", "item_missile", "item_book",
        "item_stave", "item_miscellany", "item_corpse", "item_gold",
        "item_amulet", "cloud", "tree",
    };

    for (unsigned i = 0; i < sizeof(dchar_names) / sizeof(*dchar_names); ++i)
        if (dchar_names[i] == name)
            return dungeon_char_type(i);

    return (NUM_DCHAR_TYPES);
}

void init_char_table(char_set_type set)
{
    for (int i = 0; i < NUM_DCHAR_TYPES; i++)
    {
        if (Options.cset_override[set][i])
            Options.char_table[i] = Options.cset_override[set][i];
        else
            Options.char_table[i] = dchar_table[set][i];
    }
}

unsigned dchar_glyph(dungeon_char_type dchar)
{
    if (dchar >= 0 && dchar < NUM_DCHAR_TYPES)
        return (Options.char_table[dchar]);
    else
        return (0);
}

std::string stringize_glyph(unsigned glyph)
{
    if (crawl_state.glyph2strfn && Options.char_set == CSET_UNICODE)
        return (*crawl_state.glyph2strfn)(glyph);

    return (std::string(1, glyph));
}

int multibyte_strlen(const std::string &s)
{
    if (crawl_state.multibyte_strlen)
        return (*crawl_state.multibyte_strlen)(s);

    return (s.length());
}

dungeon_char_type get_feature_dchar(dungeon_feature_type feat)
{
    return (get_feature_def(feat).dchar);
}
