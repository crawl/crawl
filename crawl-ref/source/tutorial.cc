/*
 *  File:       tutorial.cc
 *  Summary:    A tutorial mode as an introduction on how to play Dungeon Crawl.
 *  Written by: j-p-e-g
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Created on 2007-01-11.
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "cio.h"

#include <cstring>
#include <sstream>

#include "tutorial.h"

#include "abl-show.h"
#include "cloud.h"
#include "command.h"
#include "describe.h"
#include "food.h"
#include "format.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "Kills.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "monstuff.h"
#include "mutation.h"
#include "newgame.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tiles.h"
#endif
#include "view.h"

static species_type _get_tutorial_species(unsigned int type);
static job_type     _get_tutorial_job(unsigned int type);
static void         _tutorial_describe_disturbance(int x, int y);
static void         _tutorial_describe_cloud(int x, int y);
static bool         _water_is_disturbed(int x, int y);

//#define TUTORIAL_DEBUG
#define TUTORIAL_VERSION  8

static int _get_tutorial_cols()
{
#ifdef USE_TILE
    return crawl_view.msgsz.x;
#else
    int ncols = get_number_of_cols();
    return (ncols > 80? 80 : ncols);
#endif
}

void save_tutorial(writer& outf)
{
    marshallLong( outf, TUTORIAL_VERSION);
    marshallShort( outf, Options.tutorial_type);
    for (long i = 0; i < TUT_EVENTS_NUM; ++i)
        marshallBoolean( outf, Options.tutorial_events[i] );
}

void load_tutorial(reader& inf)
{
    Options.tutorial_left = 0;

    int version = unmarshallLong(inf);
    if (version != TUTORIAL_VERSION)
        return;

    Options.tutorial_type = unmarshallShort(inf);
    for (long i = 0; i < TUT_EVENTS_NUM; ++i)
    {
        Options.tutorial_events[i] = unmarshallBoolean(inf);
        Options.tutorial_left += Options.tutorial_events[i];
    }
}

// Override init file definition for some options.
void init_tutorial_options()
{
    if (!Options.tutorial_left)
        return;

    Options.delay_message_clear = false;
    Options.auto_list = true;

#ifdef USE_TILE
    // Show all items in inventory.
    // FIXME: Ideally, we'd use the user-specified order, and push all
    //        missing item types at the end of it, NetHack like.
    //        Unfortunately I can't think of a remotely non-hacky way
    //        to do this.
    strncpy(Options.tile_show_items, "!?/%=([)x}+\\_.", 18);
    Options.tile_tag_pref = TAGPREF_TUTORIAL;
#endif
}

// Tutorial selection screen and choice.
bool pick_tutorial()
{
    clrscr();

    cgotoxy(1,1);
    formatted_string::parse_string(
        "<white>You must be new here indeed!</white>"
        EOL EOL
        "<cyan>You can be:</cyan>"
        EOL EOL).display();

    textcolor( LIGHTGREY );

    for (int i = 0; i < TUT_TYPES_NUM; i++)
        print_tutorial_menu(i);

    formatted_string::parse_string(
        EOL
        "<brown>SPACE - Back to class selection; "
        "Bksp - Back to race selection; X - Quit"
        EOL "* - Random tutorial"
        "</brown>" EOL).display();

    while (true)
    {
        char keyn = c_getch();

        // Random choice.
        if (keyn == '*' || keyn == '+' || keyn == '!' || keyn == '#')
            keyn = 'a' + random2(TUT_TYPES_NUM);

        // Choose character for tutorial game and set starting values.
        if (keyn >= 'a' && keyn <= 'a' + TUT_TYPES_NUM - 1)
        {
            Options.tutorial_type = keyn - 'a';
            you.species    = _get_tutorial_species(Options.tutorial_type);
            you.char_class = _get_tutorial_job(Options.tutorial_type);

            // Activate all triggers.
            // This is rather backwards: If (true) an event still needs to be
            // triggered, if (false) the relevant message was already printed.
            Options.tutorial_events.init(true);
            Options.tutorial_left = TUT_EVENTS_NUM;

            // Used to compare which fighting means was used most often.
            // XXX: This gets reset with every save, which seems odd.
            //      On the other hand, it's precisely between saves that
            //      players are most likely to forget these.
            Options.tut_spell_counter   = 0;
            Options.tut_throw_counter   = 0;
            Options.tut_melee_counter   = 0;
            Options.tut_berserk_counter = 0;

            // Store whether explore, stash search or travelling was used.
            // XXX: Also not stored across save games.
            Options.tut_explored = true;
            Options.tut_stashes  = true;
            Options.tut_travel   = true;

            // For occasional healing reminders.
            Options.tut_last_healed = 0;

            // Did the player recently see a monster turn invisible?
            Options.tut_seen_invisible = 0;

            Options.random_pick = true; // random choice of starting spellbook
            Options.weapon = WPN_HAND_AXE; // easiest choice for fighters
            return (true);
        }

        if (keyn == CK_BKSP || keyn == ' ')
        {
            // In this case, undo previous choices.
            you.species    = SP_UNKNOWN;
            you.char_class = JOB_UNKNOWN;
            Options.race   = 0;
            Options.cls    = 0;
        }

        switch (keyn)
        {
        case CK_BKSP:
            choose_race();
            return (false);
        case ' ':
            choose_class();
            return (false);
        case 'X':
            cprintf(EOL "Goodbye!");
            end(0);
            return (false);
        }
    }
    return (false);
}

void print_tutorial_menu(unsigned int type)
{
    char letter = 'a' + type;
    char desc[100];

    switch (type)
    {
      case TUT_BERSERK_CHAR:
          strcpy(desc, "(Melee oriented character with divine support)");
          break;
      case TUT_MAGIC_CHAR:
          strcpy(desc, "(Magic oriented character)");
          break;
      case TUT_RANGER_CHAR:
          strcpy(desc, "(Ranged fighter)");
          break;
      default: // no further choices
          strcpy(desc, "(erroneous character)");
          break;
    }

    cprintf("%c - %s %s %s" EOL,
            letter, species_name(_get_tutorial_species(type), 1).c_str(),
                    get_class_name(_get_tutorial_job(type)), desc);
}

static species_type _get_tutorial_species(unsigned int type)
{
    switch (type)
    {
      case TUT_BERSERK_CHAR:
          return SP_MINOTAUR;
      case TUT_MAGIC_CHAR:
          return SP_DEEP_ELF;
      case TUT_RANGER_CHAR:
          return SP_CENTAUR;
      default:
          // Use something fancy for debugging.
          return SP_KENKU;
    }
}

static job_type _get_tutorial_job(unsigned int type)
{
    switch (type)
    {
      case TUT_BERSERK_CHAR:
          return JOB_BERSERKER;
      case TUT_MAGIC_CHAR:
          return JOB_CONJURER;
      case TUT_RANGER_CHAR:
          return JOB_HUNTER;
      default:
          // Use something fancy for debugging.
          return JOB_NECROMANCER;
    }
}

// Converts all secret doors in a fixed radius around the player's starting
// position into normal closed doors.
void tutorial_zap_secret_doors()
{
    for (radius_iterator ri(you.pos(), 10, true, false); ri; ++ri)
        if (grd(*ri) == DNGN_SECRET_DOOR)
            grd(*ri) = DNGN_CLOSED_DOOR;
}

// Prints the tutorial welcome screen.
static formatted_string _tut_starting_info(unsigned int width)
{
    std::ostringstream istr;

    istr << "<white>Welcome to Dungeon Crawl!</white>" EOL EOL
         << "Your object is to lead a <w>"
         << species_name(you.species, 1) << " " << you.class_name
         <<
        "</w> safely through the depths of the dungeon, retrieving the "
        "fabled Orb of Zot and returning it to the surface. "
        "In the beginning, however, let discovery be your "
        "main goal. Try to delve as deeply as possible but beware; "
        "death lurks around every corner." EOL EOL
        "For the moment, just remember the following keys "
        "and their functions:" EOL
        "  <white>?\?</white> - shows the items and the commands" EOL
        "  <white>S</white>  - saves the game, to be resumed later "
        "(but note that death is permanent)" EOL
        "  <white>x</white>  - examines something in your vicinity" EOL EOL
        "This tutorial will help you play Crawl without reading any "
        "documentation. If you feel intrigued, there is more information "
        "available in the following files from the docs/ folder (all of "
        "which can also be read in-game):"
        EOL
        "  <lightblue>quickstart.txt</lightblue>     - "
        "A very short guide to Crawl." EOL
        "  <lightblue>crawl_manual.txt</lightblue>   - "
        "This contains all details on races, magic, skills, etc." EOL
        "  <lightblue>options_guide.txt</lightblue>  - "
        "Crawl's interface is highly configurable. This document " EOL
        "                       explains all the options." EOL
        EOL
        "Press <white>Space</white> to proceed to the basics "
        "(the screen division and movement)." EOL
        "Press <white>Esc</white> to fast forward to the game start.";

    std::string broken = istr.str();
    linebreak_string2(broken, width);
    return formatted_string::parse_block(broken);
}

#ifdef TUTORIAL_DEBUG
static std::string _tut_debug_list(int event)
{
    switch(event)
    {
    case TUT_SEEN_FIRST_OBJECT:
        return "seen first object";
    case TUT_SEEN_POTION:
        return "seen first potion";
    case TUT_SEEN_SCROLL:
        return "seen first scroll";
    case TUT_SEEN_WAND:
        return "seen first wand";
    case TUT_SEEN_SPBOOK:
        return "seen first spellbook";
    case TUT_SEEN_WEAPON:
        return "seen first weapon";
    case TUT_SEEN_MISSILES:
        return "seen first missiles";
    case TUT_SEEN_ARMOUR:
        return "seen first armour";
    case TUT_SEEN_RANDART:
        return "seen first random artefact";
    case TUT_SEEN_FOOD:
        return "seen first food";
    case TUT_SEEN_CARRION:
        return "seen first corpse";
    case TUT_SEE_GOLD:
        return "seen first pile of gold";
    case TUT_SEEN_JEWELLERY:
        return "seen first jewellery";
    case TUT_SEEN_MISC:
        return "seen first misc. item";
    case TUT_SEEN_MONSTER:
        return "seen first monster";
    case TUT_SEEN_STAIRS:
        return "seen first stairs";
    case TUT_SEEN_ESCAPE_HATCH:
        return "seen first escape hatch";
    case TUT_SEEN_BRANCH:
        return "seen first branch entrance";
    case TUT_SEEN_TRAP:
        return "encountered a trap";
    case TUT_SEEN_ALTAR:
        return "seen an altar";
    case TUT_SEEN_SHOP:
        return "seen a shop";
    case TUT_SEEN_DOOR:
        return "seen a closed door";
    case TUT_SEEN_SECRET_DOOR:
        return "found a secret door";
    case TUT_KILLED_MONSTER:
        return "killed first monster";
    case TUT_NEW_LEVEL:
        return "gained a new level";
    case TUT_SKILL_RAISE:
        return "raised a skill";
    case TUT_YOU_ENCHANTED:
        return "caught an enchantment";
    case TUT_YOU_SICK:
        return "became sick";
    case TUT_YOU_POISON:
        return "were poisoned";
    case TUT_YOU_ROTTING:
        return "were rotting";
    case TUT_YOU_CURSED:
        return "had something cursed";
    case TUT_YOU_HUNGRY:
        return "felt hungry";
    case TUT_YOU_STARVING:
        return "were starving";
    case TUT_MAKE_CHUNKS:
        return "learned about chunks";
    case TUT_OFFER_CORPSE:
        return "learned about sacrifice";
    case TUT_MULTI_PICKUP:
        return "read about pickup menu";
    case TUT_HEAVY_LOAD:
        return "were encumbered";
    case TUT_ROTTEN_FOOD:
        return "carried rotten food";
    case TUT_NEED_HEALING:
        return "needed healing";
    case TUT_NEED_POISON_HEALING:
        return "needed healing for poison";
    case TUT_INVISIBLE_DANGER:
        return "encountered an invisible foe";
    case TUT_NEED_HEALING_INVIS:
        return "had to heal near an unseen monster";
    case TUT_ABYSS:
        return "was cast into the Abyss";
    case TUT_POSTBERSERK:
        return "learned about Berserk after-effects";
    case TUT_RUN_AWAY:
        return "were told to run away";
    case TUT_RETREAT_CASTER:
        return "were told to retreat as a caster";
    case TUT_SHIFT_RUN:
        return "learned about shift-run";
    case TUT_MAP_VIEW:
        return "learned about the level map";
    case TUT_DONE_EXPLORE:
        return "explored a level";
    case TUT_YOU_MUTATED:
        return "caught a mutation";
    case TUT_NEW_ABILITY:
        return "gained a divine ability";
    case TUT_WIELD_WEAPON:
        return "wielded an unsuitable weapon";
    case TUT_FLEEING_MONSTER:
        return "made a monster flee";
    case TUT_MONSTER_BRAND:
        return "learned about colour brandings";
    case TUT_MONSTER_FRIENDLY:
        return "seen first friendly monster";
    case TUT_CONVERT:
        return "converted to a god";
    case TUT_GOD_DISPLEASED:
        return "piety ran low";
    case TUT_EXCOMMUNICATE:
        return "excommunicated by a god";
    case TUT_SPELL_MISCAST:
        return "spell miscast";
    case TUT_SPELL_HUNGER:
        return "spell casting caused hunger";
    case TUT_GLOWING:
        return "player glowing from contamination";
    case TUT_STAIR_BRAND:
        return "saw stairs with objects on it";
    case TUT_YOU_RESIST:
        return "resisted some magic";
    case TUT_CAUGHT_IN_NET:
        return "were caught in a net";
    case TUT_LOAD_SAVED_GAME:
        return "restored a saved game";
    case TUT_GAINED_MAGICAL_SKILL:
        return "gained a new magical skill";
    case TUT_CHOOSE_STAT:
        return "could choose a stat";
    case TUT_CAN_BERSERK:
        return "were told to Berserk";
    default:
        return "faced a bug";
    }
}

// Lists all triggerable events and whether they actually were triggered
// at some point, at game start or reload.
static formatted_string _tutorial_debug()
{
    std::string result;
    bool lbreak = false;
    snprintf(info, INFO_SIZE, "Tutorial Debug Screen");

    int i = _get_tutorial_cols()/2-1 - strlen(info) / 2;
    result += std::string(i, ' ');
    result += "<white>";
    result += info;
    result += "</white>" EOL EOL;

    result += "<lightblue>";
    for (i = 0; i < TUT_EVENTS_NUM; i++)
    {
        snprintf(info, INFO_SIZE, "%d: %s (%s)",
                 i, _tut_debug_list(i).c_str(),
                 Options.tutorial_events[i] ? "true" : "false");

        result += info;

        // Break text into 2 columns where possible.
        if (strlen(info) >= _get_tutorial_cols()/2 || lbreak)
        {
            result += EOL;
            lbreak = false;
        }
        else
        {
            result += std::string(_get_tutorial_cols()/2-1 - strlen(info), ' ');
            lbreak = true;
        }
    }
    result += "</lightblue>" EOL EOL;

    snprintf(info, INFO_SIZE, "tutorial_left: %d\n", Options.tutorial_left);
    result += info;
    result += EOL;

    snprintf(info, INFO_SIZE, "You are a %s %s, and the tutorial will reflect "
             "that.", species_name(you.species, 1), you.class_name);

    result += info;

    return formatted_string::parse_string(result);
}
#endif // debug

#ifndef USE_TILE
static formatted_string _tutorial_map_intro()
{
    std::string result;

    result  = "<";
    result += colour_to_str(channel_to_colour(MSGCH_TUTORIAL));
    result += ">";
    result += "What you see here is the typical Crawl screen. The upper left "
              "map shows your hero as the <w>@</w> in the center. The parts "
              "of the map you remember but cannot currently see will be greyed "
              "out.";
    result += "</";
    result += colour_to_str(channel_to_colour(MSGCH_TUTORIAL));
    result += ">" EOL;
    result += "<lightgrey> --more--                               "
              "Press <white>Escape</white> to skip the basics.</lightgrey>";

    linebreak_string2(result, _get_tutorial_cols());
    return formatted_string::parse_block(result, false);
}

static formatted_string _tutorial_stats_intro()
{
    std::ostringstream istr;

    // Note: must fill up everything to override the map
    istr << "<"
         << colour_to_str(channel_to_colour(MSGCH_TUTORIAL))
         << ">"
         << "To the right, important properties \n"
            "of the character are displayed. The \n"
            "most basic one is Health, shown as \n"
            "<w>Health: " << you.hp << "/" << you.hp_max << "</w> "
            "and meaning current \n"
            "out of maximum health points. When \n"
            "Health drops to zero, you die. \n"
            "<w>Magic: " << you.magic_points << "/" << you.max_magic_points
         << "</w> represents your energy \n"
            "for casting spells, although other \n"
            "actions often draw from Magic, too. \n"
            "<w>Str</w>ength, <w>Int</w>elligence, <w>Dex</w>terity \n"
            "below provide an all-around account \n"
            "of the character's attributes. \n"
            "Don't worry about the rest for now. \n"
         << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">"
            "                                      \n"
            "<lightgrey> --more--                               "
            "Press <white>Escape</white> to skip the basics.</lightgrey>\n"
            "                                      \n"
            "                                      \n";

    return formatted_string::parse_block(istr.str(), false);
}
#endif

static void _tutorial_message_intro()
{
    std::string result;

    result  = "This lower part of the screen is reserved for messages. "
              "Everything related to the tutorial is shown in this colour. If "
              "you missed something, previous messages can be read again with "
              "<w>Ctrl-P</w>"
#ifdef USE_TILE
              " or by <w>clicking into the message area</w>"
#endif
              "." EOL;
    result += "<lightgrey> --more--                               "
              "Press <white>Escape</white> to skip the basics.</lightgrey>";

    mesclr();
    formatted_message_history(result, MSGCH_TUTORIAL, 0, _get_tutorial_cols());
}

static void _tutorial_movement_info()
{
    std::string text =
        "To move your character, use the numpad; try Numlock both on and off. "
        "If your system has no number pad, or if you are familiar with the vi "
        "keys, movement is also possible with <w>hjklyubn</w>. "
#ifdef USE_TILE
        "You can also move by clicking somewhere on the map. If this is "
        "considered safe, i.e. there are no monsters around, you'll move "
        "towards the chosen square."
#endif
        EOL
        "A basic command list can be found under <w>?\?</w>, and the most "
        "important commands will be explained to you as it becomes necessary.";
    mesclr();
    formatted_message_history(text, MSGCH_TUTORIAL, 0, _get_tutorial_cols());
}

// copied from display_mutations and adapted
void tut_starting_screen()
{
#ifndef USE_TILE
    int y_pos;
#endif
    int MAX_INFO = 4;
#ifdef TUTORIAL_DEBUG
    MAX_INFO = 5; // add tutorial_debug
#endif
    char ch;

    int i;
    for (i = 0; i <= MAX_INFO; i++)
    {
#ifndef USE_TILE
        // Map window (starts at 1) or message window (starts at 18).
        // FIXME: This should be done more cleanly using the
        //        crawl_view settings!
        y_pos = (i == 1 || i == 3) ? 18 : 1;

        cgotoxy(1, y_pos);
#endif
        if (i == 0)
            clrscr();

        int width = _get_tutorial_cols();
#ifdef USE_TILE
        // use a more sensible screen width
        if (width < 80 && width < crawl_view.msgsz.x + crawl_view.hudsz.x)
            width = crawl_view.msgsz.x + crawl_view.hudsz.x;
        if (width > 80)
            width = 80;
#endif
        if (i == 0)
            _tut_starting_info(width).display();
#ifdef USE_TILE
        // Skip map and stats explanation for Tiles.
        else if (i > 0 && i < 3)
            continue;
#else
        else if (i == 1)
            _tutorial_map_intro().display();
        else if (i == 2)
            _tutorial_stats_intro().display();
#endif
        else if (i == 3)
            _tutorial_message_intro();
        else if (i == 4)
            _tutorial_movement_info();
        else
        {
#ifdef TUTORIAL_DEBUG
            clrscr();
 #ifndef USE_TILE
            cgotoxy(1,y_pos);
 #endif
            _tutorial_debug().display();
#else
            continue;
#endif
        }

        if (i < MAX_INFO)
        {
            ch = c_getch();
            redraw_screen();
            if (ch == ESCAPE)
                break;
        }
    }
    if (i >= MAX_INFO)
        more();

    mesclr();
}

// Once a tutorial character dies, offer some playing hints.
void tutorial_death_screen()
{
    Options.tutorial_left = 0;
    std::string text;

    mpr( "Condolences! Your character's premature death is a sad, but "
         "common occurrence in Crawl. Rest assured that with diligence and "
         "playing experience your characters will last longer.",
         MSGCH_TUTORIAL);

    mpr( "Perhaps the following advice can improve your playing style:",
         MSGCH_TUTORIAL);
    more();

    if (Options.tutorial_type == TUT_MAGIC_CHAR
        && Options.tut_spell_counter < Options.tut_melee_counter )
    {
        text = "As a Conjurer your main weapon should be offensive magic. Cast "
               "spells more often! Remember to rest when your Magic is low.";
    }
    else if (you.religion == GOD_TROG && Options.tut_berserk_counter <= 3 )
    {
        text = "Don't forget to go berserk when fighting particularly "
               "difficult foes. It is risky, but makes you faster and beefier.";
    }
    else if (Options.tutorial_type == TUT_RANGER_CHAR
             && 2*Options.tut_throw_counter < Options.tut_melee_counter )
    {
        text = "Your bow and arrows are extremely powerful against distant "
               "monsters. Be sure to collect all arrows lying around in the "
               "dungeon.";
    }
    else
    {
        int hint = random2(6);
        // If a character has been unusually busy with projectiles and spells
        // give some other hint rather than the first one.
        if (hint == 0 && Options.tut_throw_counter + Options.tut_spell_counter
                          >= Options.tut_melee_counter)
        {
            hint = random2(5)+1;
        }
        // FIXME: The hints below could be somewhat less random, so that e.g.
        // the message for fighting several monsters in a corridor only happens
        // if there's more than one monster around and you're not in a corridor,
        // or the one about using consumable objects only if you actually have
        // any (useful or unidentified) scrolls/wands/potions.

        switch (hint)
        {
        case 0:
            text = "Always consider using projectiles, wands or spells before "
                   "engaging monsters in close combat.";
            break;

        case 1:
            text = "Learn when to run away from things you can't handle - this "
                   "is important! It is often wise to skip a particularly "
                   "dangerous level. But don't overdo this as monsters will "
                   "only get harder the deeper you delve.";
            break;

        case 2:
            text = "Rest between encounters, if possible in an area already "
                   "explored and cleared of monsters. In Crawl, searching and "
                   "resting are one and the same. To search for one turn, "
                   "press <w>s</w>, <w>.</w>, <w>delete</w> or "
                   "<w>keypad-5</w>. Pressing <w>5</w> or "
                   "<w>shift-and-keypad-5</w>"
#ifdef USE_TILE
                   ", or clicking into the stat area"
#endif
                   " will let you rest for a longer time (you will stop "
                   "resting after 100 turns, or when fully healed).";
            break;

        case 3:
            text =  "Remember to use those scrolls, potions or wands you've "
                    "found. Very often, you cannot expect to identify "
                    "everything with the scroll only. Learn to improvise: "
                    "identify through usage.";
            break;

        case 4:
            text =  "If a particular encounter feels overwhelming don't "
                    "forget to use emergency items early on. A scroll of "
                    "teleportation or a potion of speed can really save your "
                    "bacon.";
            break;

        case 5:
            text =  "Never fight more than one monster, if you can help it. "
                    "Always back into a corridor so that they are forced to "
                    "fight you one on one.";
            break;

        default:
            text =  "Sorry, no hint this time, though there should have been "
                    "one.";
        }
    }
    formatted_message_history(text, MSGCH_TUTORIAL, 0, _get_tutorial_cols());
    more();

    mpr( "See you next game!", MSGCH_TUTORIAL);

    Options.tutorial_events.init(false);
}

// If a character survives until Xp 7, the tutorial is declared finished
// and they get a more advanced playing hint, depending on what they might
// know by now.
void tutorial_finished()
{
    std::string text;

    Options.tutorial_left = 0;
    text =  "Congrats! You survived until the end of this tutorial - be sure "
            "to try the other ones as well. Note that the help screen "
            "(<w>?\?</w>) will look very different from now on. Here's a last "
            "playing hint:";

    formatted_message_history(text, MSGCH_TUTORIAL, 0, _get_tutorial_cols());
    more();

    if (Options.tut_explored)
    {
        text =  "Walking around and exploring levels gets easier by using "
                "auto-explore (<w>o</w>). You can even make Crawl "
                "automatically pick up interesting items by setting the "
                "option <w>explore_greedy=true</w> in the init file.";
    }
    else if (Options.tut_travel)
    {
        text =  "There is a convenient way for travelling between far away "
                "dungeon levels: press <w>Ctrl-G</w> or <w>G</w> and enter "
                "the desired destination. If your travel gets interrupted, "
                "issuing <w>Ctrl-G Enter</w> or <w>G Enter</w> will continue "
                "it.";
    }
    else if (Options.tut_stashes)
    {
        text =  "You can search among all items existing in the dungeon with "
                "the <w>Ctrl-F</w> command. For example, "
                "<w>Ctrl-F \"knife\"</w> will list all knives. You can then "
                "travel to one of the spots. It is even possible to enter "
                "words like <w>\"shop\"</w> or <w>\"altar\"</w>.";
    }
    else
    {
        int hint = random2(3);
        switch (hint)
        {
          case 0:
              text = "The game keeps an automated logbook for your characters. "
                     "Use <w>?:</w> to read it. You can enter notes manually "
                     "with the <w>:</w> command. Once your character perishes, "
                     "two morgue files are left in the <w>morgue/</w> "
                     "directory. The one ending in .txt contains a copy of "
                     "your logbook. During play, you can create a dump file "
                     "with <w>#</w>.";
              break;

          case 1:
              text = "Crawl has a macro function built in: press <w>~m</w> "
                     "to define a macro by first specifying a trigger key "
                     "(say, <w>F1</w>) and a command sequence, for example "
                     "<w>za+.</w>. The latter will make the <w>F1</w> "
                     "key always zap the spell in slot a at the nearest "
                     "monster. For more information on macros, type <w>?~</w>.";
              break;

          case 2:
              text = "The interface can be greatly customised. All options are "
                     "explained in the file <w>options_guide.txt</w> which "
                     "can be found in the <w>docs</w> directory. The options "
                     "themselves are set in <w>init.txt</w> or "
                     "<w>.crawlrc</w>. Crawl will complain if it can't find "
                     "either file.";
               break;

          default:
              text =  "Oops... No hint for now. Better luck next time!";
        }
    }
    formatted_message_history(text, MSGCH_TUTORIAL, 0, _get_tutorial_cols());
    more();

    Options.tutorial_events.init(false);
}

// Occasionally remind religious characters of sacrifices.
void tutorial_dissection_reminder(bool healthy)
{
    if (Options.tut_just_triggered || !Options.tutorial_left)
        return;

    // When hungry, give appropriate message or at least don't suggest
    // sacrifice.
    if (you.hunger_state < HS_SATIATED && healthy)
    {
        learned_something_new(TUT_MAKE_CHUNKS);
        return;
    }

    if (!god_likes_butchery(you.religion))
        return;

    if (Options.tutorial_events[TUT_OFFER_CORPSE])
        learned_something_new(TUT_OFFER_CORPSE);
    else if (one_chance_in(8))
    {
        Options.tut_just_triggered = true;

        std::string text;
        text += "If you don't want to eat it, consider <w>c</w>hopping this "
                "corpse up under <w>p</w>rayer as a sacrifice to ";
        text += god_name(you.religion);
#ifdef USE_TILE
        text += ". You can also chop up any corpse that shows in the floor "
                "part of your inventory tiles by clicking on it with your "
                "<w>left mouse button</w>";
#endif

        text += ". Whenever you view a corpse while in tutorial mode you can "
                "reread this information.";

        formatted_message_history(text, MSGCH_TUTORIAL, 0,
                                  _get_tutorial_cols());

        if (is_resting())
            stop_running();
    }
}

// Occasionally remind injured characters of resting.
void tutorial_healing_reminder()
{
    if (!Options.tutorial_left)
        return;

    if (you.duration[DUR_POISONING] && 2*you.hp < you.hp_max)
    {
        if (Options.tutorial_events[TUT_NEED_POISON_HEALING])
            learned_something_new(TUT_NEED_POISON_HEALING);
    }
    else if (Options.tut_seen_invisible > 0
             && you.num_turns - Options.tut_seen_invisible <= 20)
    {
        // If we recently encountered an invisible monster, we need a
        // special message.
        learned_something_new(TUT_NEED_HEALING_INVIS);
        // If that one was already displayed, don't print a reminder.
    }
    else
    {
        if (Options.tutorial_events[TUT_NEED_HEALING])
            learned_something_new(TUT_NEED_HEALING);
        else if (you.num_turns - Options.tut_last_healed >= 50
                 && !you.duration[DUR_POISONING])
        {
            if (Options.tut_just_triggered)
                return;

            Options.tut_just_triggered = 1;

            std::string text;
            text =  "Remember to rest between fights and to enter unexplored "
                    "terrain with full hitpoints and magic. Ideally you "
                    "should retreat into areas you've already explored and "
                    "cleared of monsters; resting on the edge of the explored "
                    "terrain increases the chances of your rest being "
                    "interrupted by wandering monsters. For resting, press "
                    "<w>5</w> or <w>Shift-numpad 5</w>"
#ifdef USE_TILE
                    ", or click on the stat area with your mouse"
#endif
                    ".";

            if (you.religion == GOD_TROG && !you.duration[DUR_BERSERKER]
                && !you.duration[DUR_EXHAUSTED]
                && you.hunger_state >= HS_SATIATED)
            {
              text += "\nAlso, berserking might help you not to lose so many "
                      "hitpoints in the first place. To use your abilities type "
                      "<w>a</w>.";
            }
            formatted_message_history(text, MSGCH_TUTORIAL, 0,
                                      _get_tutorial_cols());

            if (is_resting())
                stop_running();
        }
        Options.tut_last_healed = you.num_turns;
    }
}

// Give a message if you see, pick up or inspect an item type for the
// first time.
void taken_new_item(unsigned char item_type)
{
    switch(item_type)
    {
      case OBJ_WANDS:
          learned_something_new(TUT_SEEN_WAND);
          break;
      case OBJ_SCROLLS:
          learned_something_new(TUT_SEEN_SCROLL);
          break;
      case OBJ_JEWELLERY:
          learned_something_new(TUT_SEEN_JEWELLERY);
          break;
      case OBJ_POTIONS:
          learned_something_new(TUT_SEEN_POTION);
          break;
      case OBJ_BOOKS:
          learned_something_new(TUT_SEEN_SPBOOK);
          break;
      case OBJ_FOOD:
          learned_something_new(TUT_SEEN_FOOD);
          break;
      case OBJ_CORPSES:
          learned_something_new(TUT_SEEN_CARRION);
          break;
      case OBJ_WEAPONS:
          learned_something_new(TUT_SEEN_WEAPON);
          break;
      case OBJ_ARMOUR:
          learned_something_new(TUT_SEEN_ARMOUR);
          break;
      case OBJ_MISSILES:
          learned_something_new(TUT_SEEN_MISSILES);
          break;
      case OBJ_MISCELLANY:
          learned_something_new(TUT_SEEN_MISC);
          break;
      case OBJ_STAVES:
          learned_something_new(TUT_SEEN_STAFF);
          break;
      case OBJ_GOLD:
          learned_something_new(TUT_SEEN_GOLD);
          break;
      default: // nothing to be done
          return;
    }
}

// Give a special message if you gain a skill you didn't have before.
void tut_gained_new_skill(int skill)
{
    if (!Options.tutorial_left)
        return;

    learned_something_new(TUT_SKILL_RAISE);

    switch (skill)
    {
    // Special cases first.
    case SK_FIGHTING:
    case SK_ARMOUR:
    case SK_STEALTH:
    case SK_STABBING:
    case SK_TRAPS_DOORS:
    case SK_UNARMED_COMBAT:
    case SK_INVOCATIONS:
    case SK_EVOCATIONS:
    case SK_DODGING:
    case SK_SHIELDS:
    case SK_THROWING:
    case SK_SPELLCASTING:
    {
        formatted_message_history(get_skill_description(skill), MSGCH_TUTORIAL,
                                  0, _get_tutorial_cols());
        stop_running();
        break;
    }
    // Only one message for all magic skills (except Spellcasting).
    case SK_CONJURATIONS:
    case SK_ENCHANTMENTS:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_TRANSLOCATIONS:
    case SK_TRANSMUTATIONS:
    case SK_DIVINATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_POISON_MAGIC:
        learned_something_new(TUT_GAINED_MAGICAL_SKILL);
        break;

    // Melee skills.
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
    case SK_AXES:
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
    case SK_STAVES:
        learned_something_new(TUT_GAINED_MELEE_SKILL);
        break;

    // Ranged skills.
    case SK_SLINGS:
    case SK_BOWS:
    case SK_CROSSBOWS:
    case SK_DARTS:
        learned_something_new(TUT_GAINED_RANGED_SKILL);
        break;

    default:
        break;
    }
}

#ifndef USE_TILE
// As safely as possible, colourize the passed glyph.
// Handles quoting "<", MBCS-ing unicode, and
// making DEC characters safe if not properly printable.
static std::string _colourize_glyph(int col, unsigned glyph)
{
    std::string colour_str = colour_to_str(col);
    std::ostringstream text;
    text << "<" << colour_str << ">";

    text << stringize_glyph(glyph);
    if (glyph == '<') text << '<';

    text << "</" << colour_str << ">";
    return text.str();
}
#endif

static bool _mons_is_highlighted(const monsters *mons)
{
    return (mons_friendly(mons)
                && Options.friend_brand != CHATTR_NORMAL
            || mons_looks_stabbable(mons)
                && Options.stab_brand != CHATTR_NORMAL
            || mons_looks_distracted(mons)
                && Options.may_stab_brand != CHATTR_NORMAL);
}

static bool _advise_use_wand()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &obj(you.inv[i]);

        if (!is_valid_item( obj ))
            continue;

        if (obj.base_type != OBJ_WANDS)
            continue;

        // Wand type unknown, might be useful.
        if (!item_type_known(obj))
            return (true);

        // Empty wands are no good.
        if (obj.plus2 == ZAPCOUNT_EMPTY
            || item_ident(obj, ISFLAG_KNOW_PLUSES) && obj.plus <= 0)
        {
            continue;
        }

        // Can it be used to fight?
        switch(obj.sub_type)
        {
        case WAND_FLAME:
        case WAND_FROST:
        case WAND_SLOWING:
        case WAND_MAGIC_DARTS:
        case WAND_PARALYSIS:
        case WAND_FIRE:
        case WAND_COLD:
        case WAND_CONFUSION:
        case WAND_FIREBALL:
        case WAND_TELEPORTATION:
        case WAND_LIGHTNING:
        case WAND_ENSLAVEMENT:
        case WAND_DRAINING:
        case WAND_RANDOM_EFFECTS:
        case WAND_DISINTEGRATION:
            return (true);
        }
    }

    return (false);
}

void tutorial_first_monster(const monsters &mon)
{
    if (!Options.tutorial_events[TUT_SEEN_MONSTER])
    {
        if (get_mons_colour(&mon) != mon.colour)
            learned_something_new(TUT_MONSTER_BRAND, mon.pos());
        if (mons_friendly(&mon))
            learned_something_new(TUT_MONSTER_FRIENDLY, mon.pos());

        if (!Options.tut_just_triggered
            && one_chance_in(4)
            && you.religion == GOD_TROG && !you.duration[DUR_BERSERKER]
            && !you.duration[DUR_EXHAUSTED]
            && you.hunger_state >= HS_SATIATED)
        {
            learned_something_new(TUT_CAN_BERSERK);
        }
        return;
    }

    // XXX: Crude hack (and doesn't really work either):
    // If the first monster is sleeping wake it
    // (highlighting is an unnecessary complication).
    if (_mons_is_highlighted(&mon))
    {
        noisy(1, mon.pos());
        viewwindow(true, false);
    }

    stop_running();

    Options.tutorial_events[TUT_SEEN_MONSTER] = false;
    Options.tutorial_left--;
    Options.tut_just_triggered = true;

    std::string text = "That ";
#ifdef USE_TILE
    // need to highlight monster
    const coord_def gc = mon.pos();
    tiles.place_cursor(CURSOR_TUTORIAL, gc);
    tiles.add_text_tag(TAG_TUTORIAL, &mon);

    text += "monster is a ";
    text += mon.name(DESC_PLAIN).c_str();
    text += ". Examples for typical early monsters are rats, giant newts, "
            "kobolds, or goblins. You can gain information about any monster "
            "by hovering your mouse over its tile, and read the monster "
            "description by clicking on it with your <w>right mouse button</w>."
#else
    unsigned ch;
    unsigned short col;
    get_mons_glyph(&mon, &ch, &col);

    text += _colourize_glyph(col, ch);
    text += " is a monster, usually depicted by a letter. Some typical "
            "early monsters look like <brown>r</brown>, <green>l</green>, "
            "<brown>K</brown> or <lightgrey>g</lightgrey>. ";

    if (crawl_view.mlistsz.y > 0)
    {
        text += "Your console settings allowing, you'll always see a "
                "list of monsters somewhere on the screen." EOL;
    }
    text += "You can gain information about it by pressing <w>x</w> and "
            "moving the cursor on the monster, and read the monster "
            "description by then pressing <w>v</w>. "
#endif
            "\nTo attack this monster with your wielded weapon, just move "
            "into it. ";

#ifdef USE_TILE
    text += "Note that as long as there's a non-friendly monster in view you "
            "won't be able to automatically move to distant squares, to avoid "
            "death by misclicking.";
#endif

    formatted_message_history(text, MSGCH_TUTORIAL, 0, _get_tutorial_cols());

    if (Options.tutorial_type == TUT_RANGER_CHAR)
    {
        text =  "However, as a hunter you will want to deal with it using your "
                "bow. If you have a look at your bow from your "
                "<w>i</w>nventory, you'll find an explanation of how to do "
                "this. ";

        if (!you.weapon()
            || you.weapon()->base_type != OBJ_WEAPONS
            || you.weapon()->sub_type != WPN_BOW)
        {
            text += "First <w>w</w>ield it, then follow the instructions.";

#ifdef USE_TILE
        text += EOL "As a short-cut you can also <w>right-click</w> on your "
                "bow to read its description, and <w>left-click</w> to wield "
                "it.";
#endif
        }
#ifdef USE_TILE
        else
        {
            text += "Clicking with your <w>right mouse button</w> on your bow "
                    "will also let you read its description.";
        }
#endif


        formatted_message_history(text, MSGCH_TUTORIAL, 0,
                                  _get_tutorial_cols());
    }
    else if (Options.tutorial_type == TUT_MAGIC_CHAR)
    {
        text =  "However, as a conjurer you will want to deal with it using "
                "magic. If you have a look at your spellbook from your "
                "<w>i</w>nventory, you'll find an explanation of how to do "
                "this.";

#ifdef USE_TILE
        text += EOL "As a short-cut you can also <w>right-click</w> on your "
                "book in your inventory to read its description.";
#endif
        formatted_message_history(text, MSGCH_TUTORIAL, 0,
                                  _get_tutorial_cols());
    }

    if (get_mons_colour(&mon) != mon.colour)
        learned_something_new(TUT_MONSTER_BRAND, mon.pos());
    if (mons_friendly(&mon))
        learned_something_new(TUT_MONSTER_FRIENDLY, mon.pos());
}

void tutorial_first_item(const item_def &item)
{
    // Happens if monster is standing on dropped corpse or item.
    if (monster_at(item.pos))
        return;

    if (!Options.tutorial_events[TUT_SEEN_FIRST_OBJECT]
        || Options.tut_just_triggered)
    {
        // NOTE: Since a new player might not think to pick up a
        // corpse, TUT_SEEN_CARRION is done when a corpse is first seen.
        if (!Options.tut_just_triggered
            && item.base_type == OBJ_CORPSES
            && monster_at(item.pos) == NULL)
        {
            learned_something_new(TUT_SEEN_CARRION, item.pos);
        }
        return;
    }

    stop_running();

    Options.tutorial_events[TUT_SEEN_FIRST_OBJECT] = false;
    Options.tutorial_left--;
    Options.tut_just_triggered = true;

    std::string text = "That ";
#ifndef USE_TILE
    unsigned ch;
    unsigned short col;
    get_item_glyph(&item, &ch, &col);

    text += _colourize_glyph(col, ch);
    text += " ";
#else
    const coord_def gc = item.pos;
    tiles.place_cursor(CURSOR_TUTORIAL, gc);
    tiles.add_text_tag(TAG_TUTORIAL, item.name(DESC_CAP_A), gc);
#endif
    text += "is an item. If you move there and press <w>g</w> or "
            "<w>,</w> you will pick it up. "
#ifndef USE_TILE
            "Generally, items are shown by non-letter symbols like "
            "<w>%?!\"=()[</w>. "
#else
            "You can also pick up an item by clicking on your <w>left mouse "
            "button</w> while standing on its square. "
#endif
            "Also, several types of objects will usually be picked up "
            "automatically. "
#ifdef USE_TILE
            "(In Tiles, these will be marked with a green frame around them.)"
#endif
            EOL "Once it is in your inventory, you can drop it again with "
#ifdef USE_TILE
            "a <w>left mouse click</w> while pressing the <w>Shift key</w>. "
            "Whenever you <w>right-click</w> on an item"
#else
            "<w>d</w>. Any time you look at an item in your <w>i</w>nventory"
#endif
            ", you can read about its properties and its description.";

    formatted_message_history(text, MSGCH_TUTORIAL, 0, _get_tutorial_cols());
}

static void _new_god_conduct()
{
    std::ostringstream text;

    const std::string new_god_name  = god_name(you.religion);

    text << "You've just converted to worshipping <w>" << new_god_name
         << "</w>. ";

    if (you.religion == GOD_XOM)
    {
        // Xom is a special case.
        text << "You can keep Xom happy by keeping him amused; you do "
                "absolutely not want this god to grow bored with you!\n"
                "If you keep Xom amused he'll treat you like a plaything, "
                "randomly helping and harming you for his own amusement; "
                "otherwise he'll treat you like a disfavoured plaything.";

        formatted_message_history(text.str(), MSGCH_TUTORIAL, 0,
                                  _get_tutorial_cols());
        return;
    }

    if (is_good_god(you.religion))
    {
        // For the good gods, piety grows over time.
        text << "From now on, " << new_god_name << " will watch over you and "
                "judge your behaviour. Thus, your actions will greatly "
                "influence your piety (divine favour). If your piety runs out ";
    }
    else
    {
        text << "Your piety (divine favour) will gradually decrease over time, "
                "and if it runs out ";
    }

    text << new_god_name << " will excommunicate you and punish you. "
            "You can prevent this, however, and even gain enough piety to get "
            "powers and divine gifts, by doing things to please "
         << new_god_name << ". But don't panic: you start out with a decent "
            "amount of piety, so any danger of excommunication is far off.\n";

    formatted_message_history(text.str(), MSGCH_TUTORIAL, 0,
                              _get_tutorial_cols());
    text.str("");

    text << "\nYou can check your god's likes and dislikes, as well as your "
            "current standing and divine abilities, by typing <w>^</w>"
#ifdef USE_TILE
            " (alternatively press <w>Shift</w> while "
            "<w>right-clicking</w> on your avatar)"
#endif
            ".";

    formatted_message_history(text.str(), MSGCH_TUTORIAL, 0,
                              _get_tutorial_cols());
}

#define DELAY_EVENT \
{ \
    Options.tutorial_events[seen_what] = true; \
    Options.tutorial_left++; \
    return; \
}

// Here most of the tutorial messages for various triggers are handled.
void learned_something_new(tutorial_event_type seen_what, coord_def gc)
{
    // Already learned about that.
    if (!Options.tutorial_events[seen_what])
        return;

    // Don't trigger twice in the same turn.
    if (Options.tut_just_triggered)
        return;

    std::ostringstream text;

#ifndef USE_TILE
    const coord_def e = gc - you.pos() + coord_def(9,9);
    unsigned ch;
    unsigned short colour;
    int object;
#endif

    Options.tut_just_triggered = true;
    Options.tutorial_events[seen_what] = false;
    Options.tutorial_left--;

    switch (seen_what)
    {
    case TUT_SEEN_POTION:
        text << "You have picked up your first potion"
#ifndef USE_TILE
                " ('<w>!</w>'). Use "
#else
                ". Simply click on it with your <w>left mouse button</w>, or "
                "press "
#endif
                "<w>q</w> to quaff it.";
        break;

    case TUT_SEEN_SCROLL:
        text << "You have picked up your first scroll"
#ifndef USE_TILE
                " ('<w>?</w>'). Type "
#else
                ". Simply click on it with your <w>left mouse button</w>, or "
                "type "
#endif
                "<w>r</w> to read it.";
        break;

    case TUT_SEEN_WAND:
        text << "You have picked up your first wand"
#ifndef USE_TILE
                " ('<w>/</w>'). Type "
#else
                ". Simply click on it with your <w>left mouse button</w>, or "
                "type "
#endif
                "<w>Z</w> to zap it.";
        break;

    case TUT_SEEN_SPBOOK:
        text << "You have picked up a book ";
#ifndef USE_TILE
        text << "('<w>";

        get_item_symbol(DNGN_ITEM_BOOK, &ch, &colour);
        text << static_cast<char>(ch)
             << "'</w>) "
             << "that you can read by typing <w>r</w>. "
                "If it's a spellbook you'll then be able to memorise spells "
                "via <w>M</w> and cast a memorised spell with <w>z</w>.";
#else
        text << ". You can read it doing a <w>right click</w> with your "
                "mouse, and memorise spells with a <w>left click</w>. ";
#endif

        if (you.religion == GOD_TROG)
        {
            text << "\nAs a worshipper of "
                 << god_name(GOD_TROG)
                 << ", though, you might instead wish to burn those tomes "
                    "of hated magic by using the corresponding "
                    "<w>a</w>bility.";
        }
        else if (!you.skills[SK_SPELLCASTING])
        {
            text << "\nHowever, first you will have to get accustomed to "
                    "spellcasting by reading lots of scrolls.";
        }
        text << "\nDuring the tutorial you can reread this information at "
                "any time by "
#ifndef USE_TILE
                "having a look in your <w>i</w>nventory at the item in "
                "question.";
#else
                "clicking on it with your <w>right mouse button</w>.";
#endif
        break;

    case TUT_SEEN_WEAPON:
        text << "This is the first weapon "
#ifndef USE_TILE
                "('<w>)</w>') "
#endif
                "you've picked up. Use <w>w</w> "
#ifdef USE_TILE
                "or click on it with your <w>left mouse button</w> "
#endif
                "to wield it, but be aware that this weapon "
                "might train a different skill from your current one. You can "
                "view the weapon's properties from your <w>i</w>nventory"
#ifdef USE_TILE
                " or by <w>right-clicking</w> on it"
#endif
                ".";

        if (Options.tutorial_type == TUT_BERSERK_CHAR)
        {
            text << "\nAs you're already trained in Axes you should stick "
                    "with these. Checking other axes can be worthwhile.";
        }
        break;

    case TUT_SEEN_MISSILES:
        text << "This is the first stack of missiles "
#ifndef USE_TILE
                "('<w>(</w>') "
#endif
                "you've picked up. Darts can be thrown by hand, but other "
                "missile types like arrows and needles require a launcher "
                "and training in using it to be really effective. "
#ifdef USE_TILE
                "<w>Right-clicking</w> on "
#else
                "Selecting "
#endif
                "the item in your <w>i</w>nventory will give more "
                "information about both missiles and launcher.";

        if (Options.tutorial_type == TUT_RANGER_CHAR)
        {
            text << "\nAs you're already trained in Bows you should stick"
                    " with arrows and collect more of them in the dungeon.";
        }
        else if (Options.tutorial_type == TUT_MAGIC_CHAR)
        {
            text << "\nHowever, as a spellslinger, you don't really need "
                    "another type of ranged attack, unless there's another "
                    "effect in addition to damage.";
        }
        else
        {
            text << "\nFor now you might be best off with sticking to darts "
                    "or stones for ranged attacks.";
        }
        break;

    case TUT_SEEN_ARMOUR:
        text << "This is the first piece of armour "
#ifndef USE_TILE
                "('<w>[</w>') "
#endif
                "you've picked up. "
#ifdef USE_TILE
                "You can click on it to wear it, and click a second time to "
                "take it off again. Doing a <w>right mouse click</w> will "
                "show you its properties.";
#else
                "Use <w>W</w> to wear it and <w>T</w> to take it off again. "
                "You can view its properties from your <w>i</w>nventory.";
#endif

        if (you.species == SP_CENTAUR || you.species == SP_MINOTAUR)
        {
            text << "\nNote that as a " << species_name(you.species, 1)
                 << " you will be unable to wear "
                 << (you.species == SP_CENTAUR ? "boots" : "helmets")
                 << ".";
        }
        break;

    case TUT_SEEN_RANDART:
        text << "Weapons and armour that have unusual descriptions like this "
                "are much more likely to be of higher enchantment or have "
                "special properties, good or bad. The rarer the description, "
                "the greater the potential value of an item.";
        break;

    case TUT_SEEN_FOOD:
        text << "You have picked up some food"
#ifndef USE_TILE
                " ('<w>%</w>')"
#endif
                ". You can eat it by typing <w>e</w>"
#ifdef USE_TILE
                " or by clicking on it with your <w>left mouse button</w>"
#endif
                ".";
        break;

    case TUT_SEEN_CARRION:
        // NOTE: This is called when a corpse is first seen as well as when
        //       first picked up, since a new player might not think to pick
        //        up a corpse.

        if (gc.x <= 0 || gc.y <= 0)
            text << "Ah, a corpse!";
        else
        {
            int i = igrd(gc);
            while (i != NON_ITEM)
            {
                if (mitm[i].base_type == OBJ_CORPSES)
                    break;

                i = mitm[i].link;
            }
            if (i == NON_ITEM)
                text << "Ah, a corpse!";
            else
            {
                text << "That ";
#ifndef USE_TILE
                unsigned short col;
                get_item_glyph(&mitm[i], &ch, &col);

                text << _colourize_glyph(col, ch);
                text << " ";
#else
                tiles.place_cursor(CURSOR_TUTORIAL, gc);
                tiles.add_text_tag(TAG_TUTORIAL, mitm[i].name(DESC_CAP_A), gc);
#endif

                text << "is a corpse.";
            }
        }

        text << " When a corpse is lying on the ground, you "
                "can <w>c</w>hop it up with a sharp implement. Once "
                "hungry you can then <w>e</w>at the resulting chunks "
                "(though they may not be healthy).";
#ifdef USE_TILE
        text << " With tiles, you can also chop up any corpse that shows in "
                "the floor part of your inventory region, simply by doing a "
                "<w>left mouse click</w> while pressing <w>Shift</w>, and "
                "then eat the resulting chunks with <w>Shift + right mouse "
                "click</w>.";

#endif
        if (god_likes_butchery(you.religion))
        {
            text << " During prayer you can offer corpses to "
                 << god_name(you.religion)
                 << " by chopping them up, as well. Note that the gods will "
                 << "not accept rotting flesh.";
        }
        text << "\nDuring the tutorial you can reread this information at "
                "any time by selecting the item in question in your "
                "<w>i</w>nventory.";
        break;

    case TUT_SEEN_JEWELLERY:
        text << "You have picked up a a piece of jewellery, either a ring"
#ifndef USE_TILE
             << " ('<w>=</w>')"
#endif
             << " or an amulet"
#ifndef USE_TILE
             << " ('<w>\"</w>')"
             << ". Type <w>P</w> to put it on and <w>R</w> to remove "
               "it. You can view its properties from your <w>i</w>nventory"
#else
             << ". You can click on it to put it on, and click a second time "
                "remove it off again. By clicking on it with your <w>right "
                "mouse button</w> you can view its properties"
#endif
             << ", though often magic is necessary to reveal its true "
                "nature.";
        break;

    case TUT_SEEN_MISC:
        text << "This is a curious object indeed. You can play around with "
                "it to find out what it does by "
#ifdef USE_TILE
                "clicking on it once to <w>w</w>ield it, and a second time "
                "to e<w>v</w>oke "
#else
                "<w>w</w>ielding and e<w>v</w>oking "
#endif
                "it. As usually, selecting it from your <w>i</w>nventory "
                "might give you more information.";
        break;

    case TUT_SEEN_STAFF:
        text << "You have picked up a magic staff or a rod"
#ifndef USE_TILE
                ", both of which are represented by '<w>";

        get_item_symbol(DNGN_ITEM_STAVE, &ch, &colour);
        text << static_cast<char>(ch)
             << "</w>'"
#endif
                ". Both must be <w>w</w>ielded to be of use. "
                "Magicians use staves to increase their power in certain "
                "spell schools. By contrast, a rod allows the casting of "
                "certain spells even without magic knowledge simply by "
                "e<w>v</w>oking it. For the latter the power depends on "
                "your Evocations skill.";
#ifdef USE_TILE
        text << "Both wielding, and evoking a wielded item can be achieved "
                "by clicking on it with your <w>left mouse button</w>.";
#endif
        text << "\nDuring the tutorial you can reread this information at "
                "any time by selecting the item in question in your "
                "<w>i</w>nventory.";
        break;

    case TUT_SEEN_GOLD:
        text << "You have picked up your first pile of gold"
#ifndef USE_TILE
                " ('<yellow>$</yellow>')"
#endif
                ". Unlike all other objects in Crawl it doesn't show up in "
                "your inventory, takes up no space in your inventory, weighs "
                "nothing and can't be dropped. Gold can be used to buy "
                "items from shops, and can also be sacrificed to some gods. ";

        if (!Options.show_gold_turns)
        {
            text << "Whenever you pick up some gold, your current amount will "
                    "be mentioned. If you'd like to check your wealth at other "
                    "times, it will be listed on the <w>%</w> screen.";
        }
        break;

    case TUT_SEEN_STAIRS:
        // Don't give this information during the first turn, to give
        // the player time to have a look around.
        if (you.num_turns < 1)
            DELAY_EVENT;

        text << "These ";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        object = env.show(e);
        colour = env.show_col(e);
        { unsigned short dummy; get_item_symbol( object, &ch, &dummy ); }

        text << _colourize_glyph(colour, ch) << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Stairs", gc);
#endif
        text << "are some downstairs. You can enter the next (deeper) "
                "level by following them down (<w>></w>). To get back to "
                "this level again, press <w><<</w> while standing on the "
                "upstairs.";
#ifdef USE_TILE
        text << "\nAlternatively, clicking on your <w>left mouse button</w> "
                "while pressing the <w>Shift key</w> will let you follow any "
                "stairs you're standing on.";
#endif
        break;

    case TUT_SEEN_ESCAPE_HATCH:
        if (you.num_turns < 1)
            DELAY_EVENT;

        // monsters standing on stairs
        if (monster_at(gc))
            DELAY_EVENT;

        text << "These ";
#ifndef USE_TILE
        object = env.show(e);
        colour = env.show_col(e);
        get_item_symbol( object, &ch, &colour );

        text << _colourize_glyph(colour, ch);
        text << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Escape hatch", gc);
#endif
        text << "are some kind of escape hatch. You can use them to "
                "quickly leave a level with <w><<</w> and <w>></w>, "
                "respectively "
#ifdef USE_TILE
                "(or by using your <w>left mouse button</w> in combination "
                "with the <w>Shift key</w>)"
#endif
                ", but will usually be unable to return right away.";
        break;

    case TUT_SEEN_BRANCH:
        text << "This ";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        // FIXME: Branch entrance character is not being colored yellow.
        object = env.show(e);
        colour = env.show_col(e);
        { unsigned short dummy; get_item_symbol( object, &ch, &dummy ); }

        text << _colourize_glyph(colour, ch) << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Branch stairs", gc);
#endif
        text << "is the entrance to a different branch of the dungeon, "
                "which might have different terrain, level layout and "
                "monsters than the current main branch you're in.  Branches "
                "can range from being up to ten levels deep to having only "
                "a single level. They can also contain entrances to other "
                "branches."

                "\n\nThe first three branches you'll encounter are the "
                "Temple, the Orcish Mines and the Lair.  While the Mines "
                "and the Lair can be dangerous for the new adventurer, "
                "the Temple is completely safe and contains a number of "
                "altars at which you might convert to a new god.";
        break;

    case TUT_STAIR_BRAND:
#ifdef USE_TILE
        // XXX: How does stair branding work with tiles?
        return;
#else
        // Monster or player standing on stairs.
        if (monster_at(gc) || (you.pos() == gc))
            DELAY_EVENT;

        text << "If any items are covering stairs or an escape hatch then "
                "that will be indicated by highlighting the <w><<</w> or "
                "<w>></w> symbol, instead of hiding the stair symbol with "
                "an item glyph.";
#endif
        break;

    case TUT_SEEN_TRAP:
        if (you.pos() == gc)
            text << "Oops... you just triggered a trap. ";
        else
            text << "You just discovered a trap. ";

        text << "An unwary adventurer will occasionally stumble into one "
                "of these nasty constructions";
#ifndef USE_TILE
        object = env.show(e);
        colour = env.show_col(e);
        get_item_symbol( object, &ch, &colour );

        if (ch == ' ' || colour == BLACK)
            colour = LIGHTCYAN;

        text << " depicted by " << _colourize_glyph(colour, '^');
#endif
        text << ". They can do physical damage (with darts or needles, for "
                "example) or have other, more magical effects, like "
                "teleportation.";
        break;

    case TUT_SEEN_ALTAR:
        text << "That ";
#ifndef USE_TILE
        object = env.show(e);
        colour = env.show_col(e);
        get_item_symbol( object, &ch, &colour );
        text << _colourize_glyph(colour, ch) << " ";
#else
        {
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            std::string altar = "An altar to ";
            altar += god_name(grid_altar_god(grd(gc)));
            tiles.add_text_tag(TAG_TUTORIAL, altar, gc);
        }
#endif
        text << "is an altar. You can get information about it by pressing "
                "<w>p</w> while standing on the square. Before taking up "
                "the responding faith you'll be asked for confirmation.";

        if (you.religion == GOD_NO_GOD
            && Options.tutorial_type == TUT_MAGIC_CHAR)
        {
            text << "\n\nThe best god for an unexperienced conjurer is "
                    "probably Vehumet, though Sif Muna is a good second "
                    "choice.";
        }
        break;

    case TUT_SEEN_SHOP:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, shop_name(gc), gc);
#endif
        text << "That "
#ifndef USE_TILE
             << _colourize_glyph(YELLOW, get_screen_glyph(gc)) << " "
#endif
                "is a shop. You can enter it by typing <w><<</w> "
#ifdef USE_TILE
                ", or by pressing <w>Shift</w> and clicking on it with your "
                "<w>left mouse button</w> "
#endif
                "while standing on the square.";
        break;

    case TUT_SEEN_DOOR:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Closed door", gc);
#endif
        if (you.num_turns < 1)
            DELAY_EVENT;

        text << "That "
#ifndef USE_TILE
             << _colourize_glyph(WHITE, get_screen_glyph(gc)) << " "
#endif
                "is a closed door. You can open it by walking into it. "
                "Sometimes it is useful to close a door. Do so by pressing "
                "<w>C</w> while standing next to it. If there are several "
                "doors you will then be prompted for a direction. "
                "Alternatively, you can also use <w>Ctrl-Direction</w>.";
#ifdef USE_TILE
        text << "\nIn Tiles, the same can be achieved by clicking on an "
                "adjacent door square.";
#endif
        break;

    case TUT_SEEN_SECRET_DOOR:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Secret door", gc);
#endif
        text << "That "
#ifndef USE_TILE
             << _colourize_glyph(WHITE, get_screen_glyph(gc)) << " "
#endif
                "was a secret door. You can actively try to find secret "
                "doors by searching. To search for one turn, press <w>s</w>, "
                "<w>.</w>, <w>delete</w> or <w>keypad-5</w>. Pressing "
                "<w>5</w> or <w>shift-and-keypad-5</w> "
#ifdef USE_TILE
                ", or clicking into the stat area "
#endif
                "will search 100 times, stopping early if you find any "
                "secret doors or traps, or when your HP or MP fully "
                "recovers.\n\n"

                "If you can't find all three (or any) of the down stairs "
                "on a level you should try searching for secret doors, since "
                "the missing stairs might be in sections of the level blocked "
                "off by them. If you can't find any secret doors then the "
                "missing stairs are probably in sections of the level totally "
                "disconnected from the section you're searching.";
        break;

    case TUT_KILLED_MONSTER:
        text << "Congratulations, your character just gained some experience "
                "by killing this monster! Every action will use up some of "
                "it to train certain skills. For example, fighting monsters ";

        if (Options.tutorial_type == TUT_BERSERK_CHAR)
        {
            text << "in melee battle will raise your Axes and Fighting "
                    "skills.";
        }
        else if (Options.tutorial_type == TUT_RANGER_CHAR)
        {
            text << "using bow and arrows will raise your Bows skill.";
        }
        else // if (Options.tutorial_type == TUT_MAGIC_CHAR)
        {
            text << "with offensive magic will raise your Conjurations and "
                    "Spellcasting skills.";
        }

        if (you.religion == GOD_TROG)
        {
            text << " Also, kills of living creatures are automatically "
                    "dedicated to "
                 << god_name(you.religion)
                 << ".";
        }
        break;

    case TUT_NEW_LEVEL:
        text << "Well done! Reaching a new experience level is always a nice "
                "event: you get more health and magic points, and "
                "occasionally increases to your attributes (strength, "
                "dexterity, intelligence).";

        if (Options.tutorial_type == TUT_MAGIC_CHAR)
        {
            text << "\nAlso, new experience levels let you learn more spells "
                    "(the Spellcasting skill also does this). For now, you "
                    "should try to memorise the second spell of your "
                    "starting book with <w>Mcb</w>, which can then be zapped "
                    "with <w>zb</w>.";
#ifdef USE_TILE
            text << " Memorising is also possible by doing a <w>left "
                    "click</w> on the book in your inventory.";
#endif
        }
        break;

    case TUT_SKILL_RAISE:
        text << "One of your skills just got raised. You can train your skills "
                "or pick up new ones by performing the corresponding actions. "
                "To view or manage your skill set, type <w>m</w>.";
        break;

    case TUT_GAINED_MAGICAL_SKILL:
        text << "Being skilled in a magical \"school\" makes it easier to "
                "learn and cast spells of this school. Many spells belong to "
                "a combination of several schools, in which case the average "
                "skill in these schools will decide on spellcasting success "
                "and power.";
        break;

    case TUT_GAINED_MELEE_SKILL:
        text << "Being skilled with a particular type of weapon will make it "
                "easier to fight with all weapons of this type and make you "
                "deal more damage with them. It is generally recommended to "
                "concentrate your efforts on one or two weapon types to become "
                "more powerful in them. Some weapons are closely related, and "
                "being trained in one will ease training the other. This is "
                "true for the following pairs: Short Blades/Long Blades, "
                "Axes/Polearms, Polearms/Staves, Axes/Maces, and (though not "
                "strictly a weapon skill) Slings/Throwing.";
        break;

    case TUT_GAINED_RANGED_SKILL:
        text << "Being skilled in a particular type of ranged attack will let "
                "you deal more damage when using the appropriate weapons. It "
                "is usually best to concentrate on one type of ranged attack "
                "(including spells), and to add another one as back-up.";
        break;

    case TUT_CHOOSE_STAT:
        text << "Every third level you may choose what stat to invest in, "
                "Strength, Dexterity, or Intelligence. <w>Strength</w> "
                "influences the amount you can carry, and increases the damage "
                "you deal in melee. <w>Dexterity</w> increases your evasion "
                "and thus influences your chance of dodging attacks or traps. "
                "<w>Intelligence</w> increases your success in casting spells "
                "and decreases the amount by which you hunger when you do so.\n"
                "Note that it is generally recommended to raise all your "
                "stats to a minimum of 8, so as to prevent death by stat loss.";
        break;

    case TUT_YOU_ENCHANTED:
        text << "Enchantments of all types can befall you temporarily. "
                "Brief descriptions of these appear at the lower end of the "
                "stats area. Press <w>@</w> for more details. A list of all "
                "possible enchantments is in the manual (<w>?5</w>).";
        break;

    case TUT_YOU_SICK:
        // Hack: reset tut_just_triggered, to force recursive calling of
        // learned_something_new().
        Options.tut_just_triggered = false;
        learned_something_new(TUT_YOU_ENCHANTED);
        Options.tut_just_triggered = true;
        text << "Corpses can be spoiled or inedible, making you sick. "
                "Also, some monsters' flesh is less palatable than others'. "
                "While sick, your hitpoints won't regenerate and sometimes "
                "an attribute may decrease. It wears off with time (";

        if (!i_feel_safe())
            text << "find a quiet corner and ";

        text << "wait with <w>5</w>"
#ifdef USE_TILE
                " or by clicking into the stats area"
#endif
                "), or you could quaff a potion of healing. ";
        break;

    case TUT_YOU_POISON:
        // Hack: reset tut_just_triggered, to force recursive calling of
        // learned_something_new().
        Options.tut_just_triggered = false;
        learned_something_new(TUT_YOU_ENCHANTED);
        Options.tut_just_triggered = true;
        text << "Poison will slowly reduce your HP. It wears off with time (";

        if (!i_feel_safe())
            text << "find a quiet corner and ";

        text << "wait with <w>5</w>"
#ifdef USE_TILE
                "or by clicking onto the stats area"
#endif
                "), or you could quaff a potion of healing. ";
        break;

    case TUT_YOU_ROTTING:
        // Hack: Reset tut_just_triggered, to force recursive calling of
        //       learned_something_new().
        Options.tut_just_triggered = false;
        learned_something_new(TUT_YOU_ENCHANTED);
        Options.tut_just_triggered = true;

        text << "Ugh, your flesh is rotting!  Not only does this slowly "
                "reduce your HP, it also slowly reduces your <w>maximum</w> "
                "HP (your usual maximum HP will be indicated by a number in "
                "parentheses).\n"
                "While you can wait it out, you'll probably want to stop "
                "rotting as soon as possible by drinking a potion of healing, "
                "since the longer you wait the more your maximum HP will be "
                "reduced.  Once you've stopped rotting you can restore your "
                "maximum HP to normal by drinking potions of healing and heal "
                "wounds while fully healed.";
        break;

    case TUT_YOU_CURSED:
        text << "Curses are comparatively harmless, but they do mean that "
                "you cannot remove cursed equipment and will have to suffer "
                "the (possibly) bad effects until you find and read a scroll "
                "of remove curse. Weapons and armour can also be uncursed "
                "using the appropriate enchantment scrolls.";
        break;

    case TUT_YOU_HUNGRY:
        text << "There are two ways to overcome hunger: food you started "
                "with or found, and selfmade chunks from corpses. To get the "
                "latter, all you need to do is <w>c</w>hop up a corpse "
                "with a sharp implement. Your starting weapon will do "
                "nicely. Try to dine on chunks in order to save permanent "
                "food.";

        if (Options.tutorial_type == TUT_BERSERK_CHAR)
            text << "\nNote that you cannot Berserk while hungry or worse.";
        break;

    case TUT_YOU_STARVING:
        text << "You are now suffering from terrible hunger. You'll need to "
                "<w>e</w>at something quickly, or you'll die. The safest "
                "way to deal with this is to simply eat something from your "
                "inventory rather than wait for a monster to leave a corpse.";

        if (Options.tutorial_type == TUT_MAGIC_CHAR)
            text << "\nNote that you cannot cast spells while starving.";
        break;

    case TUT_MULTI_PICKUP:
        text << "There's a more comfortable way to pick up several items at "
                "the same time. If you press <w>,</w> or <w>g</w> "
#ifdef USE_TILE
                ", or click your left mouse button "
#endif
                "twice you can choose items from a menu"
#ifdef USE_TILE
                ", either by pressing their letter, or by clicking on their "
                "tiles presented at the bottom of the screen"
#endif
                ".\nThis takes fewer keystrokes but has no influence on the "
                "number of turns needed. Multi-pickup will be interrupted by "
                "monsters or other dangerous events.";
        break;

    case TUT_HEAVY_LOAD:
        if (you.burden_state != BS_UNENCUMBERED)
        {
            text << "It is not usually a good idea to run around encumbered; "
                    "it slows you down and increases your hunger.";
        }
        else
        {
            text << "Sadly, your inventory is limited to 52 items, and it "
                    "appears your knapsack is full.";
        }

        text << " However, this is easy enough to rectify: simply "
                "<w>d</w>rop some of the stuff you don't need or that's too "
                "heavy to lug around permanently.";
#ifdef USE_TILE
        text << " In the drop menu you can then comfortably select which "
                "items to drop by pressing their inventory letter, or by "
                "clicking on their tiles.";
#endif
        break;

    case TUT_ROTTEN_FOOD:
        text << "One or more of the chunks or corpses you carry has started "
                "to rot. Few races can digest these safely, so you might "
                "just as well <w>d</w>rop them now.";
        break;

    case TUT_MAKE_CHUNKS:
        text << "How lucky! That monster left a corpse which you can now "
                "<w>c</w>hop up. One or more chunks will appear that you "
                "can then <w>e</w>at. Beware that some chunks may be, "
                "sometimes or always, hazardous. You can find out whether "
                "that might be the case by "
#ifdef USE_TILE
                "clicking with your <w>right mouse button</w> onto the corpse "
                "or chunk.";
#else
                "<w>v</w>iewing a corpse or chunk on the floor or by having a "
                "look at it in your <w>i</w>nventory.";
#endif

        if (you.duration[DUR_PRAYER]
            && (god_likes_butchery(you.religion)
                || god_hates_butchery(you.religion)))
        {
            text << "\nRemember, though, to wait until your prayer is over, "
                    "or the corpse will instead be sacrificed to "
                 << god_name(you.religion)
                 << ".";
        }
        break;

    case TUT_OFFER_CORPSE:
        if (!god_likes_butchery(you.religion)
            || you.hunger_state < HS_SATIATED)
        {
            return;
        }

        text << "Hey, that monster left a corpse! If you don't need it for "
                "food or other purposes, you can sacrifice it to "
             << god_name(you.religion)
             << " by <w>c</w>hopping it while <w>p</w>raying.";
        break;

    case TUT_SHIFT_RUN:
        text << "Walking around takes fewer keystrokes if you press "
                "<w>Shift-direction</w> or <w>/ direction</w>. "
                "That will let you run until a monster comes into sight or "
                "your character sees something interesting.";
        break;

    case TUT_MAP_VIEW:
        text << "As you explore a level, orientation can become difficult. "
                "Press <w>X</w> to bring up the level map. Typing <w>?</w> "
                "shows the list of level map commands. "
                "Most importantly, moving the cursor to a spot and pressing "
                "<w>.</w> or <w>Enter</w> lets your character move there on "
                "its own.";
#ifdef USE_TILE
        text << "\nAlso, clicking on the right-side minimap with your "
                "<w>right mouse button</w> will zoom into that dungeon area. "
                "Clicking with the <w>left mouse button</w> instead will let "
                "you move there.";
#endif
        break;

    case TUT_DONE_EXPLORE:
        // XXX: You'll only get this message if you're using auto exploration.
        text << "Hey, you've finished exploring the dungeon on this level! "
                "You can search for stairs from the level map (<w>X</w>) "
                "by pressing <w>></w>. The cursor will jump to the nearest "
                "staircase, and by pressing <w>.</w> or <w>Enter</w> your "
                "character can move there, too. ";

        if (Options.tutorial_events[TUT_SEEN_STAIRS])
        {
            text << "In rare cases, you may have found no downstairs at all. "
                    "Try searching for secret doors in suspicious looking "
                    "spots; use <w>s</w>, <w>.</w> or for 100 turns with "
                    "<w>5</w> "
#ifdef USE_TILE
                    "(or alternatively click into the stat area) "
#endif
                    "to do so.";
        }
        else
        {
            text << "Each level of Crawl has three "
#ifndef USE_TILE
                    "white "
#endif
                    "up and three "
#ifndef USE_TILE
                    "white "
#endif
                    "down stairs. Unexplored parts can often be accessed via "
                    "another level or through secret doors. To find the "
                    "latter, search the adjacent squares of walls for one "
                    "turn with <w>.</w> or <w>s</w>, or for 100 turns with "
                    "<w>5</w> or <w>Shift-numpad 5</w>"
#ifdef USE_TILE
                    ", or by clicking on the stat area"
#endif
                    ".\n\n";
        }
        break;

    case TUT_NEED_HEALING:
        text << "If you're low on hitpoints or magic and there's no urgent "
                "need to move, you can rest for a bit. Ideally you should "
                "retreat to an area you've already explored and cleared "
                "of monsters before resting, since resting on the edge of "
                "the explored terrain increases the risk of rest being "
                "interrupted by a wandering monster. Press <w>5</w> or "
                "<w>shift-numpad-5</w>"
#ifdef USE_TILE
                ", or click into the stat area"
#endif
                " to do so.";
        break;

    case TUT_NEED_POISON_HEALING:
        text << "Your poisoning could easily kill you, so now would be a "
                "good time to <w>q</w>uaff a potion of heal wounds or, "
                "better yet, a potion of healing. If you have seen neither "
                "of these so far, try unknown ones in your inventory. Good "
                "luck!";
        break;

    case TUT_INVISIBLE_DANGER:
        text << "Fighting against a monster you cannot see is difficult. "
                "Your best bet is probably a strategic retreat, be it via "
                "teleportation or by getting off the level. "
                "Or else, luring the monster into a corridor should at least "
                "make it easier for you to hit it.";

        // To prevent this text being immediately followed by the next one...
        Options.tut_last_healed = you.num_turns - 30;
        break;

    case TUT_NEED_HEALING_INVIS:
        text << "You recently noticed an invisible monster, so resting might "
                "not be safe. If you still need to replenish your hitpoints "
                "or magic, you'll have to quaff an appropriate potion. For "
                "normal resting you will first have to get away from the "
                "danger.";

        Options.tut_last_healed = you.num_turns;
        break;

    case TUT_CAN_BERSERK:
        // Don't print this information if the player already knows it.
        if (!Options.tut_berserk_counter)
        {
            text << "Against particularly difficult foes, you should use your "
                    "Berserk <w>a</w>bility. Berserk will last longer if you "
                    "kill a lot of monsters.";
        }
        break;

    case TUT_POSTBERSERK:
        text << "Berserking is extremely exhausting! It burns a lot of "
                "nutrition, and afterwards you are slowed down and "
                "occasionally even pass out. Press <w>@</w> to see your "
                "current exhaustion status.";
        break;

    case TUT_RUN_AWAY:
        text << "Whenever you've got only a few hitpoints left and you're in "
                "danger of dying, check your options carefully. Often, "
                "retreat or use of some item might be a viable alternative "
                "to fighting on.";

        if (you.species == SP_CENTAUR)
        {
            text << " As a four-legged centaur you are particularly quick - "
                    "running is an option!";
        }

        text << " If retreating to another level, keep in mind that monsters "
                "may follow you if they're standing right next to you when "
                "you start climbing or descending the stairs. And even if "
                "you've managed to shake them off, they'll still be there when "
                "you come back, though, so you'll want to use a different set "
                "of stairs when you return.";

        if (you.religion == GOD_TROG && !you.duration[DUR_BERSERKER]
            && !you.duration[DUR_EXHAUSTED]
            && you.hunger_state >= HS_SATIATED)
        {
            text << "\nAlso, with "
                 << apostrophise(god_name(you.religion))
                 << " support you can use your Berserk ability (<w>a</w>) "
                    "to temporarily gain more hitpoints and greater "
                    "strength. ";
        }
        break;

    case TUT_RETREAT_CASTER:
        text << "Without magical power you're unable to cast spells. While "
                "melee is a possibility, that's not where your strengths "
                "lie, so retreat (if possible) might be the better option.";

        if (_advise_use_wand())
            text << "\n\nOr you could <w>Z</w>ap a wand to deal damage.";
        break;

    case TUT_YOU_MUTATED:
        text << "Mutations can be obtained from several sources, among them "
                "potions, spell miscasts, and overuse of strong enchantments "
                "like Invisibility. The only reliable way to get rid of "
                "mutations is with potions of cure mutation. There are about "
                "as many harmful as beneficial mutations, and most of them "
                "have three levels. Check your mutations with <w>A</w>.";
        break;

    case TUT_NEW_ABILITY:
        switch(you.religion)
        {
        // Gods where first granted ability is active.
        case GOD_KIKUBAAQUDGHA: case GOD_YREDELEMNUL: case GOD_NEMELEX_XOBEH:
        case GOD_ZIN:           case GOD_OKAWARU:     case GOD_SIF_MUNA:
        case GOD_TROG:          case GOD_ELYVILON:    case GOD_LUGONU:
            text << "You just gained a new ability. Press <w>a</w> to "
                    "take a look at your abilities or to use one of them.";
            break;

        // Gods where first granted ability is passive.
        default:
            text << "You just gained a new ability. Press <w>^</w> "
#ifdef USE_TILE
                    "or press <w>Shift</w> and <w>right-click</w> on the "
                    "player tile "
#endif
                    "to take a look at your abilities.";
            break;
        }
        break;

    case TUT_CONVERT:
        _new_god_conduct();
        break;

    case TUT_GOD_DISPLEASED:
        text << "Uh-oh, " << god_name(you.religion) << " is growing "
                "displeased because your piety is running low. Possibly this "
                "is the case because you're committing heretic acts";

        if (!is_good_god(you.religion))
        {
            // Piety decreases over time for non-good gods.
            text << ", because " << god_name(you.religion) << " finds your "
                    "worship lacking, or a combination of the two";
        }
        text << ". If your piety goes to zero, then you'll be excommunicated. "
                "Better get cracking on raising your piety, and/or stop "
                "annoying your god. ";

        text << "In any case, you'd better reread the religious description. "
                "To do so, "
#ifdef USE_TILE
                "press <w>Shift</w> and <w>right-click</w> on your avatar.";
#else
                "type <w>^</w>.";
#endif
        break;

    case TUT_EXCOMMUNICATE:
    {
        const god_type new_god   = (god_type) gc.x;
        const int      old_piety = gc.y;

        god_type old_god = GOD_NO_GOD;
        for (int i = 0; i < MAX_NUM_GODS; i++)
            if (you.worshipped[i] > 0)
            {
                old_god = (god_type) i;
                break;
            }

        const std::string old_god_name  = god_name(old_god);
        const std::string new_god_name  = god_name(new_god);

        if (new_god == GOD_NO_GOD)
        {
            if (old_piety < 1)
            {
                text << "Uh-oh, " << old_god_name << " just excommunicated you "
                        "for running out of piety (your divine favour went "
                        "to nothing). Maybe you repeatedly violated the "
                        "religious rules, or maybe you failed to please your "
                        "deity often enough, or some combination of the two. "
                        "If you can find an altar dedicated to "
                     << old_god_name;
            }
            else
            {
                text << "Should you decide that abandoning " << old_god_name
                     << "wasn't such a smart move after all, and you'd like to "
                        "return to your old faith you'll have to find an altar "
                        "dedicated to " << old_god_name << " where";
            }
            text << " you can re-convert, and all will be well. Otherwise "
                    "you'll have to weather this god's displeasure until all "
                    "divine wrath is spent.";

        }
        else
        {
            bool angry = false;
            if (is_good_god(old_god))
            {
                if (is_good_god(new_god))
                {
                    text << "Fortunately, it seems that " << old_god_name <<
                            " didn't mind you converting to " << new_god_name
                         << ". ";

                    if (old_piety > 30)
                        text << "You even kept some of your piety! ";

                    text << "Note that this kind of alliance only exists "
                            "between the three good gods, so don't expect this "
                            "to be the norm.";
                }
                else if (!god_hates_your_god(old_god))
                {
                    text << "Fortunately, it seems that " << old_god_name <<
                            " didn't mind you converting to " << new_god_name
                         << ". That's because " << old_god_name << " is one of "
                            "the good gods who generally are rather forgiving "
                            "about change of faith - unless you switch over to "
                            "the path of evil, in which case their retribution "
                            "can be nasty indeed!";
                }
                else
                {
                    text << "Looks like " << old_god_name << " didn't "
                            "appreciate you converting to " << new_god_name
                         << "! But really, changing from one of the good gods "
                            "to an evil one, what did you expect!? For any god "
                            "not on the opposing side of the faith, "
                         << old_god_name << " would have been much more "
                            "forgiving. ";

                    angry = true;
                }
            }
            else
            {
                text << "Looks like " << old_god_name << " didn't appreciate "
                        "you converting to " << new_god_name << "! (Actually, "
                        "only the three good gods will sometimes be forgiving "
                        "about this kind of faithlessness.) ";

                angry = true;
            }

            if (angry)
            {
                text << "Unfortunately, while converting back would appease "
                     << old_god_name << ", it would annoy " << new_god_name
                     << ", so you're stuck with having to suffer the wrath of "
                        "one god or another.";
            }
        }

        break;
    }

    case TUT_WIELD_WEAPON:
    {
        int wpn = you.equip[EQ_WEAPON];
        if (wpn != -1
            && you.inv[wpn].base_type == OBJ_WEAPONS
            && item_cursed(you.inv[wpn]))
        {
            // Don't trigger if the wielded weapon is cursed.
            Options.tutorial_events[seen_what] = true;
            Options.tutorial_left++;
            return;
        }

        if (Options.tutorial_type == TUT_RANGER_CHAR && wpn != -1
            && you.inv[wpn].base_type == OBJ_WEAPONS
            && you.inv[wpn].sub_type == WPN_BOW)
        {
            text << "You can easily switch between weapons in slots a and "
                    "b by pressing <w>'</w>.";
        }
        else
        {
            text << "You can easily switch back to your weapon in slot a by "
                    "pressing <w>'</w>. To change the slot of an item, type "
                    "<w>=i</w> and choose the appropriate slots.";
        }
        break;
    }
    case TUT_FLEEING_MONSTER:
        if (Options.tutorial_type != TUT_BERSERK_CHAR)
            break;

        text << "Now that monster is scared of you! Note that you do not "
                "absolutely have to follow it. Rather, you can let it run "
                "away. Sometimes, though, it can be useful to attack a "
                "fleeing creature by throwing something after it. If you "
                "have any daggers or hand axes in your <w>i</w>nventory you "
                "can look at one of them to read an explanation of how to do "
                "this.";
        break;

    case TUT_MONSTER_BRAND:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        if (const monsters *m = monster_at(gc))
            tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_CAP_A), gc);
#endif
        text << "That monster looks a bit unusual. You might wish to examine "
                "it a bit more closely by "
#ifdef USE_TILE
                "hovering your mouse over its tile";
#else
                "pressing <w>x</w> and moving the cursor onto its square.";
#endif
        break;

    case TUT_MONSTER_FRIENDLY:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        if (const monsters *m = monster_at(gc))
            tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_CAP_A), gc);
#endif
        text << "That monster is friendly to you and will attack your "
                "enemies, though you'll get only half the experience for "
                "monsters killed by allies of what you'd get for killing them "
                "yourself. You can command your allies by pressing <w>t</w> "
                "to talk to them.";
        break;

    case TUT_SEEN_MONSTER:
    case TUT_SEEN_FIRST_OBJECT:
        // Handled in special functions.
        break;

    case TUT_ABYSS:
        text << "Uh-oh, you've wound up in the Abyss! The Abyss is a special "
                "place where you cannot remember or map where you've been; it "
                "is filled with nasty monsters, and you're probably going to "
                "die.\n";
        text << "To increase your chances of survival until you can find the "
                "exit"
#ifndef USE_TILE
                " (a flickering <w>\\</w>)"
#endif
                ", keep moving, don't fight any of the monsters, and don't "
                "bother picking up any items on the ground. If you're "
                "encumbered or overburdened then lighten up your load, and if "
                "the monsters are closing in, try to use items of speed to get "
                "away. Also, wherever possible, move in a direction slightly "
                "off from a compass direction (for example, north-by-northwest "
                "instead of north or northwest), as you're more likely to miss "
                "the exit if you keep heading solely in a compass direction.";
        break;

    case TUT_SPELL_MISCAST:
    {
        text << "You just miscast a spell. ";

        item_def *armour = you.slot_item(EQ_BODY_ARMOUR);
        item_def *shield = you.slot_item(EQ_SHIELD);
        if (armour && !is_light_armour(*armour) || shield)
        {
            text << "Wearing heavy body armour or using a shield, especially a "
                    "large one, can severely hamper your spellcasting "
                    "abilities. You can check the effect of this by comparing "
                    "the success rates on the <w>z\?</w> screen with and "
                    "without the item being worn.\n\n";
        }

        text << "If the spellcasting success chance is high (which can be "
                "checked by entering <w>z\?</w>) then a miscast merely means "
                "the spell is not working, along with a harmless side effect. "
                "However, for spells with a low success rate there's a chance "
                "of contaminating yourself with magical energy, plus a chance "
                "of an additional harmful side effect. Normally this isn't a "
                "problem, since magical contamination bleeds off over time, "
                "but if you're repeatedly contaminated in a short amount of "
                "time you'll mutate or suffer from other ill side effects.\n\n";

        text << "Note that a miscast spell will still consume the full amount "
                "of MP and nutrition that a successfully cast spell would.";
        break;
    }
    case TUT_SPELL_HUNGER:
        text << "The spell you just cast made you hungrier; you can see how "
                "hungry spells make you by entering <w>z\?!</w>. The amount of "
                "nutrition consumed increases with the level of the spell and "
                "decreases in dependence of your intelligence stat and your "
                "Spellcasting skill. If both of these are high enough a spell "
                "might even not cost you any nutrition at all.";
        break;

    case TUT_GLOWING:
        text << "Uh-oh, you've accumulated so much magical contamination that "
                "you're glowing! You usually acquire magical contamination "
                "from using some powerful magics, like invisibility, haste "
                "or potions of resistance. This normally isn't a problem as "
                "contamination slowly bleeds off on its own, but it seems that "
                "you've accumulated so much contamination over a short amount "
                "of time that it can have nasty effects, such as mutate you "
                "or deal direct damage. In addition, glowing is going to make "
                "you much more noticeable.";
        break;

    case TUT_YOU_RESIST:
        text << "There are many dangers in Crawl. Luckily, there are ways to "
                "(at least partially) resist some of them, if you are "
                "fortunate enough to find them. There are two basic variants "
                "of resistances: the innate magic resistance that depends on "
                "your species, grows with the experience level, and protects "
                "against hostile enchantments; and the specific resistances "
                "against certain types of magic and also other effects, e.g. "
                "fire or draining.\n"
                "You can find items in the dungeon or gain mutations that will "
                "increase (or lower) one or more of your resistances. To view "
                "your current set of resistances, "
#ifdef USE_TILE
                "<w>right-click</w> on the player avatar.";
#else
                "press <w>%</w>.";
#endif
        break;

    case TUT_CAUGHT_IN_NET:
        text << "While you are held in a net, you cannot move around or engage "
                "monsters in combat. Instead, any movement you take is counted "
                "as an attempt to struggle free from the net. With a wielded "
                "bladed weapon you will be able to cut the net faster";

        if (Options.tutorial_type == TUT_BERSERK_CHAR)
            text << ", especially if you're berserking while doing so";

        text << ". Small races may also wriggle out of a net, only damaging it "
                "a bit, so as to then <w>f</w>ire it at a monster.";

        if (Options.tutorial_type == TUT_MAGIC_CHAR)
            text << " Note that casting spells is still very much possible.";
        break;

    case TUT_LOAD_SAVED_GAME:
    {
        text << "Welcome back! If it's been a while since you last played this "
                "character, you should take some time to refresh your memory "
                "of your character's progress. It is recommended to at least "
                "have a look through your <w>i</w>nventory, but you should "
                "also check ";

        std::vector<std::string> listed;
        if (!your_talents(false).empty())
            listed.push_back("your <w>a</w>bilities");
        if (Options.tutorial_type != TUT_MAGIC_CHAR || how_mutated())
            listed.push_back("your set of mutations (<w>A</w>)");
        if (you.religion != GOD_NO_GOD)
            listed.push_back("your religious standing (<w>^</w>)");

        listed.push_back("the message history (<w>Ctrl-P</w>)");
        listed.push_back("the character overview screen (<w>%</w>)");
        text << comma_separated_line(listed.begin(), listed.end()) << ".";

        text << "\nAlternatively, you can dump all information pertaining to "
                "your character into a text file with the <w>#</w> command. "
                "You can then find said file in the <w>morgue/</w> folder (<w>"
             << you.your_name << ".txt</w>) and read it at your leisure. Also, "
                "such a file will automatically be created upon death (the "
                "filename will then also contain the date) but that won't be "
                "of much use to you now.";
        break;
    }
    default:
        text << "You've found something new (but I don't know what)!";
    }

    if (!text.str().empty())
    {
        formatted_message_history(text.str(), MSGCH_TUTORIAL, 0,
                                  _get_tutorial_cols());
        stop_running();
    }
}

formatted_string tut_abilities_info()
{
    std::ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    std::string broken = "This screen shows your character's set of talents. "
        "You can gain new abilities via certain items, through religion or by "
        "way of mutations. Activation of an ability usually comes at a cost, "
        "e.g. nutrition or Magic power. Press '<w>!</w>' or '<w>?</w>' to "
        "toggle between ability selection and description.";
    linebreak_string2(broken, _get_tutorial_cols());
    text << broken;

    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return formatted_string::parse_string(text.str(), false);
}

// Explains the basics of the skill screen. Don't bother the player with the
// aptitude information. (Toggling is still possible, of course.)
void print_tut_skills_info()
{
    textcolor(channel_to_colour(MSGCH_TUTORIAL));
    std::ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    std::string broken = "This screen shows the skill set of your character. "
        "The number next to the skill is your current level, the higher the "
        "better. The <cyan>cyan percent value</cyan> shows your progress "
        "towards the next skill level. You can toggle which skills to train by "
        "pressing their slot letters. A <darkgrey>greyish</darkgrey> skill "
        "will increase at a decidedly slower rate and ease training of others. "
        "Press <w>?</w> to read your skills' descriptions.";
    linebreak_string2(broken, _get_tutorial_cols());
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    formatted_string::parse_string(text.str(), false).display();
}

void print_tut_skills_description_info()
{
    textcolor(channel_to_colour(MSGCH_TUTORIAL));
    std::ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    std::string broken = "This screen shows the skill set of your character. "
                         "Press the letter of a skill to read its description, "
                         "or press <w>?</w> again to return to the skill "
                         "selection.";

    linebreak_string2(broken, _get_tutorial_cols());
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    formatted_string::parse_string(text.str(), false).display();
}

// A short explanation of Crawl's target mode and its most important commands.
static std::string _tut_target_mode(bool spells = false)
{
   std::string result;
   result = "then be taken to target mode with the nearest monster or previous "
            "target already targeted. You can also cycle through all hostile "
            "monsters in sight with <w>+</w> or <w>-</w>. "
            "Once you're aiming at the correct monster, simply hit "
            "<w>f</w>, <w>Enter</w> or <w>.</w> to shoot at it. "
            "If you miss, <w>";

   if (spells)
       result += "zap";
   else
       result += "ff";

   result += "</w> fires at the same target again.";

   return (result);
}

static std::string _tut_abilities()
{
   return ("To do this, enter the ability menu with <w>a</w>, and then "
           "choose the corresponding ability. Note that such an attempt of "
           "activation, especially by the untrained, is likely to fail.");
}

static std::string _tut_throw_stuff(const item_def &item)
{
    std::string result;

    result  = "To do this, type <w>f</w> to fire, then <w>";
    result += item.slot;
    result += "</w> for ";
    result += (item.quantity > 1 ? "these" : "this");
    result += " ";
    result += item_base_name(item);
    result += (item.quantity > 1? "s" : "");
    result += ". You'll ";
    result += _tut_target_mode();

    return (result);
}

// Explains the most important commands necessary to use an item, and mentions
// special effects, etc.
// NOTE: For identified artefacts don't give all this information!
//       (The screen is likely to overflow.) Artefacts need special information
//       if they are evokable or grant resistances.
//       In any case, check whether we still have enough space for the
//       inscription prompt and answer.
void tutorial_describe_item(const item_def &item)
{
    std::ostringstream ostr;
    ostr << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    switch (item.base_type)
    {
       case OBJ_WEAPONS:
       {
            if (is_artefact(item) && item_type_known(item))
            {
                if (gives_ability(item)
                    && wherey() <= get_number_of_lines() - 5)
                {
                    // You can activate it.
                    ostr << "When wielded, some weapons (such as this one) "
                            "offer certain abilities you can activate. ";
                    ostr << _tut_abilities();
                    break;
                }
                else if (gives_resistance(item)
                         && wherey() <= get_number_of_lines() - 3)
                {

                    // It grants a resistance.
                    ostr << "\nThis weapon offers its wearer protection from "
                            "certain sources. For an overview of your "
                            "resistances (among other things) type <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";
                    break;
                }
                return;
            }

            item_def *weap = you.slot_item(EQ_WEAPON);
            bool wielded = (weap && (*weap).slot == item.slot);
            bool long_text = false;

            if (!wielded)
            {
               ostr << "You can wield this weapon with <w>w</w>, or use "
                       "<w>'</w> to switch between the weapons in slot "
                       "a and b. (Use <w>=</w> to adjust item slots.)";

               // Weapon skill used by this weapon and the best weapon skill.
               int curr_wpskill, best_wpskill;

               // Maybe this is a launching weapon?
               if (is_range_weapon(item))
               {
                   // Then only compare with other launcher skills.
                   curr_wpskill = range_skill(item);
                   best_wpskill = best_skill(SK_SLINGS, SK_DARTS, 99);
               }
               else
               {
                   // Compare with other melee weapons.
                   curr_wpskill = weapon_skill(item);
                   best_wpskill = best_skill(SK_SHORT_BLADES, SK_STAVES, 99);
                   // Maybe unarmed is better.
                   if (you.skills[SK_UNARMED_COMBAT] > you.skills[best_wpskill])
                       best_wpskill = SK_UNARMED_COMBAT;
               }

               if (you.skills[curr_wpskill] + 2 < you.skills[best_wpskill])
               {
                    ostr << "\nOn second look, you've been training in <w>"
                         << skill_name(best_wpskill)
                         << "</w> for a while, so maybe you should "
                            "continue training that rather than <w>"
                         << skill_name(curr_wpskill)
                         << "</w>. (Type <w>m</w> to see the skill "
                            "management screen for the actual numbers.)";

                    long_text = true;
               }
            }
            else // wielded weapon
            {
                if (is_range_weapon(item))
                {
                    ostr << "To attack a monster, you only need to "
                            "<w>f</w>ire the appropriate type of ammunition. "
                            "You'll ";
                    ostr << _tut_target_mode();
                }
                else
                {
                    ostr << "To attack a monster, you can simply walk into it.";
                }
            }

            if (is_throwable(&you, item) && !long_text)
            {
                ostr << "\n\nSome weapons (including this one), can also be "
                        "<w>f</w>ired. ";
                ostr << _tut_throw_stuff(item);
                long_text = true;
            }
            if (!item_type_known(item) &&
                (is_artefact(item) || get_equip_desc( item ) != ISFLAG_NO_DESC))
            {
                ostr << "\n\nWeapons and armour that have unusual descriptions "
                     << "like this are much more likely to be of higher "
                     << "enchantment or have special properties, good or bad. "
                     << "The rarer the description, the greater the potential "
                     << "value of an item.";

                Options.tutorial_events[TUT_SEEN_RANDART] = false;
            }
            if (item_known_cursed( item ) && !long_text)
            {
                ostr << "\n\nOnce wielded, a cursed weapon won't leave your "
                        "hands again until the curse has been lifted by "
                        "reading a scroll of remove curse or one of the "
                        "enchantment scrolls.";

                if (!wielded && is_throwable(&you, item))
                    ostr << " (Throwing it is safe, though.)";

                Options.tutorial_events[TUT_YOU_CURSED] = false;
            }
            Options.tutorial_events[TUT_SEEN_WEAPON] = false;
            break;
       }
       case OBJ_MISSILES:
            if (is_throwable(&you, item))
            {
                ostr << item.name(DESC_CAP_YOUR)
                     << " can be <w>f</w>ired without the use of a launcher. ";
                ostr << _tut_throw_stuff(item);
            }
            else if (is_launched(&you, you.weapon(), item))
            {
                ostr << "As you're already wielding the appropriate launcher, "
                        "you can simply <w>f</w>ire "
                     << (item.quantity > 1 ? "these" : "this")
                     << " " << item.name(DESC_BASENAME)
                     << (item.quantity > 1? "s" : "")
                     << ". You'll ";
                ostr << _tut_target_mode();
            }
            else
            {
                ostr << "To shoot "
                     << (item.quantity > 1 ? "these" : "this")
                     << " " << item.name(DESC_BASENAME)
                     << (item.quantity > 1? "s" : "")
                     << ", first you need to <w>w</w>ield an appropriate "
                        "launcher.";
            }
            Options.tutorial_events[TUT_SEEN_MISSILES] = false;
            break;

       case OBJ_ARMOUR:
       {
            bool wearable = true;
            if (you.species == SP_CENTAUR && item.sub_type == ARM_BOOTS)
            {
                ostr << "As a Centaur you cannot wear boots. "
                        "(Type <w>A</w> to see a list of your mutations and "
                        "innate abilities.)";
                wearable = false;
            }
            else if (you.species == SP_MINOTAUR && is_hard_helmet(item))
            {
                ostr << "As a Minotaur you cannot wear helmets. "
                        "(Type <w>A</w> to see a list of your mutations and "
                        "innate abilities.)";
                wearable = false;
            }
            else
            {
                ostr << "You can wear pieces of armour with <w>W</w> and take "
                        "them off again with <w>T</w>"
#ifdef USE_TILE
                        ", or, alternatively, simply click on their tiles to "
                        "perform either action."
#endif
                        ".";
            }

            if (Options.tutorial_type == TUT_MAGIC_CHAR
                && !is_light_armour(item))
            {
                ostr << "\nNote that body armour with high evasion penalties "
                        "may hinder your ability to learn and cast spells. "
                        "Light armour such as robes, leather armour or any "
                        "elven armour will be generally safe for any aspiring "
                        "spellcaster.";
            }
            else if (Options.tutorial_type == TUT_RANGER_CHAR
                     && is_shield(item))
            {
                ostr << "\nNote that wearing a shield will greatly decrease "
                        "the speed at which you can shoot arrows.";
            }

            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc( item ) != ISFLAG_NO_DESC))
            {
                ostr << "\n\nWeapons and armour that have unusual descriptions "
                     << "like this are much more likely to be of higher "
                     << "enchantment or have special properties, good or bad. "
                     << "The rarer the description, the greater the potential "
                     << "value of an item.";

                Options.tutorial_events[TUT_SEEN_RANDART] = false;
            }
            if (wearable)
            {
                if (item_known_cursed( item ))
                {
                    ostr << "\nA cursed piece of armour, once worn, cannot be "
                            "removed again until the curse has been lifted by "
                            "reading a scroll of remove curse or enchant "
                            "armour.";
                }
                if (gives_resistance(item))
                {
                    ostr << "\n\nThis armour offers its wearer protection from "
                            "certain sources. For an overview of your"
                            " resistances (among other things) type <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";
                }
                if (is_artefact(item) && gives_ability(item))
                {
                    ostr << "\nWhen worn, some types of armour (such as this "
                            "one) offer certain abilities you can activate. ";
                    ostr << _tut_abilities();
                }
            }
            Options.tutorial_events[TUT_SEEN_ARMOUR] = false;
            break;
       }
       case OBJ_WANDS:
            ostr << "The magic within can be unleashed by <w>Z</w>apping it.";
#ifdef USE_TILE
            ostr << " Alternatively, simply click on its tile.";
#endif
            Options.tutorial_events[TUT_SEEN_WAND] = false;
            break;

       case OBJ_FOOD:
            ostr << "Food can simply be <w>e</w>aten"
#ifdef USE_TILE
                    ", something you can also do by <w>left clicking</w> on it"
#endif
                    ". ";

            if (item.sub_type == FOOD_CHUNK)
            {
                ostr << "Note that most species refuse to eat raw meat unless "
                        "really hungry. ";
                if (food_is_rotten(item))
                {
                    ostr << "Even fewer can safely digest rotten meat, and "
                            "you're probably not part of that group.";
                }
            }
            Options.tutorial_events[TUT_SEEN_FOOD] = false;
            break;

       case OBJ_SCROLLS:
            ostr << "Type <w>r</w> to read this scroll"
#ifdef USE_TILE
                    "or simply click on it with your <w>left mouse button</w>"
#endif
                    ".";

            Options.tutorial_events[TUT_SEEN_SCROLL] = false;
            break;

       case OBJ_JEWELLERY:
       {
            ostr << "Jewellery can be <w>P</w>ut on or <w>R</w>emoved "
                    "again"
#ifdef USE_TILE
                    ", though in Tiles, either can be done by clicking on the "
                    "item in your inventory"
#endif
                    ".";

            if (item_known_cursed( item ))
            {
                ostr << "\nA cursed piece of jewellery will cling to its "
                        "unfortunate wearer's neck or fingers until the curse "
                        "is finally lifted when he or she reads a scroll of "
                        "remove curse.";
            }
            if (gives_resistance(item))
            {
                    ostr << "\n\nThis "
                         << (item.sub_type < NUM_RINGS ? "ring" : "amulet")
                         << " offers its wearer protection "
                            "from certain sources. For an overview of your "
                            "resistances (among other things) type <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";
            }
            if (gives_ability(item))
            {
                ostr << "\n\nWhen worn, some types of jewellery (such as this "
                        "one) offer certain abilities you can activate. ";
                ostr << _tut_abilities();
            }
            Options.tutorial_events[TUT_SEEN_JEWELLERY] = false;
            break;
       }
       case OBJ_POTIONS:
            ostr << "Type <w>q</w> to quaff this potion"
#ifdef USE_TILE
                    "or simply click on it with your <w>left mouse button</w>"
#endif
                    ".";
            Options.tutorial_events[TUT_SEEN_POTION] = false;
            break;

       case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL)
            {
                ostr << "A manual can greatly help you in training a skill. "
                        "To use it, <w>r</w>ead it while your experience "
                        "pool (the number in brackets) is full. Note that "
                        "this will drain said pool, so only use this manual "
                        "if you think you need the skill in question.";
            }
            else // It's a spellbook!
            {
                if (you.religion == GOD_TROG
                    && (item.sub_type != BOOK_DESTRUCTION
                        || !item_ident(item, ISFLAG_KNOW_TYPE)))
                {
                    if (!item_ident(item, ISFLAG_KNOW_TYPE))
                        ostr << "It's a book, you can <w>r</w>ead it.";
                    else
                    {
                        ostr << "A spellbook! You could <w>M</w>emorise some "
                                "spells and then cast them with <w>z</w>.";
                    }
                    ostr << "\nAs a worshipper of "
                         << god_name(GOD_TROG)
                         << ", though, you might instead wish to burn this "
                            "tome of hated magic by using the corresponding "
                            "<w>a</w>bility. "
                            "Note that this only works on books that are lying "
                            "on the floor and not on your current square. ";
                }
                else if (!item_ident(item, ISFLAG_KNOW_TYPE))
                {
                    ostr << "It's a book, you can <w>r</w>ead it"
#ifdef USE_TILE
                            ", something that can also be achieved by clicking "
                            "on its tile in your inventory."
#endif
                            ".";
                }
                else if (item.sub_type == BOOK_DESTRUCTION)
                {
                    ostr << "This magical item can cause great destruction "
                            "- to you, or your surroundings. Use with care!";
                }
                else if (!you.skills[SK_SPELLCASTING])
                {
                    ostr << "A spellbook! You could <w>M</w>emorise some "
                            "spells and then cast them with <w>z</w>. ";

                    ostr << "\nFor now, however, that will have to wait until "
                            "you've learned the basics of Spellcasting by "
                            "reading lots of scrolls.";
                }
                else // You actually can cast spells.
                {
                    if (player_can_memorise(item))
                    {
                        ostr << "Such a <lightblue>highlighted "
                                "spell</lightblue> can be <w>M</w>emorised "
                                "right away. ";
                    }
                    else
                    {
                        ostr << "You cannot memorise any "
                             << (you.spell_no ? "more " : "")
                             << "spells right now. This will change as you "
                                "grow in levels and Spellcasting proficiency. ";
                    }

                    if (you.spell_no)
                    {
                        ostr << "\n\nTo do magic, type <w>z</w> and choose a "
                                "spell, e.g. <w>a</w> (check with <w>?</w>). "
                                "For attack spells you'll ";
                        ostr << _tut_target_mode(true);
                    }
                }
            }
            ostr << "\n";
            Options.tutorial_events[TUT_SEEN_SPBOOK] = false;
            break;

       case OBJ_CORPSES:
            ostr << "Corpses lying on the floor can be <w>c</w>hopped up with "
                    "a sharp implement to produce chunks for food (though they "
                    "may not be healthy)";

            if (god_likes_butchery(you.religion))
            {
                ostr << ", or as a sacrifice to "
                     << god_name(you.religion)
                     << " (while <w>p</w>raying)";
            }
            ostr << ". ";

            if (food_is_rotten(item))
            {
                ostr << "Rotten corpses won't be of any use to you, though, so "
                        "you might just as well <w>d</w>rop this. No god will "
                        "accept such rotten sacrifice, either.";
            }
            else
            {
#ifdef USE_TILE
                ostr << " For an individual corpse in your inventory, the most "
                        "practical way to chop it up is to drop it by clicking "
                        "on it with your <w>left mouse button</w> while "
                        "<w>Shift</w> is pressed, and then repeat that command "
                        "for the corpse tile now lying on the floor. If the "
                        "intent is to eat the chunks (rather than offer the "
                        "corpse), you can then press <w>Shift + right mouse "
                        "button</w> to do that.\n"
                        EOL;
#endif
                ostr << "If there are several items in your inventory you'd "
                        "like to drop, the most convenient way is to use the "
                        "<w>d</w>rop menu. On a related note, butchering "
                        "several corpses on a floor square is facilitated by "
                        "using the <w>c</w>hop prompt where <w>c</w> is a "
                        "valid synonym for <w>y</w>es or you can directly chop "
                        "<w>a</w>ll corpses.";
            }
            Options.tutorial_events[TUT_SEEN_CARRION] = false;
            break;

       case OBJ_STAVES:
            if (item_is_rod( item ))
            {
                if (!item_ident(item, ISFLAG_KNOW_TYPE))
                {
                    ostr << "\n\nTo find out what this rod might do, you have "
                            "to <w>w</w>ield and e<w>v</w>oke it to see if you "
                            "can use the spells hidden within"
#ifdef USE_TILE
                            ", both of which can be done by clicking on it"
#endif
                            ".";
                }
                else
                {
                    ostr << "\n\nYou can use this rod's magic by "
                            "<w>w</w>ielding and e<w>v</w>oking it"
#ifdef USE_TILE
                            ", both of which can be achieved by clicking on it"
#endif
                            ".";
                }
            }
            else
            {
                ostr << "This staff can enhance your spellcasting of specific "
                        "spell schools. ";

                bool gives_resist = false;
                if (you.spell_no && !item_ident(item, ISFLAG_KNOW_TYPE))
                {
                    ostr << "You can find out which one by casting spells "
                            "while wielding this staff. Eventually, the staff "
                            "might react to a spell of its school and identify "
                            "itself.";
                }
                else if (gives_resistance(item))
                {
                    ostr << "It also offers its wielder protection from "
                            "certain sources. For an overview of your "
                            "resistances (among other things) type <w>%</w>"
#ifdef USE_TILE
                            " or click on your avatar with the <w>right mouse "
                            "button</w>"
#endif
                            ".";

                    gives_resist = true;
                }

                if (!gives_resist && you.religion == GOD_TROG)
                {
                    ostr << "\n\nSeeing how "
                         << god_name(GOD_TROG, false)
                         << " frowns upon the use of magic, this staff will be "
                            "of little use to you and you might just as well "
                            "<w>d</w>rop it now.";
                }
            }
            Options.tutorial_events[TUT_SEEN_STAFF] = false;
            break;

       case OBJ_MISCELLANY:
            ostr << "Miscellaneous items sometimes harbour magical powers. Try "
                    "<w>w</w>ielding and e<w>v</w>oking it"
#ifdef USE_TILE
                    ", either of which can be done by clicking on it"
#endif
                    ".";

            Options.tutorial_events[TUT_SEEN_MISC] = false;
            break;

       default:
            return;
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    std::string broken = ostr.str();
    linebreak_string2(broken, _get_tutorial_cols());
    cgotoxy(1, wherey() + 2);
    formatted_string::parse_block(broken, false).display();
}        // tutorial_describe_item()

void tutorial_inscription_info(bool autoinscribe, std::string prompt)
{
    // Don't print anything if there's not enough space.
    if (wherey() >= get_number_of_lines() - 1)
        return;

    std::ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    bool longtext = false;
    if (wherey() <= get_number_of_lines() - (autoinscribe ? 10 : 8))
    {
        text << EOL
         "Inscriptions are a powerful concept of Dungeon Crawl." EOL
         "You can inscribe items to differentiate them, or to comment on them, " EOL
         "but also to set rules for item interaction. If you are new to Crawl, " EOL
         "you can safely ignore this feature, though.";

        longtext = true;
    }

    if (autoinscribe && wherey() <= get_number_of_lines() - 6)
    {
        text << EOL
         "Artefacts can be autoinscribed to give a brief overview of their " EOL
         "known properties.";

        longtext = true;
    }
    text << EOL
       "(In the main screen, press <w>?6</w> for more information.)" EOL;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    formatted_string::parse_string(text.str()).display();

    // Ask a second time, if it's been a longish interruption.
    if (longtext && !prompt.empty() && wherey() <= get_number_of_lines() - 2)
        formatted_string::parse_string(prompt).display();
}

bool tutorial_pos_interesting(int x, int y)
{
    return (cloud_type_at(coord_def(x, y)) != CLOUD_NONE
            || _water_is_disturbed(x, y)
            || tutorial_feat_interesting(grd[x][y]));
}

bool tutorial_feat_interesting(dungeon_feature_type feat)
{
    // Altars and branch entrances are always interesting.
    if (feat >= DNGN_ALTAR_FIRST_GOD && feat <= DNGN_ALTAR_LAST_GOD)
        return (true);
    if (feat >= DNGN_ENTER_FIRST_BRANCH && feat <= DNGN_ENTER_LAST_BRANCH)
        return (true);

    switch (feat)
    {
    // So are statues, traps, and stairs.
    case DNGN_ORCISH_IDOL:
    case DNGN_GRANITE_STATUE:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_NATURAL:
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ESCAPE_HATCH_UP:
        return (true);
    default:
        return (false);
    }
}

void tutorial_describe_pos(int x, int y)
{
    _tutorial_describe_disturbance(x, y);
    _tutorial_describe_cloud(x, y);
    tutorial_describe_feature(grd[x][y]);
}

void tutorial_describe_feature(dungeon_feature_type feat)
{
    std::ostringstream ostr;
    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    switch (feat)
    {
       case DNGN_ORCISH_IDOL:
       case DNGN_GRANITE_STATUE:
            ostr << "It's just a harmless statue - or is it?\nEven if not "
                    "a danger by themselves, statues often mark special "
                    "areas, dangerous ones or ones harbouring treasure.";
            break;

       case DNGN_TRAP_MAGICAL:
       case DNGN_TRAP_MECHANICAL:
            ostr << "These nasty constructions can do physical damage (with "
                    "darts or needles, for example) or have other, more "
                    "magical effects. ";

            if (feat == DNGN_TRAP_MECHANICAL)
            {
                ostr << "You can attempt to deactivate the mechanical type by "
                        "standing next to it and then pressing <w>Ctrl</w> "
                        "and the direction of the trap. Note that this usually "
                        "causes the trap to go off, so it can be quite a "
                        "dangerous task.\n\n"

                        "You can safely pass over a mechanical trap if "
                        "you're flying or levitating.";
            }
            else
            {
                ostr << "Magical traps can't be disarmed, and unlike "
                        "mechanical traps you can't avoid tripping them "
                        "by levitating or flying over them.";
            }
            Options.tutorial_events[TUT_SEEN_TRAP] = false;
            break;

       case DNGN_TRAP_NATURAL: // only shafts for now
            ostr << "The dungeon contains a number of natural obstacles such "
                    "as shafts, which lead one to three levels down. They "
                    "can't be disarmed, but you can safely pass over them "
                    "if you're levitating or flying.";
            Options.tutorial_events[TUT_SEEN_TRAP] = false;
            break;

       case DNGN_STONE_STAIRS_DOWN_I:
       case DNGN_STONE_STAIRS_DOWN_II:
       case DNGN_STONE_STAIRS_DOWN_III:
            ostr << "You can enter the next (deeper) level by following them "
                    "down (<w>></w>). To get back to this level again, "
                    "press <w><<</w> while standing on the upstairs.";
#ifdef USE_TILE
            ostr << " In Tiles, you can achieve the same, in either direction, "
                    "by clicking the <w>left mouse button</w> while pressing "
                    "<w>Shift</w>. ";
#endif
            Options.tutorial_events[TUT_SEEN_STAIRS] = false;
            break;

       case DNGN_STONE_STAIRS_UP_I:
       case DNGN_STONE_STAIRS_UP_II:
       case DNGN_STONE_STAIRS_UP_III:
            if (you.your_level < 1)
            {
                ostr << "These stairs lead out of the dungeon. Following them "
                        "will end the game. The only way to win is to "
                        "transport the fabled orb of Zot outside.";
            }
            else
            {
                ostr << "You can enter the previous (shallower) level by "
                        "following these up (<w><<</w>). This is ideal for "
                        "retreating or finding a safe resting spot, since the "
                        "previous level will have less monsters and monsters "
                        "on this level can't follow you up unless they're "
                        "standing right next to you. To get back to this "
                        "level again, press <w>></w> while standing on the "
                        "downstairs.";
#ifdef USE_TILE
                ostr << " In Tiles, you can perform either action simply by "
                        "clicking the <w>left mouse button</w> while pressing "
                        "<w>Shift</w> instead. ";
#endif
            }
            Options.tutorial_events[TUT_SEEN_STAIRS] = false;
            break;

       case DNGN_ESCAPE_HATCH_DOWN:
       case DNGN_ESCAPE_HATCH_UP:
            ostr << "Escape hatches can be used to quickly leave a level with "
                    "<w><<</w> and <w>></w>, respectively. Note that you will "
                    "usually be unable to return right away.";

            Options.tutorial_events[TUT_SEEN_ESCAPE_HATCH] = false;
            break;

       default:
            if (feat >= DNGN_ALTAR_FIRST_GOD && feat <= DNGN_ALTAR_LAST_GOD)
            {
                god_type altar_god = grid_altar_god(feat);

                if (you.religion == GOD_NO_GOD)
                {
                ostr << "This is your chance to join a religion! In general, "
                        "the gods will help their followers, bestowing powers "
                        "of all sorts upon them, but many of them demand a "
                        "life of dedication, constant tributes or "
                        "entertainment in return. \nYou can get information "
                        "about <w>"
                     << god_name(altar_god)
                     << "</w> by pressing <w>p</w> while standing on the "
                        "altar. Before taking up the responding faith you'll "
                        "be asked for confirmation.";
                }
                else if (you.religion == altar_god)
                {
                    if (god_likes_items(you.religion))
                    {
                        ostr << "If "
                             << god_name(you.religion)
                             << " likes to have certain items or corpses "
                                "sacrificed on altars, any appropriate item "
                                "<w>d</w>ropped on an altar during prayer, or "
                                "already lying on an altar when you start "
                                "<w>p</w>raying will be automatically "
                                "sacrificed to "
                             << god_name(you.religion)
                             << ".";
                    }
                }
                else
                {
                    ostr << god_name(you.religion)
                         << " probably won't like it if you switch allegiance, "
                            "but having a look won't hurt: to get information "
                            "on <w>";
                    ostr << god_name(altar_god);
                    ostr << "</w>, press <w>p</w> while standing on the "
                            "altar. Before taking up the responding faith (and "
                            "abandoning your current one!) you'll be asked for "
                            "confirmation."
                            "\nTo see your current standing with "
                         << god_name(you.religion)
                         << " press <w>^</w>"
#ifdef USE_TILE
                            ", or press <w>Shift</w> while clicking with "
                            "your <w>right mouse button</w> on your avatar"
#endif
                            ".";
                }
                Options.tutorial_events[TUT_SEEN_ALTAR] = false;
                break;
            }
            else if (feat >= DNGN_ENTER_FIRST_BRANCH
                     && feat <= DNGN_ENTER_LAST_BRANCH)
            {
                ostr << "An entryway into one of the many dungeon branches in "
                        "Crawl. ";
                if (feat != DNGN_ENTER_TEMPLE)
                    ostr << "Beware, sometimes these can be deadly!";
                break;
            }
            else
                return;
    }
    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    std::string broken = ostr.str();
    linebreak_string2(broken, _get_tutorial_cols());
    formatted_string::parse_block(broken, false).display();
}

static void _tutorial_describe_cloud(int x, int y)
{
    cloud_type ctype = cloud_type_at(coord_def(x, y));
    if (ctype == CLOUD_NONE)
        return;

    std::string cname = cloud_name(ctype);

    std::ostringstream ostr;

    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    ostr << "The " << cname << " ";

    if (ends_with(cname, "s"))
        ostr << "are ";
    else
        ostr << "is ";

    bool need_cloud = false;
    switch(ctype)
    {
    case CLOUD_BLACK_SMOKE:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_PURP_SMOKE:
    case CLOUD_MIST:
        ostr << "harmless. ";
        break;

    default:
        if (!is_damaging_cloud(ctype, true))
        {
            ostr << "currently harmless, but that could change at some point. "
                    "Check the overview screen (<w>%</w>) to view your "
                    "resistances.";
            need_cloud = true;
        }
        else
        {
            ostr << "probably dangerous, and you should stay out of it if you "
                    "can. ";
        }
    }

    if (is_opaque_cloud(env.cgrid[x][y]))
    {
        ostr << (need_cloud? "\nThis cloud" : "It")
             << " is opaque. If two or more opaque clouds are between "
                "you and a square you won't be able to see anything in that "
                "square.";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    std::string broken = ostr.str();
    linebreak_string2(broken, _get_tutorial_cols());
    formatted_string::parse_block(broken, false).display();
}

static void _tutorial_describe_disturbance(int x, int y)
{
    if (!_water_is_disturbed(x, y))
        return;

    std::ostringstream ostr;

    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    ostr << "The strange disturbance means that there's a monster hiding "
            "under the surface of the shallow water. Other than non-submerged "
            "monsters, a submerged monster will not be autotargeted when doing "
            "a ranged attack while there are other, visible targets in sight. "
            "Of course you can still target it manually if you wish to.";

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    std::string broken = ostr.str();
    linebreak_string2(broken, _get_tutorial_cols());
    formatted_string::parse_block(broken, false).display();
}

static bool _water_is_disturbed(int x, int y)
{
    const coord_def c(x,y);
    const monsters *mon = monster_at(c);

    if (mon == NULL || grd(c) != DNGN_SHALLOW_WATER || !see_grid(c))
        return (false);

    return (!player_monster_visible(mon) && !mons_flies(mon));
}

bool tutorial_monster_interesting(const monsters *mons)
{
    if (mons_is_unique(mons->type) || mons->type == MONS_PLAYER_GHOST)
        return (true);

    // Highlighted in some way.
    if (_mons_is_highlighted(mons))
        return (true);

    // The monster is (seriously) out of depth.
    if (you.level_type == LEVEL_DUNGEON
        && mons_level(mons->type) >= you.your_level + 8)
    {
        return (true);
    }
    return (false);
}

void tutorial_describe_monster(const monsters *mons)
{
    std::ostringstream ostr;
    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    bool dangerous = false;
    if (mons_is_unique(mons->type))
    {
        ostr << "Did you think you were the only adventurer in the dungeon? "
                "Well, you thought wrong! These unique adversaries often "
                "possess skills that normal monster wouldn't, so be "
                "careful.\n\n";
        dangerous = true;
    }
    else if (mons->type == MONS_PLAYER_GHOST)
    {
        ostr << "The ghost of a deceased adventurer, it would like nothing "
                "better than to send you the same way.\n\n";
        dangerous = true;
    }
    else
    {
        // 8 is the default value for the note-taking of OOD monsters.
        // Since I'm too lazy to come up with any measurement of my own
        // I'll simply reuse that one.
        int level_diff = mons_level(mons->type) - (you.your_level + 8);

        if (you.level_type == LEVEL_DUNGEON && level_diff >= 0)
        {
            ostr << "This kind of monster is usually only encountered "
                 << (level_diff > 5 ? "much " : "")
                 << "deeper in the dungeon, so it's probably "
                 << (level_diff > 5 ? "extremely" : "very")
                 << " dangerous!\n\n";
            dangerous = true;
        }
    }

    if (mons->has_ench(ENCH_BERSERK))
    {
        ostr << "A berserking monster is bloodthirsty and fighting madly. "
                "Such a blood rage makes it particularly dangerous!\n\n";
        dangerous = true;
    }

    // Monster is highlighted.
    if (mons_friendly(mons))
    {
        ostr << "Friendly monsters will follow you around and attempt to aid "
                "you in battle. You can order your allies by <w>t</w>alking "
                "to them.";
    }
    else if (dangerous)
    {
        if (!Options.tut_explored && mons->foe != MHITYOU)
        {
            ostr << "You can easily mark its square as dangerous to avoid "
                    "accidentally entering into its field of view when using "
                    "auto-explore or auto-travel. To do so, enter the level "
                    "map with <w>X</w> and then press <w>Ctrl-X</w> when your "
                    "cursor is hovering over the monster's grid. Doing so will "
                    "mark this grid and all surrounding ones within a radius "
                    "of 8 as \"excluded\" ones that explore or travel modus "
                    "won't enter.\n";
#ifdef USE_TILE
            ostr << "Upon returning to the main map, you'll even find all "
                    "surrounding grids visibly highlighted.";
#endif
        }
        else
        {
            ostr << "This might be a good time to run away";

            if (you.religion == GOD_TROG && !you.duration[DUR_BERSERKER]
                && !you.duration[DUR_EXHAUSTED]
                && you.hunger_state >= HS_SATIATED)
            {
                ostr << " or apply your Berserk <w>a</w>bility";
            }
            ostr << ".";
        }
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mons_looks_stabbable(mons))
    {
        ostr << "Apparently "
             << mons_pronoun((monster_type) mons->type, PRONOUN_NOCAP)
             << " has not noticed you - yet. Note that you do not have to "
                "engage every monster you meet. Sometimes, discretion is the "
                "better part of valour.";
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mons_looks_distracted(mons))
    {
        ostr << "Apparently "
             << mons_pronoun((monster_type) mons->type, PRONOUN_NOCAP)
             << " has been distracted by something. You could use this "
                "opportunity to sneak up on this monster - or to sneak away.";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    std::string broken = ostr.str();
    linebreak_string2(broken, _get_tutorial_cols());
    formatted_string::parse_block(broken, false).display();
}

void tutorial_observe_cell(const coord_def& gc)
{
    if (grid_is_escape_hatch(grd(gc)))
        learned_something_new(TUT_SEEN_ESCAPE_HATCH, gc);
    else if (grid_is_branch_stairs(grd(gc)))
        learned_something_new(TUT_SEEN_BRANCH, gc);
    else if (is_feature('>', gc))
        learned_something_new(TUT_SEEN_STAIRS, gc);
    else if (is_feature('_', gc))
        learned_something_new(TUT_SEEN_ALTAR, gc);
    else if (grd(gc) == DNGN_CLOSED_DOOR)
        learned_something_new(TUT_SEEN_DOOR, gc);
    else if (grd(gc) == DNGN_ENTER_SHOP)
        learned_something_new(TUT_SEEN_SHOP, gc);

    if (igrd(gc) != NON_ITEM
        && Options.feature_item_brand != CHATTR_NORMAL
        && (is_feature('>', gc) || is_feature('<', gc)))
    {
        learned_something_new(TUT_STAIR_BRAND, gc);
    }
}
