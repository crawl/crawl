#include "AppHdr.h"

#include "viewchar.h"

#include "feature.h"
#include "options.h"
#include "unicode.h"

// For order and meaning of symbols, see dungeon_char_type in enum.h.
static const ucs_t dchar_table[ NUM_CSET ][ NUM_DCHAR_TYPES ] =
{
    // CSET_DEFAULT
    // It must be limited to stuff present both in CP437 and WGL4.
    {
    //       ▓
        '#', 0x2593, '*', '.', ',', '\'', '+', '^', '>', '<',
    //            ∩       ⌠       ≈
        '#', '_', 0x2229, 0x2320, 0x2248, '8', '{',
#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)
    //  ⌂
        0x2302, // CP437 but "optional" in WGL4
#else
    //  ∆
        0x2206, // WGL4 and DEC
#endif
    //       φ
        '0', 0x03C6, ')', '[', '/', '%', '?', '=', '!', '(',
    //                                §     ♣       ©
        ':', '|', '}', '%', '$', '"', 0xA7, 0x2663, 0xA9,
    //                                     ÷
        ' ', '!', '#', '%', '+', ')', '*', 0xF7,       // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#',             // fi_stick .. explosion
    //  ═       ║       ╔       ╗       ╚       ╝
        0x2550, 0x2551, 0x2554, 0x2557, 0x255a, 0x255d,
    },
    // CSET_ASCII
    {
        '#', '#', '*', '.', ',', '\'', '+', '^', '>', '<',  // wall .. stairs up
        '#', '_', '\\', '}', '~', '8', '{', '{',       // grate .. item detect
        '{', '}', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        ':', '|', '}', '%', '$', '"', '0', '7', '^',   // book .. teleporter
        ' ', '!', '#', '%', ':', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#',             // fi_stick .. explosion
        '-', '|', '+', '+', '+', '+',
    },

    // CSET_IBM - this is ANSI 437
    {
    //  ▒       ▓       ░       ∙       ·     '     ■              // wall .. stairs up
        0x2592, 0x2593, 0x2591, 0x2219, 0xb7, '\'', 0x25a0, '^', '>', '<',
    //       ▄       ∩       ⌠       ≈
        '#', 0x2584, 0x2229, 0x2320, 0x2248, '8', '{', '{',        // grate .. item detect
    //       φ
        '0', 0x03C6, ')', '[', '/', '%', '?', '=', '!', '(',       // orb .. missile
    //  ∞       \                              ♣       Ω
        0x221e, '\\', '}', '%', '$', '"', '#', 0x2663, 0x3a9,      // book .. teleporter
        ' ', '!', '#', '%', '+', ')', '*', 0xF7,                   // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#',                         // fi_stick .. explosion
    //  ═       ║       ╔       ╗       ╚       ╝
        0x2550, 0x2551, 0x2554, 0x2557, 0x255a, 0x255d,
    },

    // CSET_DEC
    // It's better known as "vt100 line drawing characters".
    {
    //  ▒       █       ♦       ·          '     ┼     // wall .. stairs up
        0x2592, 0x2588, 0x2666, 0xb7, ':', '\'', 0x253c, '^', '>', '<',
    //       π      ¶     §     »          →       ¨
        '#', 0x3c0, 0xb6, 0xa7, 0xbb, '8', 0x2192, 0xa8,        // grate .. item detect
        '0', '}', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        ':', '\\', '}', '%', '$', '"', '#', '7', '^',  // book .. teleporter
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#',             // fi_stick .. explosion
    //  ─       │       ┌       ┐       └       ┘
        0x2500, 0x2502, 0x250c, 0x2510, 0x2514, 0x2518,
    },

    // CSET_OLD_UNICODE
    /* Beware, some popular terminals (PuTTY, xterm) are incapable of coping with
       the lack of a character in the chosen font, and most popular fonts have a
       quite limited repertoire.  A subset that is reasonably likely to be present
       is https://en.wikipedia.org/wiki/WGL4; we could provide a richer alternate
       set for those on more capable terminals under a different name.
    */
    {
    //  ▒       ▓       ░       ·     ◦       '     ◼
        0x2592, 0x2593, 0x2591, 0xB7, 0x25E6, '\'', 0x25FC, '^', '>', '<',
    //            ∩       ⌠       ≈                 ∆
        '#', '_', 0x2229, 0x2320, 0x2248, '8', '{', 0x2206,
        '0', '}', ')', '[', '/', '%', '?', '=', '!', '(',
    //  ∞                                §     ♣       ©
        0x221E, '|', '}', '%', '$', '"', 0xA7, 0x2663, 0xA9,
    //                                     ÷
        ' ', '!', '#', '%', '+', ')', '*', 0xF7,       // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#',             // fi_stick .. explosion
    //  ═       ║       ╔       ╗       ╚       ╝
        0x2550, 0x2551, 0x2554, 0x2557, 0x255a, 0x255d,
    },
};

dungeon_char_type dchar_by_name(const string &name)
{
    const char *dchar_names[] =
    {
        "wall", "permawall", "wall_magic", "floor", "floor_magic", "door_open",
        "door_closed", "trap", "stairs_down", "stairs_up",
        "grate", "altar", "arch", "fountain", "wavy", "statue",
        "invis_exposed", "item_detected",
        "item_orb", "item_rune", "item_weapon", "item_armour", "item_wand", "item_food",
        "item_scroll", "item_ring", "item_potion", "item_missile", "item_book",
        "item_stave", "item_miscellany", "item_corpse", "item_gold",
        "item_amulet", "cloud", "tree", "teleporter",
        "space", "fired_flask", "fired_bolt", "fired_chunk", "fired_book",
        "fired_weapon", "fired_zap", "fired_burst", "fired_stick",
        "fired_trinket", "fired_scroll", "fired_debug", "fired_armour",
        "fired_missile", "explosion", "frame_horiz", "frame_vert",
        "frame_top_left", "frame_top_right", "frame_bottom_left",
        "frame_bottom_right",
    };
    COMPILE_CHECK(ARRAYSZ(dchar_names) == NUM_DCHAR_TYPES);

    for (unsigned i = 0; i < ARRAYSZ(dchar_names); ++i)
        if (dchar_names[i] == name)
            return dungeon_char_type(i);

    return NUM_DCHAR_TYPES;
}

void init_char_table(char_set_type set)
{
    for (int i = 0; i < NUM_DCHAR_TYPES; i++)
    {
        ucs_t c;
        if (Options.cset_override[i])
            c = Options.cset_override[i];
        else
            c = dchar_table[set][i];
        if (wcwidth(c) != 1)
            c = dchar_table[CSET_ASCII][i];
        Options.char_table[i] = c;
    }
}

ucs_t dchar_glyph(dungeon_char_type dchar)
{
    if (dchar >= 0 && dchar < NUM_DCHAR_TYPES)
        return Options.char_table[dchar];
    else
        return 0;
}

string stringize_glyph(ucs_t glyph)
{
    char buf[5];
    buf[wctoutf8(buf, glyph)] = 0;

    return buf;
}

dungeon_char_type get_feature_dchar(dungeon_feature_type feat)
{
    return get_feature_def(feat).dchar;
}
