/*
 *  File:       command.cc
 *  Summary:    Misc commands.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <4>     10/12/99        BCR             BUILD_DATE is now used in version()
 *      <3>     6/13/99         BWR             New equipment listing commands
 *      <2>     5/20/99         BWR             Swapping inventory letters.
 *      <1>     -/--/--         LRH             Created
 */

#include "AppHdr.h"
#include "command.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "externs.h"

#include "abl-show.h"
#include "invent.h"
#include "itemname.h"
#include "item_use.h"
#include "items.h"
#include "libutil.h"
#include "menu.h"
#include "ouch.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "version.h"
#include "view.h"

static void adjust_item(void);
static void adjust_spells(void);
static void adjust_ability(void);
static void list_wizard_commands();
#ifdef OBSOLETE_COMMAND_HELP
static const char *command_string( int i );
#endif
static const char *wizard_string( int i );

void quit_game(void)
{
    if (yesno("Really quit?", false, 'n'))
        ouch(-9999, 0, KILLED_BY_QUITTING);
}                               // end quit_game()

static const char *features[] = {
    "",
    "Stash-tracking",

#ifdef CLUA_BINDINGS
    "Lua",
#endif

#if defined(REGEX_POSIX)
    "POSIX regexps",
#elif defined(REGEX_PCRE)
    "PCRE regexps",
#else
    "Glob patterns",
#endif
};

void version(void)
{
    mpr( "This is " CRAWL " " VERSION " (" BUILD_DATE ")." );
    
    std::string feats = "Features: ";
    for (int i = 1, size = sizeof features / sizeof *features; i < size; ++i)
    {
        if (i > 1)
            feats += ", ";
        feats += features[i];
    }
    mpr(feats.c_str());
}                               // end version()

void adjust(void)
{
    mpr( "Adjust (i)tems, (s)pells, or (a)bilities?", MSGCH_PROMPT );

    unsigned char keyin = tolower( get_ch() );

    if (keyin == 'i')
        adjust_item();
    else if (keyin == 's')
        adjust_spells();
    else if (keyin == 'a')
        adjust_ability();
    else if (keyin == ESCAPE)
        canned_msg( MSG_OK );
    else
        canned_msg( MSG_HUH );
}                               // end adjust()

void swap_inv_slots(int from_slot, int to_slot, bool verbose)
{
    // swap items
    item_def tmp = you.inv[to_slot];
    you.inv[to_slot] = you.inv[from_slot];
    you.inv[from_slot] = tmp;

    you.inv[from_slot].link = from_slot;
    you.inv[to_slot].link = to_slot;

    for (int i = 0; i < NUM_EQUIP; i++)
    {
        if (you.equip[i] == from_slot)
            you.equip[i] = to_slot;
        else if (you.equip[i] == to_slot)
            you.equip[i] = from_slot;
    }

    if (verbose)
    {
        char str_pass[ ITEMNAME_SIZE ];
        in_name( to_slot, DESC_INVENTORY_EQUIP, str_pass );
        mpr( str_pass );

        if (is_valid_item( you.inv[from_slot] ))
        {
            in_name( from_slot, DESC_INVENTORY_EQUIP, str_pass );
            mpr( str_pass );
        }
    }

    if (to_slot == you.equip[EQ_WEAPON] || from_slot == you.equip[EQ_WEAPON])
        you.wield_change = true;
}

static void adjust_item(void)
{
    int from_slot, to_slot;
    char str_pass[ ITEMNAME_SIZE ];

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    from_slot = prompt_invent_item( "Adjust which item?", MT_INVSELECT, -1 );
    if (from_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    in_name( from_slot, DESC_INVENTORY_EQUIP, str_pass );
    mpr( str_pass );

    to_slot = prompt_invent_item(
                    "Adjust to which letter?", 
                    MT_INVSELECT, 
                    -1, 
                    false, 
                    false );
    if (to_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    swap_inv_slots(from_slot, to_slot, true);
}                               // end adjust_item()

static void adjust_spells_cleanup(bool needs_redraw)
{
    if (needs_redraw)
        redraw_screen();
}

static void adjust_spells(void)
{
    unsigned char index_1, index_2;
    unsigned char nthing = 0;

    bool needs_redraw = false;

    if (!you.spell_no)
    {
        mpr("You don't know any spells.");
        return;
    }

  query:
    mpr("Adjust which spell?", MSGCH_PROMPT);

    unsigned char keyin = get_ch();

    if (keyin == '?' || keyin == '*')
    {
        if (keyin == '*' || keyin == '?')
        {
            nthing = list_spells();
            needs_redraw = true;
        }

        if (isalpha( nthing ) || nthing == ESCAPE)
            keyin = nthing;
        else
        {
            mesclr( true );
            goto query;
        }
    }

    if (keyin == ESCAPE)
    {
        adjust_spells_cleanup(needs_redraw);
        canned_msg( MSG_OK );
        return;
    }

    int input_1 = (int) keyin;

    if (!isalpha( input_1 ))
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("You don't know that spell.");
        return;
    }

    index_1 = letter_to_index( input_1 );
    int spell = get_spell_by_letter( input_1 ); 

    if (spell == SPELL_NO_SPELL)
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("You don't know that spell.");
        return;
    }

    // print out targeted spell:
    snprintf( info, INFO_SIZE, "%c - %s", input_1, spell_title( spell ) );
    mpr(info);

    mpr( "Adjust to which letter?", MSGCH_PROMPT );

    keyin = get_ch();

    if (keyin == '?' || keyin == '*')
    {
        if (keyin == '*' || keyin == '?')
        {
            nthing = list_spells();
            needs_redraw = true;
        }

        if (isalpha( nthing ) || nthing == ESCAPE)
            keyin = nthing;
        else
        {
            mesclr( true );
            goto query;
        }
    }

    if (keyin == ESCAPE)
    {
        adjust_spells_cleanup(needs_redraw);
        canned_msg( MSG_OK );
        return;
    }

    int input_2 = (int) keyin;

    if (!isalpha( input_2 ))
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("What?");
        return;
    }

    adjust_spells_cleanup(needs_redraw);

    index_2 = letter_to_index( input_2 );

    // swap references in the letter table:
    int tmp = you.spell_letter_table[index_2];
    you.spell_letter_table[index_2] = you.spell_letter_table[index_1];
    you.spell_letter_table[index_1] = tmp;

    // print out spell in new slot (now at input_2)
    snprintf( info, INFO_SIZE, "%c - %s", input_2, 
              spell_title( get_spell_by_letter(input_2) ) );

    mpr(info);

    // print out other spell if one was involved (now at input_1)
    spell = get_spell_by_letter( input_1 );
    if (spell != SPELL_NO_SPELL)
    {
        snprintf( info, INFO_SIZE, "%c - %s", input_1, spell_title(spell) );
        mpr(info);
    }
}                               // end adjust_spells()

static void adjust_ability(void)
{
    unsigned char index_1, index_2;
    unsigned char nthing = 0;

    bool needs_redraw = false;

    if (!generate_abilities())
    {
        mpr( "You don't currently have any abilities." );
        return;
    }

  query:
    mpr( "Adjust which ability?", MSGCH_PROMPT );

    unsigned char keyin = get_ch();

    if (keyin == '?' || keyin == '*')
    {
        if (keyin == '*' || keyin == '?')
        {
            nthing = show_abilities();
            needs_redraw = true;
        }

        if (isalpha( nthing ) || nthing == ESCAPE)
            keyin = nthing;
        else
        {
            mesclr( true );
            goto query;
        }
    }

    if (keyin == ESCAPE)
    {
        adjust_spells_cleanup(needs_redraw);
        canned_msg( MSG_OK );
        return;
    }

    int input_1 = (int) keyin;

    if (!isalpha( input_1 ))
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("You don't have that ability.");
        return;
    }

    index_1 = letter_to_index( input_1 );

    if (you.ability_letter_table[index_1] == ABIL_NON_ABILITY)
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("You don't have that ability.");
        return;
    }

    // print out targeted spell:
    snprintf( info, INFO_SIZE, "%c - %s", input_1, 
              get_ability_name_by_index( index_1 ) );

    mpr(info);

    mpr( "Adjust to which letter?", MSGCH_PROMPT );

    keyin = get_ch();

    if (keyin == '?' || keyin == '*')
    {
        if (keyin == '*' || keyin == '?')
        {
            nthing = show_abilities();
            needs_redraw = true;
        }

        if (isalpha( nthing ) || nthing == ESCAPE)
            keyin = nthing;
        else
        {
            mesclr( true );
            goto query;
        }
    }

    if (keyin == ESCAPE)
    {
        adjust_spells_cleanup(needs_redraw);
        canned_msg( MSG_OK );
        return;
    }

    int input_2 = (int) keyin;

    if (!isalpha( input_2 ))
    {
        adjust_spells_cleanup(needs_redraw);
        mpr("What?");
        return;
    }

    adjust_spells_cleanup(needs_redraw);

    index_2 = letter_to_index( input_2 );

    // swap references in the letter table:
    int tmp = you.ability_letter_table[index_2];
    you.ability_letter_table[index_2] = you.ability_letter_table[index_1];
    you.ability_letter_table[index_1] = tmp;

    // Note:  the input_2/index_1 and input_1/index_2 here is intentional.
    // This is because nothing actually moves until generate_abilities is
    // called again... fortunately that has to be done everytime because
    // that's the silly way this system currently works.  -- bwr
    snprintf( info, INFO_SIZE, "%c - %s", input_2, 
              get_ability_name_by_index( index_1 ) );

    mpr(info);

    if (you.ability_letter_table[index_1] != ABIL_NON_ABILITY)
    {
        snprintf( info, INFO_SIZE, "%c - %s", input_1, 
                  get_ability_name_by_index( index_2 ) );
        mpr(info);
    }
}                               // end adjust_ability()

void list_armour(void)
{
    for (int i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++)
    {
        int armour_id = you.equip[i];

        strcpy( info,
                (i == EQ_CLOAK)       ? "Cloak  " :
                (i == EQ_HELMET)      ? "Helmet " :
                (i == EQ_GLOVES)      ? "Gloves " :
                (i == EQ_SHIELD)      ? "Shield " :
                (i == EQ_BODY_ARMOUR) ? "Armour " :

                (i == EQ_BOOTS
                    && !(you.species == SP_CENTAUR || you.species == SP_NAGA))
                                      ? "Boots  " :
                (i == EQ_BOOTS
                    && (you.species == SP_CENTAUR || you.species == SP_NAGA))
                                      ? "Barding"
                                      : "unknown" );

        strcat(info, " : ");

        if (armour_id != -1)
        {
            char str_pass[ ITEMNAME_SIZE ];
            in_name(armour_id, DESC_INVENTORY, str_pass);
            strcat(info, str_pass);
        }
        else
        {
            strcat(info, "    none");
        }

        mpr( info, MSGCH_EQUIPMENT, menu_colour(info) );
    }
}                               // end list_armour()

void list_jewellery(void)
{
    for (int i = EQ_LEFT_RING; i <= EQ_AMULET; i++)
    {
        int jewellery_id = you.equip[i];

        strcpy( info, (i == EQ_LEFT_RING)  ? "Left ring " :
                      (i == EQ_RIGHT_RING) ? "Right ring" :
                      (i == EQ_AMULET)     ? "Amulet    "
                                           : "unknown   " );

        strcat(info, " : ");

        if (jewellery_id != -1)
        {
            char str_pass[ ITEMNAME_SIZE ];
            in_name(jewellery_id, DESC_INVENTORY, str_pass);
            strcat(info, str_pass);
        }
        else
        {
            strcat(info, "    none");
        }

        mpr( info, MSGCH_EQUIPMENT, menu_colour(info) );
    }
}                               // end list_jewellery()

void list_weapons(void)
{
    const int weapon_id = you.equip[EQ_WEAPON];

    // Output the current weapon
    //
    // Yes, this is already on the screen... I'm outputing it
    // for completeness and to avoid confusion.
    strcpy(info, "Current   : ");

    if (weapon_id != -1)
    {
        char str_pass[ ITEMNAME_SIZE ];
        in_name( weapon_id, DESC_INVENTORY_EQUIP, str_pass );
        strcat(info, str_pass);
    }
    else
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            strcat(info, "    blade hands");
        else
            strcat(info, "    empty hands");
    }

    mpr(info, MSGCH_EQUIPMENT, menu_colour(info));

    // Print out the swap slots
    for (int i = 0; i <= 1; i++)
    {
        // We'll avoid repeating the current weapon for these slots,
        // in order to keep things clean.
        if (weapon_id == i)
            continue;

        strcpy(info, (i == 0) ? "Primary   : " : "Secondary : ");

        if (is_valid_item( you.inv[i] ))
        {
            char str_pass[ ITEMNAME_SIZE ];
            in_name(i, DESC_INVENTORY_EQUIP, str_pass);
            strcat(info, str_pass);
        }
        else
            strcat(info, "    none");

        mpr(info, MSGCH_EQUIPMENT, menu_colour(info));   // Output slot
    }

    // Now we print out the current default fire weapon
    strcpy(info, "Firing    : ");

    const int item = get_fire_item_index();

    if (item == ENDOFPACK)
        strcat(info, "    nothing");
    else
    {
        char str_pass[ ITEMNAME_SIZE ];
        in_name( item, DESC_INVENTORY_EQUIP, str_pass );
        strcat( info, str_pass );
    }

    mpr( info, MSGCH_EQUIPMENT, menu_colour(info) );
}                               // end list_weapons()

static int cmdhelp_keyfilter(int keyin)
{
    switch (keyin)
    {
    case CK_DOWN:
    case '+':
    case '=':
        return ('>');
    case CK_UP:
    case '-':
    case '_':
        return ('<');
    case 'x':
        return (CK_ESCAPE);
    default:
        return (keyin);
    }
}

static bool cmdhelp_textfilter(const std::string &tag)
{
    if (tag == "s")
        return (true);
#ifdef WIZARD
    if (tag == "wiz")
        return (true);
#endif
    return (false);
}

static const char *level_map_help = 
        "<h>Level Map ('<w>X</w><h>' in main screen):\n"
        "<w>Esc</w> : leave level map (also Space)\n"
        "<w>Dir.</w>: move cursor\n"
        "<w>/ Dir.</w>, <w>Shift-Dir.</w>: move cursor far\n"
        "<w>-</w>/<w>+</w> : scroll level map up/down\n"
        "<w>.</w>   : travel (also <w>Enter</w> and <w>,</w> and <w>;</w>)\n"
        "       (moves cursor to last travel\n"
        "       destination if still on @)\n"
        "<w><<</w>/<w>></w> : cycle through up/down stairs\n"
        "<w>^</w>   : cycle through traps\n"
        "<w>Tab</w> : cycle through shops and portals\n"
        "<w>X</w>   : cycle through travel eXclusions\n"
        "<w>W</w>   : cycle through waypoints\n"
        "<w>*</w>   : cycle forward through stashes\n"
        "<w>/</w>   : cycle backward through stashes\n"
        "<w>Ctrl-X</w> : set travel eXclusion\n"
        "<w>Ctrl-E</w> : Erase all travel exclusions\n"
        "<w>Ctrl-W</w> : set Waypoint\n"
        "<w>Ctrl-C</w> : Clear level and main maps\n";

static const char *targeting_help =         
        "<h>Targeting (like zapping wands/spells):\n"
        "<w>Esc</w>  : stop targeting\n"
        "<w>Dir.</w> : shoots straight in that direction\n"
        "<w>p</w> : fires at Previous target (also <w>t</w>)\n"
        "<w>+</w> : cycle monsters forward (also <w>=</w>)\n"
        "<w>-</w> : cycle monsters backward\n"
	"<w>.</w> : fires at target (also <w>Enter</w> or <w>Del</w>)\n"
	"<w>></w> : fires at target and stops there\n"
        "<w>Ctrl-F</w> : toggles cycle mode\n"
        "<w>*</w> : enter manual targeting (where <w>Dir.</w>\n"
        "      moves the cursor and <w>.</w> etc. fire)\n"
        "<w>?</w> : describes monster under cursor\n";
            
static void show_keyhelp_menu(const std::vector<formatted_string> &lines)
{
    formatted_scroller cmd_help;
    
    // Set flags, and don't use easy exit.
    cmd_help.set_flags(MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP, false);

    // FIXME: Allow for hiding Page down when at the end of the listing, ditto
    // for page up at start of listing.
    cmd_help.set_more(
            formatted_string::parse_string(
                "<cyan>[ + : Page down.   - : Page up."
                "                         Esc/x exits.]"));
    cmd_help.f_keyfilter = cmdhelp_keyfilter;

    for (unsigned i = 0; i < lines.size(); ++i )
        cmd_help.add_item_formatted_string(lines[i]);
    cmd_help.show();
}

void show_levelmap_help()
{
    std::vector<std::string> lines =
        split_string("\n", level_map_help, false, true);
    std::vector<formatted_string> formatted_lines;
    for (int i = 0, size = lines.size(); i < size; ++i)
        formatted_lines.push_back(
                formatted_string::parse_string(
                    lines[i], true, cmdhelp_textfilter));
    show_keyhelp_menu(formatted_lines);
}

void list_commands(bool wizard)
{
    if (wizard)
    {
        list_wizard_commands();
        return;
    }
        
    // 2 columns, split at column 40.
    column_composer cols(2, 41);
    // Page size is number of lines - one line for --more-- prompt.
    cols.set_pagesize(get_number_of_lines() - 1);

    cols.add_formatted(
            0,
            "<h>Movement:\n"
            "To move in a direction or to attack, use\n"
            "the numpad (try Numlock both off and on)\n"
            "or vi keys:\n"
            "              <w>1 2 3      y k u\n"
            "               \\|/        \\|/\n"
            "              <w>4</w>-5-<w>6</w>"
                              "      <w>h</w>-.-<w>l</w>\n"
            "               /|\\        /|\\\n"
            "              <w>7 8 9      b j n\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Rest/Search:\n"
            "<w>s</w> : rest one turn and search adjacent\n"
            "    squares (also <w>numpad-5</w>, <w>.</w>, <w>Del</w>)\n"
            "<w>5</w> : fully rest HP/MP or search; stop after \n"
            "    100 turns (also <w>Shift-numpad-5</w>)\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Extended Movement:\n"
            "<w>/ Dir.</w>, <w>Shift-Dir.</w>: long walk\n"
            "<w>* Dir.</w>, <w>Ctrl-Dir.</w> : untrap, attack\n"
            "           without move, open door\n"
            "<w>Ctrl-G</w> : interlevel travel\n"
            "<w>Ctrl-O</w> : auto-explore\n"
            "<w>Ctrl-W</w> : set Waypoint\n"
            "<w>Ctrl-F</w> : Find items\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>Item Interaction (inventory):\n"
            "<w>i</w> : show Inventory list\n"
            "<w>v</w> : View item description\n"
            "<w>{</w> : inscribe item\n"
            "<w>t</w> : Throw/shoot an item\n"
            "<w>f</w> : Fire first available missile\n"
            "<w>e</w> : Eat food (but tries floor first)\n"
            "<w>q</w> : Quaff a potion\n"
            "<w>z</w> : Zap a wand\n"
            "<w>r</w> : Read a scroll or book\n"
            "<w>M</w> : Memorise a spell from a book\n"
            "<w>w</w> : Wield an item ( <w>-</w> for none)\n"
            "<w>'</w> : wield item a, or switch to b\n"
            "<w>E</w> : Evoke power of wielded item\n"
            "<w>W</w> : Wear armour\n"
            "<w>T</w> : Take off armour\n"
            "<w>P</w> : Put on jewellery\n"
            "<w>R</w> : Remove jewellery\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            "<h>In-game Toggles:\n"
            "<w>Ctrl-A</w> : toggle Autopickup\n"
            "<w>Ctrl-V</w> : toggle auto-prayer\n"
//            "<w>Ctrl-T</w> : toggle spell fizzle check\n"
            " \n"
            " \n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
            level_map_help,
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            0,
//            "\p" 
           // I want a page break here, but perhaps there are 
           // less violent ways to enforce it.
            " \n \n \n \n"
            "<h>Item types (and common commands)\n"
            "<cyan>)</cyan> : hand weapons (<w>w</w>ield)\n"
            "<brown>(</brown> : missiles (<w>t</w>hrow or <w>f</w>ire)\n"
            "<cyan>[</cyan> : armour (<w>W</w>ear and <w>T</w>ake off)\n"
            "<brown>%</brown> : food and corpses (<w>e</w> and <w>D</w>issect)\n"
            "<w>?</w> : scrolls (<w>r</w>ead)\n"
            "<magenta>!</magenta> : potions (<w>q</w>uaff)\n"
            "<blue>=</blue> : rings (<w>P</w>ut on and <w>R</w>emove)\n"
            "<red>\"</red> : amulets (<w>P</w>ut on and <w>R</w>emove)\n"
            "<lightgrey>/</lightgrey> : wands (<w>z</w>ap)\n"            
            "<lightcyan>+</lightcyan> : books (<w>r</w>ead, <w>M</w>emorise and <w>Z</w>ap)\n"   
            "<brown>\\</brown> : staves and rods (<w>w</w>ield and <w>E</w>voke)\n"
            "<lightgreen>{</lightgreen> : miscellaneous items (<w>E</w>voke)\n"
            "<lightred>0</lightred> : the Orb of Zot\n"
            "\n"
            "<yellow>$</yellow> : gold\n"
            " \n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(            
            0,
            "<?s><h>Stash Management Commands:\n"
            "<?s><w>Ctrl-F</w> : Find (in stashes and shops)\n"
            "<?s>\n"
            "<?s>Searching in stashes allows regular\n"
            "<?s>expressions, and terms like 'altar'\n"
            "<?s>or 'artifact' or 'long blades'.\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Game Saving and Quitting:\n"
            "<w>S</w> : Save game and exit \n"
            "<w>Q</w> : Quit without saving\n"
            "<w>Ctrl-X</w> : save game without query\n",
            true, true, cmdhelp_textfilter, 45);

    cols.add_formatted(
            1,
            "<h>Player Character Information:\n"
            "<w>@</w> : display character status\n"
            "<w>[</w> : display worn armour\n"
            "<w>(</w> : display current weapons (also <w>)</w>)\n"
            "<w>\"</w> : display worn jewellery\n"
            "<w>C</w> : display experience info\n"
            "<w>^</w> : show religion screen\n"
            "<w>A</w> : show Abilities/mutations\n"
            "<w>\\</w> : show item knowledge\n"
            "<w>m</w> : show skill screen\n"
            "<w>%</w> : show resistances\n",
            true, true, cmdhelp_textfilter,45);

    cols.add_formatted(
            1,
            "<h>Dungeon Interaction and Information:\n"
            "<w>o</w>/<w>c</w> : Open/Close door\n"
            "<w><<</w>/<w>></w> : use staircase (<w><<</w> enter shop)\n"
            "<w>;</w>   : examine occupied tile\n"
            "<w>x</w>   : eXamine surroundings/targets\n"
            "<w>X</w>   : eXamine level map\n"
            "<w>O</w>   : show dungeon Overview\n",
            true, true, cmdhelp_textfilter,45);

    cols.add_formatted(
            1,
            "<h>Item Interaction (floor):\n"
            "<w>,</w> : pick up items (also <w>g</w>) \n"
            "    (press twice for pick up menu) \n"
            "<w>d</w> : Drop an item\n"
            "<w>d#</w>: Drop exact number of items \n"
            "<w>D</w> : Dissect a corpse \n"
            "<w>e</w> : Eat food from floor \n",
//            "<w>z</w> : Zap a wand \n"
//            "<w>r</w> : Read a scroll or book \n"
//            "<w>M</w> : Memorise a spell from a book \n"
//            "<w>w</w> : Wield an item ( <w>-</w> for none) \n"
//            "<w>'</w> : wield item a, or switch to b \n"
//            "<w>E</w> : Evoke power of wielded item\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Other Gameplay Actions:\n"
            "<w>a</w> : use special Ability\n"
            "<w>p</w> : Pray\n"
            "<w>Z</w> : cast a spell\n"
            "<w>!</w> : shout or command allies\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Non-Gameplay Commands / Info\n"
            "<w>V</w> : display Version information\n"
            "<w>Ctrl-P</w> : show Previous messages\n"
            "<w>Ctrl-R</w> : Redraw screen\n"
            "<w>Ctrl-C</w> : Clear main and level maps\n"
            "<w>#</w> : dump character to file\n"
            "<w>:</w> : add note to dump file\n"
            "<w>~</w> : add macro\n"
//            "<w>~</w> : save macros\n"
            "<w>=</w> : reassign inventory/spell letters\n"
            " \n",
            true, true, cmdhelp_textfilter);

//    cols.add_formatted(
//            1,
//            "<?s><h>Stash Management Commands:\n"
//            "<?s><w>Ctrl-S</w> : mark Stash\n"
//            "<?s><w>Ctrl-E</w> : Erase stash (ignore square)\n"
//            "<?s><w>Ctrl-F</w> : Find (in stashes and shops)\n",
//            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            "<h>Examine Surroundings ('<w>x</w><h>' in main):\n"
            " <w>x</w>  : back to map (also <w>Esc</w> and <w>Space</w>)\n"
            " <w>+</w>  : cycle monsters forward (also <w>=</w>)\n"
            " <w>-</w>  : cycle monsters backward\n"
            " <w>*</w>  : cycle objects forward (also <w>'</w>)\n"
            " <w>/</w>  : cycle objects backward (also <w>;</w>)\n"
            " <w>.</w>  : travel to cursor (also <w>Enter</w>)\n"
            " <w>?</w>  : describe monster under cursor\n"
            "<w><<</w>/<w>></w> : cycle through up/down stairs\n"
            "<w>Tab</w> : cycle dungeon features\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(
            1,
            targeting_help,
            true, true, cmdhelp_textfilter);

    cols.add_formatted(            
            1,
            "<h>Shortcuts in Lists (like multidrop):\n"
            "<w>)</w> : selects hand weapons\n"
            "<w>(</w> : selects missiles\n"
            "<w>[</w> : selects armour\n"
            "<w>%</w> : selects food, <w>&</w> : selects carrion\n"
            "<w>?</w> : selects scrolls\n"
            "<w>!</w> : selects potions\n"
            "<w>=</w> : selects rings\n"
            "<w>\"</w> : selects amulets\n"
            "<w>/</w> : selects wands\n"
            "<w>+</w> : selects books\n"
            "<w>\\</w> : selects staves\n"
            "<w>{</w> : selects miscellaneous items\n"
            "Carry the Orb to the surface and win!\n"
            "<w>,</w> : global selection\n"
            "<w>-</w> : global deselection\n"
            "<w>*</w> : invert selection\n",
            true, true, cmdhelp_textfilter);

    cols.add_formatted(            
            1, 
            " \n"
            "<?s>Crawl usually considers every item it\n"
            "<?s>sees as a stash. When using a value\n"
            "<?s>different from <darkgrey>stash_tracking = all</darkgrey>, you\n"
            "<?s>can use <w>Ctrl-S</w> to manually declare\n"
            "<?s>stashes, and <w>Ctrl-E</w> to erase them.\n",
            true, true, cmdhelp_textfilter);
    
    show_keyhelp_menu(cols.formatted_lines());
}

static void list_wizard_commands()
{
    const char *line;
    int j = 0;

#ifdef DOS_TERM
    char buffer[4800];

    window(1, 1, 80, 25);
    gettext(1, 1, 80, 25, buffer);
#endif

    clrscr();

    // BCR - Set to screen length - 1 to display the "more" string
    int moreLength = (get_number_of_lines() - 1) * 2;

    for (int i = 0; i < 500; i++)
    {
        line = wizard_string( i );

        if (strlen( line ) != 0)
        {
            // BCR - If we've reached the end of the screen, clear
            if (j == moreLength)
            {
                gotoxy(2, j / 2 + 1);
                cprintf("More...");
                getch();
                clrscr();
                j = 0;
            }

            gotoxy( ((j % 2) ? 40 : 2), ((j / 2) + 1) );
            cprintf( "%s", line );

            j++;
        }
    }

    getch();

#ifdef DOS_TERM
    puttext(1, 1, 80, 25, buffer);
#endif

    return;
}                               // end list_commands()

static const char *wizard_string( int i )
{
    UNUSED( i );

#ifdef WIZARD
    return((i ==  10) ? "a    : acquirement"                  :
           (i ==  13) ? "A    : set all skills to level"      :
           (i ==  15) ? "b    : controlled blink"             :
           (i ==  20) ? "B    : banish yourself to the Abyss" :
           (i ==  30) ? "g    : add a skill"                  :
           (i ==  35) ? "G    : remove all monsters"          :
           (i ==  40) ? "h/H  : heal yourself (super-Heal)"   :
           (i ==  50) ? "i/I  : identify/unidentify inventory":
           (i ==  70) ? "l    : make entrance to labyrinth"   :
           (i ==  80) ? "m/M  : create monster by number/name":
           (i ==  90) ? "o/%  : create an object"             :
           (i == 100) ? "p    : make entrance to pandemonium" :
           (i == 110) ? "x    : gain an experience level"     :
           (i == 115) ? "r    : change character's species"   :
           (i == 120) ? "s    : gain 20000 skill points"      :
           (i == 130) ? "S    : set skill to level"           :
           (i == 140) ? "t    : tweak object properties"      :
           (i == 150) ? "X    : Receive a gift from Xom"      :
           (i == 160) ? "z/Z  : cast any spell by number/name":
           (i == 200) ? "$    : get 1000 gold"                :
           (i == 210) ? "</>  : create up/down staircase"     :
           (i == 220) ? "u/d  : shift up/down one level"      :
           (i == 230) ? "~/\"  : goto a level"                :
           (i == 240) ? "(    : create a feature"             :
           (i == 250) ? "]    : get a mutation"               :
           (i == 260) ? "[    : get a demonspawn mutation"    :
           (i == 270) ? ":    : find branch"                  :
           (i == 280) ? "{    : magic mapping"                :
           (i == 290) ? "^    : gain piety"                   :
           (i == 300) ? "_    : gain religion"                :
           (i == 310) ? "\'    : list items"                  :
           (i == 320) ? "?    : list wizard commands"         :
           (i == 330) ? "|    : acquire all unrand artefacts" :
           (i == 340) ? "+    : turn item into random artefact" :
           (i == 350) ? "=    : sum skill points"
                      : "");

#else
    return ("");
#endif
}                               // end wizard_string()

#ifdef OBSOLETE_COMMAND_HELP
static const char *command_string( int i )
{
    /*
     * BCR - Command printing, case statement
     * Note: The numbers in this case indicate the order in which the
     *       commands will be printed out.  Make sure none of these
     *       numbers is greater than 500, because that is the limit.
     *
     * Arranged alpha lower, alpha upper, punctuation, ctrl.
     *
     */

    return((i ==  10) ? "a    : use special ability"              :
           (i ==  20) ? "d(#) : drop (exact quantity of) items"   :
           (i ==  30) ? "e    : eat food"                         :
           (i ==  40) ? "f    : fire first available missile"     :
           (i ==  50) ? "i    : inventory listing"                :
           (i ==  55) ? "m    : check skills"                     :
           (i ==  60) ? "o/c  : open / close a door"              :
           (i ==  65) ? "p    : pray"                             :
           (i ==  70) ? "q    : quaff a potion"                   :
           (i ==  80) ? "r    : read a scroll or book"            :
           (i ==  90) ? "s    : search adjacent tiles"            :
           (i == 100) ? "t    : throw/shoot an item"              :
           (i == 110) ? "v    : view item description"            :
           (i == 120) ? "w    : wield an item"                    :
           (i == 130) ? "x    : examine visible surroundings"     :
           (i == 135) ? "z    : zap a wand"                       :
           (i == 140) ? "A    : list abilities/mutations"         :
           (i == 141) ? "C    : check experience"                 :
           (i == 142) ? "D    : dissect a corpse"                 :
           (i == 145) ? "E    : evoke power of wielded item"      :
           (i == 150) ? "M    : memorise a spell"                 :
           (i == 155) ? "O    : overview of the dungeon"          :
           (i == 160) ? "P/R  : put on / remove jewellery"        :
           (i == 165) ? "Q    : quit without saving"              :
           (i == 168) ? "S    : save game and exit"               :
           (i == 179) ? "V    : version information"              :
           (i == 200) ? "W/T  : wear / take off armour"           :
           (i == 210) ? "X    : examine level map"                :
           (i == 220) ? "Z    : cast a spell"                     :
           (i == 240) ? ",/g  : pick up items"                    :
           (i == 242) ? "./del: rest one turn"                    :
           (i == 250) ? "</>  : ascend / descend a staircase"     :
           (i == 270) ? ";    : examine occupied tile"            :
           (i == 280) ? "\\    : check item knowledge"            :
#ifdef WIZARD
           (i == 290) ? "&    : invoke your Wizardly powers"      :
#endif
           (i == 300) ? "+/-  : scroll up/down [level map only]"  :
           (i == 310) ? "!    : shout or command allies"          :
           (i == 325) ? "^    : describe religion"                :
           (i == 337) ? "@    : status"                           :
           (i == 340) ? "#    : dump character to file"           :
           (i == 350) ? "=    : reassign inventory/spell letters" :
           (i == 360) ? "\'    : wield item a, or switch to b"    :
	   (i == 370) ? ":    : make a note"                      :
#ifdef USE_MACROS
           (i == 380) ? "`    : add macro"                        :
           (i == 390) ? "~    : save macros"                      :
#endif
           (i == 400) ? "]    : display worn armour"              :
           (i == 410) ? "\"    : display worn jewellery"          :
	   (i == 415) ? "{    : inscribe an item"                 :
           (i == 420) ? "Ctrl-P : see old messages"               :
#ifdef PLAIN_TERM
           (i == 430) ? "Ctrl-R : Redraw screen"                  :
#endif
           (i == 440) ? "Ctrl-A : toggle autopickup"              :
	   (i == 445) ? "Ctrl-M : toggle autoprayer"              :
	   (i == 447) ? "Ctrl-T : toggle fizzle"                  :
           (i == 450) ? "Ctrl-X : Save game without query"        :

#ifdef ALLOW_DESTROY_ITEM_COMMAND
           (i == 451) ? "Ctrl-D : Destroy inventory item"         :
#endif
           (i == 453) ? "Ctrl-G : interlevel travel"              :
           (i == 455) ? "Ctrl-O : explore"                        :

           (i == 456) ? "Ctrl-S : mark stash"                     :
           (i == 457) ? "Ctrl-E : forget stash"                   :
           (i == 458) ? "Ctrl-F : search stashes"                 :

           (i == 460) ? "Shift & DIR : long walk"                 :
           (i == 465) ? "/ DIR : long walk"                       :
           (i == 470) ? "Ctrl  & DIR : door; untrap; attack"      :
           (i == 475) ? "* DIR : door; untrap; attack"            :
           (i == 478) ? "Shift & 5 on keypad : rest 100 turns"
                      : "");
}                               // end command_string()
#endif  // OBSOLETE_COMMAND_HELP
