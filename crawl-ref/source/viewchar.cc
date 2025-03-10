#include "AppHdr.h"

#include "viewchar.h"

#include "options.h"
#include "unicode.h"
#include "tag-version.h"

// For order and meaning of symbols see dungeon_char_type in dungeon_char_type.h
static const char32_t dchar_table[NUM_CSET][NUM_DCHAR_TYPES] =
{
    // CSET_DEFAULT
    // It must be limited to stuff present both in CP437 and WGL4.
    {
        // wall .. altar
         '#', U'\x2593', //▓
            '*',  '.',  ',', '\'',  '+',  '^',  '>',  '<', '#',  '_',
        // arch .. invis_exposed
         U'\x2229', //∩
            U'\x2320', //⌠
            U'\x2248', //≈
            '~', // sadly, ∼ (U+223C, not ascii ~) and ≃ are not in WGL4
            U'\x00df',  '{',
#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)
         U'\x2302', //⌂ // CP437 but "optional" in WGL4
#else
         U'\x2206', //∆ // WGL4 and DEC
#endif
         '0', U'\x3c6', //φ
         ')',  '[',  '/',
#if TAG_MAJOR_VERSION == 34
         '%',
#endif
         '?',  '=',  '!',  '(', ':',  '|',
#if TAG_MAJOR_VERSION == 34
         '\\',
#endif
         '%', '}', U'\x2020', //%, }, †
            U'\xf7', //÷
            '$', U'\x2666', // ♦
          '"',
         U'\xa7', U'\x263c', U'\x25CB', U'\xB0', // §, ☼, ○, °
            U'\x2663', //♣
#if TAG_MAJOR_VERSION == 34
         U'\xa9', //©
#endif
        // transporter .. frame_top_left
         U'\xa9', //©
            U'\xa9', //©
            ' ',  '#',  '*', U'\xf7', //÷
            'X',  '`',  '#', U'\x2550', //═
            U'\x2551', //║
            U'\x2554', //╔
        // frame_top_right .. draw_down
         U'\x2557', //╗
            U'\x255a', //╚
            U'\x255d', //╝
            U'\x2500', //─
            U'\x2502', //│
            '/', '\\', U'\x250c', //┌
            U'\x2510', //┐
            U'\x2514', //└
            U'\x2518', //┘
            'V',
        // draw_up .. draw_left
         U'\x39b', //Λ
            '>',  '<',
    },
    // CSET_ASCII
    {
        // wall .. altar
         '#',  '#',  '*',  '.',  ',', '\'',  '+',  '^',  '>',  '<',  '#',  '_',
        // arch .. item_wand
        '\\',  '-',  '~', '~', '8',  '{',  '{',  '{',  '}',  ')',  '[',  '/',
#if TAG_MAJOR_VERSION == 34
        '%', // food
#endif
        // item_scroll .. item_staff
         '?',  '=',  '!',  '(',  ':',  '|',
#if TAG_MAJOR_VERSION == 34
         '|', // rod
#endif
        // talisman .. amulet
         '|', '}',  '%',  '%',  '$',  '$',  '"',
        // cloud .. tree
         '0', '0', '0', '0', '7',
#if TAG_MAJOR_VERSION == 34
         '^',
#endif
        // transporter .. frame_top_left
         '^',  '^',  ' ',  '#',  '*',  '+',  'X',  '`',  '#',  '-',  '|',  '+',
        // frame_top_right .. draw_down
         '+',  '+',  '+',  '-',  '|',  '/', '\\',  '*',  '*',  '*',  '*',  'V',
        // draw_up .. draw_left
         '^',  '>',  '<'
    }
};
COMPILE_CHECK(ARRAYSZ(dchar_table) == NUM_CSET);
COMPILE_CHECK(ARRAYSZ(dchar_table[0]) == NUM_DCHAR_TYPES);
COMPILE_CHECK(ARRAYSZ(dchar_table[1]) == NUM_DCHAR_TYPES);

dungeon_char_type dchar_by_name(const string &name)
{
    static const char *dchar_names[] =
    {
        "wall", "permawall", "wall_magic", "floor", "floor_magic", "door_open",
        "door_closed", "trap", "stairs_down", "stairs_up", "grate", "altar",
        "arch", "fountain", "wavy", "shallow_wavy", "statue", "invis_exposed",
        "item_detected", "item_orb", "item_rune", "item_weapon", "item_armour",
        "item_wand",
#if TAG_MAJOR_VERSION == 34
        "item_food",
#endif
        "item_scroll", "item_ring", "item_potion", "item_missile", "item_book",
        "item_staff",
#if TAG_MAJOR_VERSION == 34
        "item_rod",
#endif
        "item_talisman", "item_miscellany", "item_corpse", "item_skeleton",
        "item_gold", "item_gem", "item_amulet", "cloud", "cloud_weak",
        "cloud_fading", "cloud_terminal", "tree",
#if TAG_MAJOR_VERSION == 34
        "teleporter",
#endif
        "transporter", "transporter_landing", "space", "fired_bolt",
        "fired_zap", "fired_burst", "fired_debug", "fired_missile",
        "explosion", "frame_horiz", "frame_vert", "frame_top_left",
        "frame_top_right", "frame_bottom_left", "frame_bottom_right",
        "draw_horiz", "draw_vert", "draw_slash", "draw_backslash",
        "draw_top_left", "draw_top_right", "draw_bottom_left",
        "draw_bottom_right", "draw_down", "draw_up", "draw_right", "draw_left",
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
        char32_t c;
        if (Options.cset_override[i])
            c = Options.cset_override[i];
        else
            c = dchar_table[set][i];
        if (wcwidth(c) != 1)
            c = dchar_table[CSET_ASCII][i];
        Options.char_table[i] = c;
    }
}

char32_t dchar_glyph(dungeon_char_type dchar)
{
    if (dchar >= 0 && dchar < NUM_DCHAR_TYPES)
        return Options.char_table[dchar];
    else
        return 0;
}

string stringize_glyph(char32_t glyph)
{
    char buf[5];
    buf[wctoutf8(buf, glyph)] = 0;

    return buf;
}

dungeon_char_type get_feature_dchar(dungeon_feature_type feat)
{
    return get_feature_def(feat).dchar;
}
