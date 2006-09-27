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
#include "wpn-misc.h"

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
#ifdef STASH_TRACKING
    "Stash-tracking",
#endif

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

    from_slot = prompt_invent_item( "Adjust which item?", -1 ); 
    if (from_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    in_name( from_slot, DESC_INVENTORY_EQUIP, str_pass );
    mpr( str_pass );

    to_slot = prompt_invent_item( "Adjust to which letter?", -1, false, false );
    if (to_slot ==  PROMPT_ABORT)
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

static void cmdhelp_showline(int index, const MenuEntry *me)
{
    static_cast<formatted_string *>(me->data)->display();
}

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
#ifdef STASH_TRACKING
    if (tag == "s")
        return (true);
#endif
#ifdef WIZARD
    if (tag == "wiz")
        return (true);
#endif
    return (false);
}

void list_commands(bool wizard)
{
    if (wizard)
    {
        list_wizard_commands();
        return;
    }
        
    static const char *command_template =
    "<h>Movement:                              Extended Movement:\n"
    "To move in a direction, use either     Ctrl-G    : Interlevel travel\n"
    "of these control schema:               Ctrl-O    : Auto-explore\n"
    "7 8 9  y k u                           Ctrl-W    : Mark waypoint\n"
    " \\|/    \\|/                            Shift-Dir.: Long walk\n"
    "4-5-6  h-.-l                           / Dir.    : Long walk\n"
    " /|\\    /|\\                            Shift-5   : Rest 100 turns\n"
    "7 8 9  b j n\n"
    "                                       <?s><h>Stash Management Commands:\n"
    "\"Center\" buttons (5/./Del) will        <?s>Ctrl-S    : Mark stash\n"
    "rest one turn.                         <?s>Ctrl-E    : Forget stash\n"
    "                                       <?s>Ctrl-F    : Search stashes\n"
    "\n"
    "<h>Dungeon Interaction and Information:   Player Character Information:\n"
    "o/c: Open/Close Door                   @  : Display character Status\n"
    "Ctrl-Dir.  : Door o/c, Untrap, Attack  ]  : Display worn armour\n"
    "* Direction: Door o/c, Untrap, Attack  \"  : Display worn jewellery\n"
    "</>: Ascend/Descend a staircase        C  : Display experience info\n"
    "s  : Search adjacent tiles             ^  : Show religion screen\n"
    ";  : Examine occupied tile             A  : Show abilities/mutations\n"
    "x  : Examine visible surroundings      \\  : Show item knowlede\n"
    "X  : Examine level map                 m  : Show skill screen\n"
    "+/-: Scroll up/down on level map       i  : Show inventory list\n"
    "O  : Show dungeon overview             %  : Show resistances\n"
    "\n"
    "<h>Item Interaction (inventory):          Other Actions:\n"
    "v  : View item description             a  : Use special ability\n"
    "z  : Zap a wand                        p  : Pray\n"
    "t  : throw/shoot an item               Z  : Cast a spell\n"
    "f  : fire first available missile      !  : Shout or command allies\n"
    "q  : Quaff a potion                    Ctrl-A: Toggle autopickup\n"
    "e  : eat food (also see below)\n"
    "r  : Read a scroll or book             <h>Game saving and exiting:\n"
    "M  : Memorise a spell from a book      S  : Save game and exit\n"
    "w  : Wield an item ( - for none)       Ctrl-X:Save game without query\n"
    "'  : Wield item a, or switch to b      Q  : Quit without saving\n"
    "E  : Evoke power of wielded item\n"
    "W  : Wear armour                       <h>Non-gameplay Commands / Info\n"
    "T  : Take off armour                   V  : Version Information\n"
    "P  : Put on jewellery                  Ctrl-P: See old messages\n"
    "R  : Remove jewellery                  Ctrl-R: Redraw screen\n"
    "d(#): Drop (exact quantity of) items   #  : Dump character to file\n"
    "=  : Reassign inventory/spell letters  `  : Add macro\n"
    "                                       ~  : Save macros\n"
    "<h>Item Interaction (floor):</h>              <?wiz>&  : Wizard-mode commands\n"
    "D  : Dissect a corpse\n"
    "e  : eat food (also see above)\n"
    ",/g: pick up items\n";
    std::vector<std::string> lines = 
            split_string("\n", command_template, false, true);

    Menu cmd_help;
    
    // Set flags, and don't use easy exit.
    cmd_help.set_flags(
            MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP,
            false);
    cmd_help.set_more(
            formatted_string::parse_string(
                "<cyan>[ + : Page down.   - : Page up."
                "                         Esc/x exits.]"));
    cmd_help.f_drawitem  = cmdhelp_showline;
    cmd_help.f_keyfilter = cmdhelp_keyfilter;

    std::vector<MenuEntry*> entries;

    for (unsigned i = 0, size = lines.size(); i < size; ++i)
    {
        MenuEntry *me = new MenuEntry;
        me->data = new formatted_string( 
                formatted_string::parse_string(
                    lines[i],
                    true,
                    cmdhelp_textfilter) );
        entries.push_back(me);

        cmd_help.add_entry(me);
    }

    cmd_help.show();

    for (unsigned i = 0, size = entries.size(); i < size; ++i)
        delete static_cast<formatted_string*>( entries[i]->data );
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
            cprintf( line );

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
           (i ==  90) ? "o/%%  : create an object"            :
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

#ifdef STASH_TRACKING
           (i == 456) ? "Ctrl-S : mark stash"                     :
           (i == 457) ? "Ctrl-E : forget stash"                   :
           (i == 458) ? "Ctrl-F : search stashes"                 :
#endif

           (i == 460) ? "Shift & DIR : long walk"                 :
           (i == 465) ? "/ DIR : long walk"                       :
           (i == 470) ? "Ctrl  & DIR : door; untrap; attack"      :
           (i == 475) ? "* DIR : door; untrap; attack"            :
           (i == 478) ? "Shift & 5 on keypad : rest 100 turns"
                      : "");
}                               // end command_string()
#endif  // OBSOLETE_COMMAND_HELP
