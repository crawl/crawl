/*
 *  File:       hints.cc
 *  Summary:    A hints mode as an introduction on how to play Dungeon Crawl.
 *  Written by: j-p-e-g
 *
 *  Created on 2007-01-11.
 */

#include "AppHdr.h"

#include "cio.h"

#include <cstring>
#include <sstream>

#include "hints.h"

#include "abl-show.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "command.h"
#include "decks.h"
#include "describe.h"
#include "files.h"
#include "food.h"
#include "format.h"
#include "fprop.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
#include "jobs.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills2.h"
#include "species.h"
#include "spl-book.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "tags.h"
#include "terrain.h"
#include "travel.h"
#include "view.h"
#include "viewchar.h"
#include "viewgeom.h"

static species_type _get_hints_species(unsigned int type);
static job_type     _get_hints_job(unsigned int type);
static bool         _hints_feat_interesting(dungeon_feature_type feat);
static void         _hints_describe_disturbance(int x, int y);
static void         _hints_describe_cloud(int x, int y);
static void         _hints_describe_feature(int x, int y);
static bool         _water_is_disturbed(int x, int y);

//#define TUTORIAL_DEBUG
#define HINTS_VERSION 11

static int _get_hints_cols()
{
#ifdef USE_TILE
    return crawl_view.msgsz.x;
#else
    int ncols = get_number_of_cols();
    return (ncols > 80? 80 : ncols);
#endif
}

hints_state Hints;

void save_hints(writer& outf)
{
    marshallInt(outf, HINTS_VERSION);
    marshallShort(outf, Hints.hints_type);
    for (long i = 0; i < HINT_EVENTS_NUM; ++i)
        marshallBoolean(outf, Hints.hints_events[i]);
}

void load_hints(reader& inf)
{
    Hints.hints_left = 0;

    int version = unmarshallInt(inf);
    if (version != HINTS_VERSION)
        return;

    Hints.hints_type = unmarshallShort(inf);
    for (long i = 0; i < HINT_EVENTS_NUM; ++i)
    {
        Hints.hints_events[i] = unmarshallBoolean(inf);
        Hints.hints_left += Hints.hints_events[i];
    }
}

// Override init file definition for some options.
void init_hints_options()
{
    if (!Hints.hints_left)
        return;

    // Clear possible debug messages before messing
    // with messaging options.
    mesclr(true);
//     Options.clear_messages = true;
    Options.auto_list  = true;
    Options.show_more  = true;
    Options.small_more = false;

#ifdef USE_TILE
    Options.tile_tag_pref = TAGPREF_TUTORIAL;
#endif
}

void init_hints()
{
    // Activate all triggers.
    // This is rather backwards: If (true) an event still needs to be
    // triggered, if (false) the relevant message was already printed.
    Hints.hints_events.init(true);
    Hints.hints_left = HINT_EVENTS_NUM;

    // Used to compare which fighting means was used most often.
    // XXX: This gets reset with every save, which seems odd.
    //      On the other hand, it's precisely between saves that
    //      players are most likely to forget these.
    Hints.hints_spell_counter   = 0;
    Hints.hints_throw_counter   = 0;
    Hints.hints_melee_counter   = 0;
    Hints.hints_berserk_counter = 0;

    // Store whether explore, stash search or travelling was used.
    // XXX: Also not stored across save games.
    Hints.hints_explored = true;
    Hints.hints_stashes  = true;
    Hints.hints_travel   = true;

    // For occasional healing reminders.
    Hints.hints_last_healed = 0;

    // Did the player recently see a monster turn invisible?
    Hints.hints_seen_invisible = 0;
}

// Tutorial selection screen and choice.
void pick_hints(newgame_def* choice)
{
    clrscr();

    cgotoxy(1,1);
    formatted_string::parse_string(
        "<white>You must be new here indeed!</white>"
        "\n\n"
        "<cyan>You can be:</cyan>"
        "\n").display();

    textcolor(LIGHTGREY);

    for (int i = 0; i < HINT_TYPES_NUM; i++)
        print_hints_menu(i);

    formatted_string::parse_string(
        "<brown>\nEsc - Quit"
        "\n* - Random hints mode character"
        "</brown>\n").display();

    while (true)
    {
        char keyn = getch_ck();

        // Random choice.
        if (keyn == '*' || keyn == '+' || keyn == '!' || keyn == '#')
            keyn = 'a' + random2(HINT_TYPES_NUM);

        // Choose character for hints mode game and set starting values.
        if (keyn >= 'a' && keyn <= 'a' + HINT_TYPES_NUM - 1)
        {
            Hints.hints_type = keyn - 'a';
            choice->species  = _get_hints_species(Hints.hints_type);
            choice->job = _get_hints_job(Hints.hints_type);
            choice->book = SBT_RANDOM;
            choice->weapon = WPN_HAND_AXE; // easiest choice for fighters

            return;
        }

        switch (keyn)
        {
        case 'X':
        CASE_ESCAPE
            cprintf("\nGoodbye!");
            end(0);
            return;
        }
    }
}

void hints_load_game()
{
    if (!Hints.hints_left)
        return;

    learned_something_new(HINT_LOAD_SAVED_GAME);

    // Reinitialise counters for explore, stash search and travelling.
    Hints.hints_explored = Hints.hints_events[HINT_AUTO_EXPLORE];
    Hints.hints_stashes  = true;
    Hints.hints_travel   = true;
}

void print_hints_menu(unsigned int type)
{
    char letter = 'a' + type;
    char desc[100];

    switch (type)
    {
      case HINT_BERSERK_CHAR:
          strcpy(desc, "(Melee oriented character with divine support)");
          break;
      case HINT_MAGIC_CHAR:
          strcpy(desc, "(Magic oriented character)");
          break;
      case HINT_RANGER_CHAR:
          strcpy(desc, "(Ranged fighter)");
          break;
      default: // no further choices
          strcpy(desc, "(erroneous character)");
          break;
    }

    cprintf("%c - %s %s %s\n",
            letter, species_name(_get_hints_species(type)).c_str(),
                    get_job_name(_get_hints_job(type)), desc);
}

static species_type _get_hints_species(unsigned int type)
{
    switch (type)
    {
      case HINT_BERSERK_CHAR:
          return SP_MINOTAUR;
      case HINT_MAGIC_CHAR:
          return SP_DEEP_ELF;
      case HINT_RANGER_CHAR:
          return SP_CENTAUR;
      default:
          // Use something fancy for debugging.
          return SP_KENKU;
    }
}

static job_type _get_hints_job(unsigned int type)
{
    switch (type)
    {
      case HINT_BERSERK_CHAR:
          return JOB_BERSERKER;
      case HINT_MAGIC_CHAR:
          return JOB_CONJURER;
      case HINT_RANGER_CHAR:
          return JOB_HUNTER;
      default:
          // Use something fancy for debugging.
          return JOB_NECROMANCER;
    }
}

// Converts all secret doors in a fixed radius around the player's starting
// position into normal closed doors.
// FIXME: Ideally, we'd need to zap secret doors that block the way
// between entrance and exit.
void hints_zap_secret_doors()
{
    for (radius_iterator ri(you.pos(), 25, true, false); ri; ++ri)
        if (grd(*ri) == DNGN_SECRET_DOOR)
            grd(*ri) = DNGN_CLOSED_DOOR;
}

// Prints the hints mode welcome screen.
void hints_starting_screen()
{
#ifndef USE_TILE
    cgotoxy(1, 1);
#endif
    clrscr();

    int width = _get_hints_cols();
#ifdef USE_TILE
    // Use a more sensible screen width.
    if (width < 80 && width < crawl_view.msgsz.x + crawl_view.hudsz.x)
        width = crawl_view.msgsz.x + crawl_view.hudsz.x;
    if (width > 80)
        width = 80;
#endif

    std::string text;

    text  = "<white>Welcome to Dungeon Crawl!</white>\n\n";
    text += "Your object is to lead a <w>"
         + species_name(you.species) + " " + you.class_name
         +
        "</w> safely through the depths of the dungeon, retrieving the "
        "fabled Orb of Zot and returning it to the surface. "
        "In the beginning, however, let discovery be your "
        "main goal. Try to delve as deeply as possible but beware; "
        "death lurks around every corner.\n\n"
        "For the moment, just remember the following keys "
        "and their functions:\n"
        "  <white>%?</white> - shows the items and the commands\n"
        "  <white>%</white>  - saves the game, to be resumed later "
        "(but note that death is permanent)\n"
        "  <white>%</white>  - examines something in your vicinity\n\n"
        "The hint mode will help you play Crawl without reading any "
        "documentation. If you haven't yet, you might want to try out "
        "the tutorial. Also, if you feel intrigued, there is more information "
        "available in the following files from the docs/ directory (all of "
        "which can also be read in-game):"
        "\n"
        "  <lightblue>quickstart.txt</lightblue>     - "
        "A very short guide to Crawl.\n"
        "  <lightblue>crawl_manual.txt</lightblue>   - "
        "This contains all details on species, magic, skills, etc.\n"
        "  <lightblue>options_guide.txt</lightblue>  - "
        "Crawl's interface is highly configurable. This document \n"
        "                       explains all the options.\n"
        "\n"
        "Happy Crawling!";

    insert_commands(text, CMD_DISPLAY_COMMANDS, CMD_SAVE_GAME, CMD_LOOK_AROUND, 0);
    linebreak_string2(text, width);
    display_tagged_block(text);

#ifndef USE_TILE
    getch_ck();
#else
    mouse_control mc(MOUSE_MODE_MORE);
    getchm();
#endif
    redraw_screen();
}

#ifdef TUTORIAL_DEBUG
static std::string _hints_debug_list(int event)
{
    switch (event)
    {
    case HINT_SEEN_FIRST_OBJECT:
        return "seen first object";
    case HINT_SEEN_POTION:
        return "seen first potion";
    case HINT_SEEN_SCROLL:
        return "seen first scroll";
    case HINT_SEEN_WAND:
        return "seen first wand";
    case HINT_SEEN_SPBOOK:
        return "seen first spellbook";
    case HINT_SEEN_WEAPON:
        return "seen first weapon";
    case HINT_SEEN_MISSILES:
        return "seen first missiles";
    case HINT_SEEN_ARMOUR:
        return "seen first armour";
    case HINT_SEEN_RANDART:
        return "seen first random artefact";
    case HINT_SEEN_FOOD:
        return "seen first food";
    case HINT_SEEN_CARRION:
        return "seen first corpse";
    case HINT_SEEN_GOLD:
        return "seen first pile of gold";
    case HINT_SEEN_JEWELLERY:
        return "seen first jewellery";
    case HINT_SEEN_MISC:
        return "seen first misc. item";
    case HINT_SEEN_MONSTER:
        return "seen first monster";
    case HINT_SEEN_ZERO_EXP_MON:
        return "seen first zero experience monster";
    case HINT_SEEN_TOADSTOOL:
        return "seen first toadstool";
    case HINT_SEEN_STAIRS:
        return "seen first stairs";
    case HINT_SEEN_ESCAPE_HATCH:
        return "seen first escape hatch";
    case HINT_SEEN_BRANCH:
        return "seen first branch entrance";
    case HINT_SEEN_PORTAL:
        return "seen first portal vault entrance";
    case HINT_SEEN_TRAP:
        return "encountered a trap";
    case HINT_SEEN_ALTAR:
        return "seen an altar";
    case HINT_SEEN_SHOP:
        return "seen a shop";
    case HINT_SEEN_DOOR:
        return "seen a closed door";
    case HINT_FOUND_SECRET_DOOR:
        return "found a secret door";
    case HINT_KILLED_MONSTER:
        return "killed first monster";
    case HINT_NEW_LEVEL:
        return "gained a new level";
    case HINT_SKILL_RAISE:
        return "raised a skill";
    case HINT_YOU_ENCHANTED:
        return "caught an enchantment";
    case HINT_YOU_SICK:
        return "became sick";
    case HINT_YOU_POISON:
        return "were poisoned";
    case HINT_YOU_ROTTING:
        return "were rotting";
    case HINT_YOU_CURSED:
        return "had something cursed";
    case HINT_YOU_HUNGRY:
        return "felt hungry";
    case HINT_YOU_STARVING:
        return "were starving";
    case HINT_MAKE_CHUNKS:
        return "learned about chunks";
    case HINT_OFFER_CORPSE:
        return "learned about sacrifice";
    case HINT_MULTI_PICKUP:
        return "read about pickup menu";
    case HINT_HEAVY_LOAD:
        return "were encumbered";
    case HINT_ROTTEN_FOOD:
        return "carried rotten food";
    case HINT_NEED_HEALING:
        return "needed healing";
    case HINT_NEED_POISON_HEALING:
        return "needed healing for poison";
    case HINT_INVISIBLE_DANGER:
        return "encountered an invisible foe";
    case HINT_NEED_HEALING_INVIS:
        return "had to heal near an unseen monster";
    case HINT_ABYSS:
        return "was cast into the Abyss";
    case HINT_POSTBERSERK:
        return "learned about Berserk after-effects";
    case HINT_RUN_AWAY:
        return "were told to run away";
    case HINT_RETREAT_CASTER:
        return "were told to retreat as a caster";
    case HINT_SHIFT_RUN:
        return "learned about shift-run";
    case HINT_MAP_VIEW:
        return "learned about the level map";
    case HINT_AUTO_EXPLORE:
        return "learned about auto-explore";
    case HINT_DONE_EXPLORE:
        return "explored a level";
    case HINT_AUTO_EXCLUSION:
        return "learned about exclusions";
    case HINT_YOU_MUTATED:
        return "caught a mutation";
    case HINT_NEW_ABILITY_GOD:
        return "gained a divine ability";
    case HINT_NEW_ABILITY_MUT:
        return "gained a mutation-granted ability";
    case HINT_NEW_ABILITY_ITEM:
        return "gained an item-granted ability";
    case HINT_WIELD_WEAPON:
        return "wielded an unsuitable weapon";
    case HINT_FLEEING_MONSTER:
        return "made a monster flee";
    case HINT_MONSTER_BRAND:
        return "learned about colour brandings";
    case HINT_MONSTER_FRIENDLY:
        return "seen first friendly monster";
    case HINT_MONSTER_SHOUT:
        return "experienced first shouting monster";
    case HINT_CONVERT:
        return "converted to a god";
    case HINT_GOD_DISPLEASED:
        return "piety ran low";
    case HINT_EXCOMMUNICATE:
        return "excommunicated by a god";
    case HINT_SPELL_MISCAST:
        return "spell miscast";
    case HINT_SPELL_HUNGER:
        return "spell casting caused hunger";
    case HINT_GLOWING:
        return "player glowing from contamination";
    case HINT_STAIR_BRAND:
        return "saw stairs with objects on it";
    case HINT_HEAP_BRAND:
        return "saw heap of objects";
    case HINT_TRAP_BRAND:
        return "saw trap with objects on it";
    case HINT_YOU_RESIST:
        return "resisted some magic";
    case HINT_CAUGHT_IN_NET:
        return "were caught in a net";
    case HINT_LOAD_SAVED_GAME:
        return "restored a saved game";
    case HINT_GAINED_MAGICAL_SKILL:
        return "gained a new magical skill";
    case HINT_CHOOSE_STAT:
        return "could choose a stat";
    case HINT_CAN_BERSERK:
        return "were told to Berserk";
    case HINT_YOU_SILENCE:
        return "experienced silence";
    default:
        return "faced a bug";
    }
}

// Lists all triggerable events and whether they actually were triggered
// at some point, at game start or reload.
static formatted_string _hints_debug()
{
    std::string result;
    bool lbreak = false;
    snprintf(info, INFO_SIZE, "Hints Mode Debug Screen");

    int i = _get_hints_cols()/2-1 - strlen(info) / 2;
    result += std::string(i, ' ');
    result += "<white>";
    result += info;
    result += "</white>\n\n";

    result += "<lightblue>";
    for (i = 0; i < HINT_EVENTS_NUM; i++)
    {
        snprintf(info, INFO_SIZE, "%d: %s (%s)",
                 i, _hints_debug_list(i).c_str(),
                 Hints.hints_events[i] ? "true" : "false");

        result += info;

        // Break text into 2 columns where possible.
        if (strlen(info) >= _get_hints_cols()/2 || lbreak)
        {
            result += "\n";
            lbreak = false;
        }
        else
        {
            result += std::string(_get_hints_cols()/2-1 - strlen(info), ' ');
            lbreak = true;
        }
    }
    result += "</lightblue>\n\n";

    snprintf(info, INFO_SIZE, "hints_left: %d\n", Hints.hints_left);
    result += info;
    result += "\n";

    snprintf(info, INFO_SIZE, "You are a %s %s, and the hint mode will reflect "
             "that.", species_name(you.species), you.class_name);

    result += info;

    return formatted_string::parse_string(result);
}
#endif // debug

// Called each turn from _input. Better name welcome.
void hints_new_turn()
{
    if (Hints.hints_left)
    {
        Hints.hints_just_triggered = false;

        if (you.attribute[ATTR_HELD])
            learned_something_new(HINT_CAUGHT_IN_NET);
        else if (i_feel_safe() && you.level_type != LEVEL_ABYSS)
        {
            // We don't want those "Whew, it's safe to rest now" messages
            // if you were just cast into the Abyss. Right?

            if (2 * you.hp < you.hp_max
                || 2 * you.magic_points < you.max_magic_points)
            {
                hints_healing_reminder();
            }
            else if (!you.running
                     && Hints.hints_events[HINT_SHIFT_RUN]
                     && you.num_turns >= 200
                     && you.hp == you.hp_max
                     && you.magic_points == you.max_magic_points)
            {
                learned_something_new(HINT_SHIFT_RUN);
            }
            else if (!you.running
                     && Hints.hints_events[HINT_MAP_VIEW]
                     && you.num_turns >= 500
                     && you.hp == you.hp_max
                     && you.magic_points == you.max_magic_points)
            {
                learned_something_new(HINT_MAP_VIEW);
            }
            else if (!you.running
                     && Hints.hints_events[HINT_AUTO_EXPLORE]
                     && you.num_turns >= 700
                     && you.hp == you.hp_max
                     && you.magic_points == you.max_magic_points)
            {
                learned_something_new(HINT_AUTO_EXPLORE);
            }
        }
        else
        {
            if (2*you.hp < you.hp_max)
                learned_something_new(HINT_RUN_AWAY);

            if (Hints.hints_type == HINT_MAGIC_CHAR && you.magic_points < 1)
                learned_something_new(HINT_RETREAT_CASTER);
        }
    }
}

// Once a hints mode character dies, offer some last playing hints.
void hints_death_screen()
{
    Hints.hints_left = 0;
    std::string text;

    mpr("Condolences! Your character's premature death is a sad, but "
         "common occurrence in Crawl. Rest assured that with diligence and "
         "playing experience your characters will last longer.",
         MSGCH_TUTORIAL);

    mpr("Perhaps the following advice can improve your playing style:",
         MSGCH_TUTORIAL);
    more();

    if (Hints.hints_type == HINT_MAGIC_CHAR
        && Hints.hints_spell_counter < Hints.hints_melee_counter)
    {
        text = "As a Conjurer your main weapon should be offensive magic. Cast "
               "spells more often! Remember to rest when your Magic is low.";
    }
    else if (you.religion == GOD_TROG && Hints.hints_berserk_counter <= 3
             && !you.berserk() && !you.duration[DUR_EXHAUSTED])
    {
        text = "Don't forget to go berserk when fighting particularly "
               "difficult foes. It is risky, but makes you faster and beefier.";

        if (you.hunger_state <= HS_HUNGRY)
        {
            text += " Berserking is impossible while hungry or worse, so make "
                    "sure to keep some food with you that you can eat when you "
                    "need to go berserk.";
        }
    }
    else if (Hints.hints_type == HINT_RANGER_CHAR
             && 2*Hints.hints_throw_counter < Hints.hints_melee_counter)
    {
        text = "Your bow and arrows are extremely powerful against distant "
               "monsters. Be sure to collect all arrows lying around in the "
               "dungeon.";
    }
    else
    {
        int hint = random2(6);

        bool skip_first_hint = false;
        // If a character has been unusually busy with projectiles and spells
        // give some other hint rather than the first one.
        if (hint == 0 && Hints.hints_throw_counter + Hints.hints_spell_counter
                          >= Hints.hints_melee_counter)
        {
            hint = random2(5) + 1;
            skip_first_hint = true;
        }
        // FIXME: The hints below could be somewhat less random, so that e.g.
        // the message for fighting several monsters in a corridor only happens
        // if there's more than one monster around and you're not in a corridor,
        // or the one about using consumable objects only if you actually have
        // any (useful or unidentified) scrolls/wands/potions.

        if (hint == 5)
        {
            std::vector<monster* > visible =
                get_nearby_monsters(false, true, true, false);

            if (visible.size() < 2)
            {
                if (skip_first_hint)
                    hint = random2(4) + 1;
                else
                    hint = random2(5);
            }
        }

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
                   ", or <w>clicking into the stat area</w>"
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
    mpr(text, MSGCH_TUTORIAL, 0);
    more();

    mpr("See you next game!", MSGCH_TUTORIAL);

    Hints.hints_events.init(false);
}

// If a character survives until Xp 7, the hints mode is declared finished
// and they get a more advanced playing hint, depending on what they might
// know by now.
void hints_finished()
{
    std::string text;

    Hints.hints_left = 0;
    text =  "Congrats! You survived until the end of the hint mode - be sure "
            "to try the other ones as well. Note that the command help screen "
            "(<w>%?</w>) will look very different from now on. Here's a last "
            "playing hint:";
    insert_commands(text, CMD_DISPLAY_COMMANDS, 0);

    mpr(text, MSGCH_TUTORIAL, 0);
    more();

    if (Hints.hints_explored)
    {
        text =  "Walking around and exploring levels gets easier by using "
                "auto-explore (<w>%</w>). Crawl will let you automatically "
                "move to and pick up interesting items.";
#ifdef USE_TILE
        text += "\nAutoexploration can also be started by <w>left-clicking</w> "
                "on the minimap while the <w>Control</w> key is pressed.";
#endif
        insert_commands(text, CMD_EXPLORE, 0);
    }
    else if (Hints.hints_travel)
    {
        text =  "There is a convenient way for travelling between far away "
                "dungeon levels: press <w>%</w> or <w>G</w> and enter "
                "the desired destination. If your travel gets interrupted, "
                "issuing <w>% Enter</w> or <w>G Enter</w> will continue "
                "it.";
        insert_commands(text, CMD_INTERLEVEL_TRAVEL, CMD_INTERLEVEL_TRAVEL, 0);
    }
    else if (Hints.hints_stashes)
    {
        text =  "You can search among all items existing in the dungeon with "
                "the <w>%</w> command. For example, "
                "<w>% \"knife\"</w> will list all knives. You can then "
                "travel to one of the spots. It is even possible to enter "
                "words like <w>\"shop\"</w> or <w>\"altar\"</w>.";
        insert_commands(text, CMD_SEARCH_STASHES, CMD_SEARCH_STASHES, 0);
    }
    else
    {
        int hint = random2(4);
        switch (hint)
        {
        case 0:
            text = "The game keeps an automated logbook for your characters. "
                   "Use <w>%:</w> to read it. You can enter notes manually "
                   "with the <w>%</w> command. Once your character perishes, "
                   "two morgue files are left in the <w>morgue/</w> "
                   "directory. The one ending in .txt contains a copy of "
                   "your logbook. During play, you can create a dump file "
                   "with <w>%</w>.";
            insert_commands(text, CMD_DISPLAY_COMMANDS, CMD_MAKE_NOTE,
                            CMD_CHARACTER_DUMP, 0);
            break;

        case 1:
            text = "Crawl has a macro function built in: press <w>~m</w> "
                   "to define a macro by first specifying a trigger key "
                   "(say, <w>F1</w>) and a command sequence, for example "
                   "<w>za+.</w>. The latter will make the <w>F1</w> "
                   "key always zap the spell in slot a at the nearest "
                   "monster. For more information on macros, type <w>%~</w>.";
            insert_commands(text, CMD_DISPLAY_COMMANDS, 0);
            break;

        case 2:
            text = "The interface can be greatly customised. All options are "
                   "explained in the file <w>options_guide.txt</w> which "
                   "can be found in the <w>docs</w> directory. The options "
                   "themselves are set in <w>init.txt</w> or "
                   "<w>.crawlrc</w>. Crawl will complain if it can't find "
                   "either file.";
            break;

        case 3:
            text = "You can ask other Crawl players for advice and help "
                   "on the <w>##crawl</w> IRC (Internet Relay Chat) "
                   "channel on freenode (<w>irc.freenode.net</w>).";
            break;

        default:
            text =  "Oops... No hint for now. Better luck next time!";
        }
    }
    mpr(text, MSGCH_TUTORIAL, 0);
    more();

    Hints.hints_events.init(false);

    // Remove the hints mode file.
    you.save->delete_chunk("tut");
}

// Occasionally remind religious characters of sacrifices.
void hints_dissection_reminder(bool healthy)
{
    if (Hints.hints_just_triggered || !Hints.hints_left)
        return;

    // When hungry, give appropriate message or at least don't suggest
    // sacrifice.
    if (you.hunger_state < HS_SATIATED && healthy)
    {
        learned_something_new(HINT_MAKE_CHUNKS);
        return;
    }

    if (!god_likes_fresh_corpses(you.religion))
        return;

    if (Hints.hints_events[HINT_OFFER_CORPSE])
        learned_something_new(HINT_OFFER_CORPSE);
    else if (one_chance_in(8))
    {
        Hints.hints_just_triggered = true;

        std::string text;
        text += "If you don't want to eat it, consider offering this "
                "corpse up under <w>p</w>rayer as a sacrifice to ";
        text += god_name(you.religion);
#ifdef USE_TILE
        text += ". You can also chop up any corpse that shows in the floor "
                "part of your inventory tiles by clicking on it with your "
                "<w>left mouse button</w>";
#endif

        text += ". Whenever you view a corpse while in hint mode you can "
                "reread this information.";

        mpr(text, MSGCH_TUTORIAL, 0);


        if (is_resting())
            stop_running();
    }
}

// Occasionally remind injured characters of resting.
void hints_healing_reminder()
{
    if (!Hints.hints_left)
        return;

    if (you.duration[DUR_POISONING] && 2*you.hp < you.hp_max)
    {
        if (Hints.hints_events[HINT_NEED_POISON_HEALING])
            learned_something_new(HINT_NEED_POISON_HEALING);
    }
    else if (Hints.hints_seen_invisible > 0
             && you.num_turns - Hints.hints_seen_invisible <= 20)
    {
        // If we recently encountered an invisible monster, we need a
        // special message.
        learned_something_new(HINT_NEED_HEALING_INVIS);
        // If that one was already displayed, don't print a reminder.
    }
    else
    {
        if (Hints.hints_events[HINT_NEED_HEALING])
            learned_something_new(HINT_NEED_HEALING);
        else if (you.num_turns - Hints.hints_last_healed >= 50
                 && !you.duration[DUR_POISONING])
        {
            if (Hints.hints_just_triggered)
                return;

            Hints.hints_just_triggered = 1;

            std::string text;
            text =  "Remember to rest between fights and to enter unexplored "
                    "terrain with full hitpoints and magic. Ideally you "
                    "should retreat into areas you've already explored and "
                    "cleared of monsters; resting on the edge of the explored "
                    "terrain increases the chances of your rest being "
                    "interrupted by wandering monsters. For resting, press "
                    "<w>5</w> or <w>Shift-numpad 5</w>"
#ifdef USE_TILE
                    ", or <w>click on the stat area</w> with your mouse"
#endif
                    ".";

            if (you.hp < you.hp_max && you.religion == GOD_TROG
                && !you.berserk() && !you.duration[DUR_EXHAUSTED]
                && you.hunger_state >= HS_SATIATED)
            {
              text += "\nAlso, berserking might help you not to lose so many "
                      "hitpoints in the first place. To use your abilities type "
                      "<w>a</w>.";
            }
            mpr(text, MSGCH_TUTORIAL, 0);


            if (is_resting())
                stop_running();
        }
        Hints.hints_last_healed = you.num_turns;
    }
}

// Give a message if you see, pick up or inspect an item type for the
// first time.
void taken_new_item(object_class_type item_type)
{
    switch (item_type)
    {
      case OBJ_WANDS:
          learned_something_new(HINT_SEEN_WAND);
          break;
      case OBJ_SCROLLS:
          learned_something_new(HINT_SEEN_SCROLL);
          break;
      case OBJ_JEWELLERY:
          learned_something_new(HINT_SEEN_JEWELLERY);
          break;
      case OBJ_POTIONS:
          learned_something_new(HINT_SEEN_POTION);
          break;
      case OBJ_BOOKS:
          learned_something_new(HINT_SEEN_SPBOOK);
          break;
      case OBJ_FOOD:
          learned_something_new(HINT_SEEN_FOOD);
          break;
      case OBJ_CORPSES:
          learned_something_new(HINT_SEEN_CARRION);
          break;
      case OBJ_WEAPONS:
          learned_something_new(HINT_SEEN_WEAPON);
          break;
      case OBJ_ARMOUR:
          learned_something_new(HINT_SEEN_ARMOUR);
          break;
      case OBJ_MISSILES:
          learned_something_new(HINT_SEEN_MISSILES);
          break;
      case OBJ_MISCELLANY:
          learned_something_new(HINT_SEEN_MISC);
          break;
      case OBJ_STAVES:
          learned_something_new(HINT_SEEN_STAFF);
          break;
      case OBJ_GOLD:
          learned_something_new(HINT_SEEN_GOLD);
          break;
      default: // nothing to be done
          return;
    }
}

// Give a special message if you gain a skill you didn't have before.
void hints_gained_new_skill(int skill)
{
    if (!Hints.hints_left)
        return;

    learned_something_new(HINT_SKILL_RAISE);

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
        mpr(get_skill_description(skill), MSGCH_TUTORIAL, 0);
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
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_POISON_MAGIC:
        learned_something_new(HINT_GAINED_MAGICAL_SKILL);
        break;

    // Melee skills.
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
    case SK_AXES:
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
    case SK_STAVES:
        learned_something_new(HINT_GAINED_MELEE_SKILL);
        break;

    // Ranged skills.
    case SK_SLINGS:
    case SK_BOWS:
    case SK_CROSSBOWS:
        learned_something_new(HINT_GAINED_RANGED_SKILL);
        break;

    default:
        break;
    }
}

#ifndef USE_TILE
// As safely as possible, colourize the passed glyph.
// Handles quoting "<", MBCS-ing unicode, and
// making DEC characters safe if not properly printable.
static std::string _colourize_glyph(int col, unsigned ch)
{
    glyph g;
    g.col = col;
    g.ch = ch;
    return glyph_to_tagstr(g);
}
#endif

static bool _mons_is_highlighted(const monster* mons)
{
    return (mons->friendly()
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

        if (!obj.defined())
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
        switch (obj.sub_type)
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

void hints_monster_seen(const monster& mon)
{
    if (mons_class_flag(mon.type, M_NO_EXP_GAIN))
    {
        hints_event_type et = mon.type == MONS_TOADSTOOL ?
            HINT_SEEN_TOADSTOOL : HINT_SEEN_ZERO_EXP_MON;

        if (Hints.hints_events[et])
        {
            if (Hints.hints_just_triggered)
                return;

            learned_something_new(et, mon.pos());
            return;
        }

        // Don't do HINT_SEEN_MONSTER for zero exp monsters.
        if (Hints.hints_events[HINT_SEEN_MONSTER])
            return;
    }

    if (!Hints.hints_events[HINT_SEEN_MONSTER])
    {
        if (Hints.hints_just_triggered)
            return;

        if (_mons_is_highlighted(&mon))
            learned_something_new(HINT_MONSTER_BRAND, mon.pos());
        if (mon.friendly())
            learned_something_new(HINT_MONSTER_FRIENDLY, mon.pos());

        if (you.religion == GOD_TROG && !you.berserk()
            && !you.duration[DUR_EXHAUSTED] && you.hunger_state >= HS_SATIATED
            && one_chance_in(4))
        {
            learned_something_new(HINT_CAN_BERSERK);
        }
        return;
    }

    stop_running();

    Hints.hints_events[HINT_SEEN_MONSTER] = false;
    Hints.hints_left--;
    Hints.hints_just_triggered = true;

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
    text += glyph_to_tagstr(get_mons_glyph(&mon));
    text += " is a monster, usually depicted by a letter. Some typical "
            "early monsters look like <brown>r</brown>, <green>l</green>, "
            "<brown>K</brown> or <lightgrey>g</lightgrey>. ";

    if (crawl_view.mlistsz.y > 0)
    {
        text += "Your console settings allowing, you'll always see a "
                "list of monsters somewhere on the screen.\n";
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

    mpr(text, MSGCH_TUTORIAL, 0);

    if (Hints.hints_type == HINT_RANGER_CHAR)
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
        text += "\nAs a short-cut you can also <w>right-click</w> on your "
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


        mpr(text, MSGCH_TUTORIAL, 0);

    }
    else if (Hints.hints_type == HINT_MAGIC_CHAR)
    {
        text =  "However, as a conjurer you will want to deal with it using "
                "magic. If you have a look at your spellbook from your "
                "<w>i</w>nventory, you'll find an explanation of how to do "
                "this.";

#ifdef USE_TILE
        text += "\nAs a short-cut you can also <w>right-click</w> on your "
                "book in your inventory to read its description.";
#endif
        mpr(text, MSGCH_TUTORIAL, 0);

    }
}

void hints_first_item(const item_def &item)
{
    // Happens if monster is standing on dropped corpse or item.
    if (monster_at(item.pos))
        return;

    if (!Hints.hints_events[HINT_SEEN_FIRST_OBJECT]
        || Hints.hints_just_triggered)
    {
        // NOTE: Since a new player might not think to pick up a
        // corpse (and why should they?), HINT_SEEN_CARRION is done when a
        // corpse is first seen.
        if (!Hints.hints_just_triggered
            && item.base_type == OBJ_CORPSES
            && !monster_at(item.pos))
        {
            learned_something_new(HINT_SEEN_CARRION, item.pos);
        }
        return;
    }

    stop_running();

    Hints.hints_events[HINT_SEEN_FIRST_OBJECT] = false;
    Hints.hints_left--;
    Hints.hints_just_triggered = true;

    std::string text = "That ";
#ifndef USE_TILE
    text += glyph_to_tagstr(get_item_glyph(&item));
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
            "\nOnce it is in your inventory, you can drop it again with "
#ifdef USE_TILE
            "a <w>left mouse click</w> while pressing the <w>Shift key</w>. "
            "Whenever you <w>right-click</w> on an item"
#else
            "<w>d</w>. Any time you look at an item in your <w>i</w>nventory"
#endif
            ", you can read about its properties and its description.";

    mpr(text, MSGCH_TUTORIAL, 0);
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

        mpr(text.str(), MSGCH_TUTORIAL, 0);

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

    mpr(text.str(), MSGCH_TUTORIAL, 0);

    text.str("");

    text << "\nYou can check your god's likes and dislikes, as well as your "
            "current standing and divine abilities, by typing <w>^</w>"
#ifdef USE_TILE
            " (alternatively press <w>Shift</w> while "
            "<w>right-clicking</w> on your avatar)"
#endif
            ".";

    mpr(text.str(), MSGCH_TUTORIAL, 0);

}

// If the player is wielding a cursed non-slicing weapon then butchery
// isn't currently possible.
static bool _cant_butcher()
{
    const item_def *wpn = you.weapon();

    if (!wpn || wpn->base_type != OBJ_WEAPONS)
        return false;

    return (wpn->cursed() && !can_cut_meat(*wpn));
}

static int _num_butchery_tools()
{
    int num = 0;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        const item_def& tool(you.inv[i]);

        if (tool.defined()
            && tool.base_type == OBJ_WEAPONS
            && can_cut_meat(tool))
        {
            num++;
        }
    }

    return (num);
}

static std::string _describe_portal(const coord_def &gc)
{
    const std::string desc = feature_description(gc);

    std::ostringstream text;

    // Ziggurat entrances can rarely appear as early as DL 3.
    if (desc.find("zig") != std::string::npos)
    {
        text << "is a portal to a set of special levels filled with very "
                "tough monsters; you probably shouldn't even think of going "
                "in here. Additionally, entering a ziggurat takes a lot of "
                "gold, a lot more than you'd have right now; don't bother "
                "saving gold up for it, since at this point your gold is "
                "better spent at shops buying items which can help you "
                "survive."

                "\n\nIf you <w>still</w> want to enter (and somehow have "
                "gathered enough gold to do so) ";
    }
    // For the sake of completeness, though it's very unlikely that a
    // player will find a bazaar entrance before reahing XL 7.
    else if (desc.find("bazaar") != std::string::npos)
    {
        text << "is a portal to an inter-dimensional bazaar filled with "
                "shops. It will disappear if you don't enter it soon, "
                "so hurry. To enter ";
    }
    // The sewers can appear from DL 3 to DL 6.
    else
    {
        text << "is a portal to a special level where you'll have to fight "
                "your way back to the exit through some tougher than average "
                "monsters (the monsters around the portal should give a "
                "good indication as to how tough), but with the reward of "
                "some good loot. There's no penalty for skipping it, but if "
                "you do skip it the portal will disappear, so you have to "
                "decide now if you want to risk it. To enter ";
    }

    text << "stand over the portal and press <w>></w>. To return find "
#ifdef USE_TILE
        "a similar looking portal tile "
#else
        "another <w>\\</w> (though NOT the ancient stone arch you'll start "
        "out on) "
#endif
        "and press <w><<</w>.";

#ifdef USE_TILE
    text << "\nAlternatively, clicking on your <w>left mouse button</w> "
            "while pressing the <w>Shift key</w> will let you enter any "
            "portal you're standing on.";
#endif

    return (text.str());
}

#define DELAY_EVENT \
{ \
    Hints.hints_events[seen_what] = true; \
    Hints.hints_left++; \
    return; \
}

// Really rare or important events should get a comment even if
// learned_something_new() was already triggered this turn.
// NOTE: If put off, the SEEN_<feature> variant will be triggered the
//       next turn, so they may be rare but aren't urgent.
static bool _rare_hints_event(hints_event_type event)
{
    switch (event)
    {
    case HINT_FOUND_SECRET_DOOR:
    case HINT_KILLED_MONSTER:
    case HINT_NEW_LEVEL:
    case HINT_YOU_ENCHANTED:
    case HINT_YOU_SICK:
    case HINT_YOU_POISON:
    case HINT_YOU_ROTTING:
    case HINT_YOU_CURSED:
    case HINT_YOU_HUNGRY:
    case HINT_YOU_STARVING:
    case HINT_GLOWING:
    case HINT_CAUGHT_IN_NET:
    case HINT_YOU_SILENCE:
    case HINT_NEED_POISON_HEALING:
    case HINT_INVISIBLE_DANGER:
    case HINT_NEED_HEALING_INVIS:
    case HINT_ABYSS:
    case HINT_RUN_AWAY:
    case HINT_RETREAT_CASTER:
    case HINT_YOU_MUTATED:
    case HINT_NEW_ABILITY_GOD:
    case HINT_NEW_ABILITY_MUT:
    case HINT_NEW_ABILITY_ITEM:
    case HINT_CONVERT:
    case HINT_GOD_DISPLEASED:
    case HINT_EXCOMMUNICATE:
    case HINT_GAINED_MAGICAL_SKILL:
    case HINT_GAINED_MELEE_SKILL:
    case HINT_GAINED_RANGED_SKILL:
    case HINT_CHOOSE_STAT:
    case HINT_AUTO_EXCLUSION:
        return (true);
    default:
        return (false);
    }
}

// Here most of the hints mode messages for various triggers are handled.
void learned_something_new(hints_event_type seen_what, coord_def gc)
{
    // Already learned about that.
    if (!Hints.hints_events[seen_what])
        return;

    // Don't trigger twice in the same turn.
    if (Hints.hints_just_triggered && !_rare_hints_event(seen_what))
        return;

    std::ostringstream text;
    std::vector<command_type> cmd;

    Hints.hints_just_triggered = true;
    Hints.hints_events[seen_what] = false;
    Hints.hints_left--;

    switch (seen_what)
    {
    case HINT_SEEN_POTION:
        text << "You have picked up your first potion"
#ifndef USE_TILE
                " ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_POTION))
             << "</w>'). Use "
#else
                ". Simply click on it with your <w>left mouse button</w>, or "
                "press "
#endif
                "<w>%</w> to quaff it.\n"
                "Note that potion effects might be good or bad. For the bad "
                "ones, you might want to wait until you're a bit tougher, but "
                "at the same time it would be nice to know the good ones when "
                "you need them. Ah, decisions, decisions...";
        cmd.push_back(CMD_QUAFF);
        break;

    case HINT_SEEN_SCROLL:
        text << "You have picked up your first scroll"
#ifndef USE_TILE
                " ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_SCROLL))
             << "</w>'). Type "
#else
                ". Simply click on it with your <w>left mouse button</w>, or "
                "type "
#endif
                "<w>%</w> to read it, though you might want to wait until "
                "there's a monster around or you have some more items, so the "
                "scroll can actually have an effect.";
        cmd.push_back(CMD_READ);
        break;

    case HINT_SEEN_WAND:
        text << "You have picked up your first wand"
#ifndef USE_TILE
                " ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_WAND))
             << "</w>'). Type "
#else
                ". Simply click on it with your <w>left mouse button</w>, or "
                "type "
#endif
                "<w>%</w> to evoke it.";
        cmd.push_back(CMD_EVOKE);
        break;

    case HINT_SEEN_SPBOOK:
        text << "You have picked up a book ";
#ifndef USE_TILE
        text << "('<w>";

        text << stringize_glyph(get_item_symbol(SHOW_ITEM_BOOK))
             << "'</w>) "
             << "that you can read by typing <w>%</w>. "
                "If it's a spellbook you'll then be able to memorise spells "
                "via <w>%</w> and cast a memorised spell with <w>%</w>.";
        cmd.push_back(CMD_READ);
        cmd.push_back(CMD_MEMORISE_SPELL);
        cmd.push_back(CMD_CAST_SPELL);
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
                    "<w>%</w>bility.";
            cmd.push_back(CMD_USE_ABILITY);
        }
        else if (!you.skills[SK_SPELLCASTING])
        {
            text << "\nHowever, first you will have to get accustomed to "
                    "spellcasting by reading lots of scrolls.";
        }
        text << "\nIn hint mode you can reread this information at "
                "any time by "
#ifndef USE_TILE
                "having a look in your <w>%</w>nventory at the item in "
                "question.";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
#else
                "clicking on it with your <w>right mouse button</w>.";
#endif
        break;

    case HINT_SEEN_WEAPON:
        text << "This is the first weapon "
#ifndef USE_TILE
                "('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_WEAPON))
             << "</w>') "
#endif
                "you've picked up. Use <w>%</w> "
#ifdef USE_TILE
                "or click on it with your <w>left mouse button</w> "
#endif
                "to wield it, but be aware that this weapon "
                "might train a different skill from your current one. You can "
                "view the weapon's properties from your <w>%</w>nventory"
#ifdef USE_TILE
                " or by <w>right-clicking</w> on it"
#endif
                ".";

        cmd.push_back(CMD_WIELD_WEAPON);
        cmd.push_back(CMD_DISPLAY_INVENTORY);

        if (Hints.hints_type == HINT_BERSERK_CHAR)
        {
            text << "\nAs you're already trained in Axes you should stick "
                    "with these. Checking other axes' enchantments and "
                    "attributes can be worthwhile.";
        }
        else if (Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << "\nAs a spellslinger you don't need a weapon to fight. "
                    "However, you should still carry at least one knife, "
                    "dagger, sword or axe so that you can chop up corpses.";
        }
        break;

    case HINT_SEEN_MISSILES:
        text << "This is the first stack of missiles "
#ifndef USE_TILE
                "('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_MISSILE))
             << "</w>') "
#endif
                "you've picked up. Missiles like darts and throwing nets "
                "can be thrown by hand, but other missiles like arrows and "
                "needles require a launcher and training in using it to be "
                "really effective. "
#ifdef USE_TILE
                "<w>Right-clicking</w> on "
#else
                "Selecting "
#endif
                "the item in your <w>%</w>nventory will give more "
                "information about both missiles and launcher.";

        cmd.push_back(CMD_DISPLAY_INVENTORY);

        if (Hints.hints_type == HINT_RANGER_CHAR)
        {
            text << "\nAs you're already trained in Bows you should stick "
                    "with arrows and collect more of them in the dungeon.";
        }
        else if (Hints.hints_type == HINT_MAGIC_CHAR)
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

    case HINT_SEEN_ARMOUR:
        text << "This is the first piece of armour "
#ifndef USE_TILE
                "('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_ARMOUR))
             << "</w>') "
#endif
                "you've picked up. "
#ifdef USE_TILE
                "You can click on it to wear it, and click a second time to "
                "take it off again. Doing a <w>right mouse click</w> will "
                "show you its properties.";
#else
                "Use <w>%</w> to wear it and <w>%</w> to take it off again. "
                "You can view its properties from your <w>%</w>nventory.";
        cmd.push_back(CMD_WEAR_ARMOUR);
        cmd.push_back(CMD_REMOVE_ARMOUR);
        cmd.push_back(CMD_DISPLAY_INVENTORY);
#endif

        if (you.species == SP_CENTAUR || you.species == SP_MINOTAUR)
        {
            text << "\nNote that as a " << species_name(you.species)
                 << " you will be unable to wear "
                 << (you.species == SP_CENTAUR ? "boots" : "helmets")
                 << ".";
        }
        break;

    case HINT_SEEN_RANDART:
        text << "Weapons and armour that have unusual descriptions like this "
                "are much more likely to be of higher enchantment or have "
                "special properties, good or bad. The rarer the description, "
                "the greater the potential value of an item.";
        break;

    case HINT_SEEN_FOOD:
        text << "You have picked up some food"
#ifndef USE_TILE
                " ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_FOOD))
             << "</w>')"
#endif
                ". You can eat it by typing <w>%</w>"
#ifdef USE_TILE
                " or by clicking on it with your <w>left mouse button</w>"
#endif
                ". However, it is usually best to conserve rations and fruit "
                "until you are hungry or even starving.";

        cmd.push_back(CMD_EAT);
        break;

    case HINT_SEEN_CARRION:
        // NOTE: This is called when a corpse is first seen as well as when
        //       first picked up, since a new player might not think to pick
        //       up a corpse.
        // TODO: Specialcase skeletons and rotten corpses!

        if (gc.x <= 0 || gc.y <= 0)
            text << "Ah, a corpse!";
        else
        {
            int i = you.visible_igrd(gc);
            if (i == NON_ITEM)
                text << "Ah, a corpse!";
            else
            {
                text << "That ";
#ifndef USE_TILE
                std::string glyph = glyph_to_tagstr(get_item_glyph(&mitm[i]));
                const std::string::size_type found = glyph.find("%");
                if (found != std::string::npos)
                    glyph.replace(found, 1, "percent");
                text << glyph;
                text << " ";
#else
                tiles.place_cursor(CURSOR_TUTORIAL, gc);
                tiles.add_text_tag(TAG_TUTORIAL, mitm[i].name(DESC_CAP_A), gc);
#endif

                text << "is a corpse.";
            }
        }

        text << " When a corpse is lying on the ground, you "
                "can <w>%</w>hop it up with a sharp implement";
        cmd.push_back(CMD_BUTCHER);

        if (_cant_butcher())
        {
            text << " (though unfortunately you can't do that right now, "
                    "since the cursed weapon you're wielding can't slice up "
                    "meat, and you can't let go of it to wield one that "
                    "can)";
        }
        else if (_num_butchery_tools() == 0)
        {
            text << " (but you currently possess nothing which can do this, "
                    "so you should pick up the first knife, dagger, sword "
                    "or axe you find)";
        }
        text << ". Once hungry you can then <w>%</w>at the resulting chunks "
                "(though they may not be healthful).";
        cmd.push_back(CMD_EAT);

#ifdef USE_TILE
        text << " With tiles, you can also chop up any corpse that shows up in "
                "the floor part of your inventory region, simply by doing a "
                "<w>left mouse click</w> while pressing <w>Shift</w>, and "
                "then eat the resulting chunks with <w>Shift + right mouse "
                "click</w>.";
#endif

        if (god_likes_fresh_corpses(you.religion))
        {
            text << "\nYou can also offer corpses to "
                 << god_name(you.religion)
                 << " by <w>%</w>raying over them. Note that the gods will not "
                    "accept rotting flesh.";
            cmd.push_back(CMD_PRAY);
        }
        text << "\nIn hint mode you can reread this information at "
                "any time by selecting the item in question in your "
                "<w>%</w>nventory.";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_JEWELLERY:
        text << "You have picked up a a piece of jewellery, either a ring"
#ifndef USE_TILE
             << " ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_RING))
             << "</w>')"
#endif
             << " or an amulet"
#ifndef USE_TILE
             << " ('<w>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_AMULET))
             << "</w>')"
             << ". Type <w>%</w> to put it on and <w>%</w> to remove "
                "it. You can view its properties from your <w>%</w>nventory"
#else
             << ". You can click on it to put it on, and click a second time "
                "remove it again. By clicking on it with your <w>right mouse "
                "button</w> you can view its properties"
#endif
             << ", though often magic is necessary to reveal its true "
                "nature.";
        cmd.push_back(CMD_WEAR_JEWELLERY);
        cmd.push_back(CMD_REMOVE_JEWELLERY);
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_MISC:
        text << "This is a curious object indeed. You can play around with "
                "it to find out what it does by "
#ifdef USE_TILE
                "clicking on it to e<w>%</w>oke "
#else
                "e<w>%</w>oking "
#endif
                "it. Some items need to be <w>%</w>ielded first before you can "
                "e<w>%</w>oke them. As usual, selecting it from your "
                "<w>%</w>nventory might give you more information.";
        cmd.push_back(CMD_EVOKE);
        cmd.push_back(CMD_WIELD_WEAPON);
        cmd.push_back(CMD_EVOKE_WIELDED);
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_STAFF:
        text << "You have picked up a magic staff or a rod"
#ifndef USE_TILE
                ", both of which are represented by '<w>";

        text << stringize_glyph(get_item_symbol(SHOW_ITEM_STAVE))
             << "</w>'"
#endif
                ". Both must be <w>%</w>ielded to be of use. "
                "Magicians use staves to increase their power in certain "
                "spell schools. By contrast, a rod allows the casting of "
                "certain spells even without magic knowledge simply by "
                "e<w>%</w>oking it. For the latter the power depends on "
                "your Evocations skill.";
        cmd.push_back(CMD_WIELD_WEAPON);
        cmd.push_back(CMD_EVOKE_WIELDED);

#ifdef USE_TILE
        text << " Both wielding and evoking a wielded item can be achieved "
                "by clicking on it with your <w>left mouse button</w>.";
#endif
        text << "\nIn hint mode you can reread this information at "
                "any time by selecting the item in question in your "
                "<w>%</w>nventory.";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_SEEN_GOLD:
        text << "You have picked up your first pile of gold"
#ifndef USE_TILE
                " ('<yellow>"
             << stringize_glyph(get_item_symbol(SHOW_ITEM_GOLD))
             << "</yellow>')"
#endif
                ". Unlike all other objects in Crawl it doesn't show up in "
                "your inventory, takes up no space in your inventory, weighs "
                "nothing and can't be dropped. Gold can be used to buy "
                "items from shops, and can also be sacrificed to some gods. ";

        if (!Options.show_gold_turns)
        {
            text << "Whenever you pick up some gold, your current amount will "
                    "be mentioned. If you'd like to check your wealth at other "
                    "times, you can press <w>%</w>. It will also be "
                    "listed on the <w>%</w> screen.";
            cmd.push_back(CMD_LIST_GOLD);
            cmd.push_back(CMD_RESISTS_SCREEN);
        }
        break;

    case HINT_SEEN_STAIRS:
        // Don't give this information during the first turn, to give
        // the player time to have a look around.
        if (you.num_turns < 1)
            DELAY_EVENT;

        text << "These ";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Stairs", gc);
#endif
        text << "are some downstairs. You can enter the next (deeper) "
                "level by following them down (<w>%</w>). To get back to "
                "this level again, press <w>%</w> while standing on the "
                "upstairs.";
        cmd.push_back(CMD_GO_DOWNSTAIRS);
        cmd.push_back(CMD_GO_UPSTAIRS);

#ifdef USE_TILE
        text << "\nAlternatively, clicking on your <w>left mouse button</w> "
                "while pressing the <w>Shift key</w> will let you follow any "
                "stairs you're standing on.";
#endif
        break;

    case HINT_SEEN_ESCAPE_HATCH:
        if (you.num_turns < 1)
            DELAY_EVENT;

        // monsters standing on stairs
        if (monster_at(gc))
            DELAY_EVENT;

        text << "These ";
#ifndef USE_TILE
        text << glyph_to_tagstr(get_cell_glyph(gc));
        text << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Escape hatch", gc);
#endif
        text << "are some kind of escape hatch. You can use them to "
                "quickly leave a level with <w>%</w> and <w>%</w>, "
                "respectively "
#ifdef USE_TILE
                "(or by using your <w>left mouse button</w> in combination "
                "with the <w>Shift key</w>)"
#endif
                ", but will usually be unable to return right away.";
        cmd.push_back(CMD_GO_UPSTAIRS);
        cmd.push_back(CMD_GO_DOWNSTAIRS);
        break;

    case HINT_SEEN_BRANCH:
        text << "This ";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        // FIXME: Branch entrance character is not being colored yellow.
        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Branch stairs", gc);
#endif
        text << "is the entrance to a different branch of the dungeon, "
                "which might have different terrain, level layout and "
                "monsters from the current main branch you're in. Branches "
                "can range from being up to ten levels deep to having only "
                "a single level. They can also contain entrances to other "
                "branches."

                "\n\nThe first three branches you'll encounter are the "
                "Temple, the Orcish Mines and the Lair. While the Mines "
                "and the Lair can be dangerous for the new adventurer, "
                "the Temple is completely safe and contains a number of "
                "altars at which you might convert to a new god.";
        break;

    case HINT_SEEN_PORTAL:
        // Delay in the unlikely event that a player still in hints mode
        // creates a portal with a Trowel card, since a portal vault
        // entry's description doesn't seem to get set properly until
        // after the vault is done being placed.
        if (you.pos() == gc)
            DELAY_EVENT;

        text << "This ";
#ifndef USE_TILE
        // Is a monster blocking the view?
        if (monster_at(gc))
            DELAY_EVENT;

        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#else
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Portal", gc);
#endif
        text << _describe_portal(gc);
        break;

    case HINT_STAIR_BRAND:
        // Monster or player standing on stairs.
        if (actor_at(gc))
            DELAY_EVENT;

#ifdef USE_TILE
        text << "A small question mark on a stair tile signifies that there "
                "are items in that position that you may want to check out.";
#else
        text << "If any items are covering stairs or an escape hatch, then "
                "that will be indicated by highlighting the <w><<</w> or "
                "<w>></w> symbol, instead of hiding the stair symbol with "
                "an item glyph.";
#endif
        break;

    case HINT_HEAP_BRAND:
        // Monster or player standing on heap.
        if (actor_at(gc))
            DELAY_EVENT;

#ifdef USE_TILE
        text << "A small question mark on an item tile signifies that there "
                "is at least one other item in the same heap that you may want "
                "to check out.";
        break;
#else
        text << "If two or more items are on a single square, then the square "
                "will be highlighted, and the symbol for the item on the top "
                "of the heap will be shown.";
#endif
        break;

    case HINT_TRAP_BRAND:
#ifdef USE_TILE
        // Tiles show both the trap and the item heap.
        return;
#else
        // Monster or player standing on trap.
        if (actor_at(gc))
            DELAY_EVENT;

        text << "If any items are covering a trap, then that will be "
                "indicated by highlighting the <w>^</w> symbol, instead of "
                "hiding the trap symbol with an item glyph.";
#endif
        break;

    case HINT_SEEN_TRAP:
        if (you.pos() == gc)
            text << "Oops... you just triggered a trap. ";
        else
            text << "You just discovered a trap. ";

        text << "An unwary adventurer will occasionally stumble into one "
                "of these nasty constructions";
#ifndef USE_TILE
        {
            glyph g = get_cell_glyph(gc);

            if (g.ch == ' ' || g.col == BLACK)
                g.col = LIGHTCYAN;

            text << " depicted by " << _colourize_glyph(g.col, '^');
        }
#endif
        text << ". They can do physical damage (with darts or needles, for "
                "example) or have other, more magical effects, like "
                "teleportation. The mechanical variant can be disarmed with "
                "<w>Ctrl + direction</w> "
#ifdef USE_TILE
                "or with <w>Ctrl + leftclick</w> "
#endif
                "while standing next to it.";
        break;

    case HINT_SEEN_ALTAR:
        text << "That ";
#ifndef USE_TILE
        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#else
        {
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            std::string altar = "An altar to ";
            altar += god_name(feat_altar_god(grd(gc)));
            tiles.add_text_tag(TAG_TUTORIAL, altar, gc);
        }
#endif
        text << "is an altar. "
#ifdef USE_TILE
                "By <w>rightclicking</w> on it with your mouse "
#else
                "If you target the altar with <w>x</w>, then press <w>v</w> "
#endif
                "you can get a short description.\n"
                "Press <w>%</w> while standing on the square to join the faith "
                "or read some information about the god in question. Before "
                "taking up the corresponding faith you'll be asked for "
                "confirmation.";
        cmd.push_back(CMD_PRAY);

        if (you.religion == GOD_NO_GOD
            && Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << "\n\nThe best god for an unexperienced conjurer is "
                    "probably Vehumet, though Sif Muna is a good second "
                    "choice.";
        }
        break;

    case HINT_SEEN_SHOP:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, shop_name(gc), gc);
#endif
        text << "That "
#ifndef USE_TILE
             << glyph_to_tagstr(get_cell_glyph(gc)) << " "
#endif
                "is a shop. You can enter it by typing <w>%</w> or <w>%</w>"
#ifdef USE_TILE
                ", or by pressing <w>Shift</w> and clicking on it with your "
                "<w>left mouse button</w> "
#endif
                "while standing on the square.";
        cmd.push_back(CMD_GO_UPSTAIRS);
        cmd.push_back(CMD_GO_DOWNSTAIRS);

        text << "\n\nIf there's anything you want which you can't afford yet "
                "you can select those items and press <w>@</w> to put them "
                "on your shopping list. The game will then remind you when "
                "you gather enough gold to buy the items on your list.";
        break;

    case HINT_SEEN_DOOR:
        if (you.num_turns < 1)
            DELAY_EVENT;

#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Closed door", gc);
#endif

        text << "That "
#ifndef USE_TILE
             << glyph_to_tagstr(get_cell_glyph(gc)) << " "
#endif
                "is a closed door. You can open it by walking into it. "
                "Sometimes it is useful to close a door. Do so by pressing "
                "<w>%</w> while standing next to it. If there are several "
                "doors, you will then be prompted for a direction. "
                "Alternatively, you can also use <w>Ctrl-Direction</w>.";
        cmd.push_back(CMD_CLOSE_DOOR);

#ifdef USE_TILE
        text << "\nIn Tiles, the same can be achieved by clicking on an "
                "adjacent door square.";
#endif
        if (!Hints.hints_explored)
        {
            text << "\nTo avoid accidentally opening a door you'd rather "
                    "remain closed during travel or autoexplore, you can mark "
                    "it with an exclusion from the map view (<w>%</w>) with "
                    "<w>ee</w> while your cursor is on the grid in question. "
                    "Such an exclusion will prevent autotravel from ever "
                    "entering that grid until you remove the exclusion with "
                    "another press of <w>%e</w>.";
            cmd.push_back(CMD_DISPLAY_MAP);
            cmd.push_back(CMD_DISPLAY_MAP);
        }
        break;

    case HINT_FOUND_SECRET_DOOR:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, "Secret door", gc);
#endif
        text << "That ";
#ifndef USE_TILE
        text << glyph_to_tagstr(get_cell_glyph(gc)) << " ";
#endif
        if (grd(gc) == DNGN_SECRET_DOOR)
            text << "is";
        else
            text << "was";

        text << " a secret door. You can actively try to find secret doors "
                "by searching. To search for one turn, press <w>s</w>, "
                "<w>.</w>, <w>delete</w> or <w>keypad-5</w>. Pressing "
                "<w>5</w> or <w>shift-and-keypad-5</w> "
#ifdef USE_TILE
                ", or clicking into the stat area "
#endif
                "will search 100 times, stopping early if you find any "
                "secret doors or traps, or when your HP or MP fully "
                "recovers.\n\n"

                "If you can't find all three (or any) of the down stairs "
                "on a level, you should try searching for secret doors, since "
                "the missing stairs might be in sections of the level blocked "
                "off by them. If you really can't find any secret doors, then "
                "the missing stairs are probably in sections of the level "
                "totally disconnected from the section you're searching.";
        break;

    case HINT_KILLED_MONSTER:
        text << "Congratulations, your character just gained some experience "
                "by killing this monster! Every action will use up some of "
                "it to train certain skills. For example, fighting monsters ";

        if (Hints.hints_type == HINT_BERSERK_CHAR)
        {
            text << "in melee battle will raise your Axes and Fighting "
                    "skills.";
        }
        else if (Hints.hints_type == HINT_RANGER_CHAR)
            text << "using bow and arrows will raise your Bows skill.";
        else // if (Hints.hints_type == HINT_MAGIC_CHAR)
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

    case HINT_NEW_LEVEL:
        text << "Well done! Reaching a new experience level is always a nice "
                "event: you get more health and magic points, and "
                "occasionally increases to your attributes (strength, "
                "dexterity, intelligence).";

        if (Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << "\nAlso, new experience levels let you learn more spells "
                    "(the Spellcasting skill also does this). For now, you "
                    "should try to memorise the second spell of your "
                    "starting book with <w>%a</w>, which can then be zapped "
                    "with <w>%b</w>.";
#ifdef USE_TILE
            text << " Memorising is also possible by doing a <w>left "
                    "click</w> on the book in your inventory, or by clicking "
                    "on the <w>spellbook tab</w> to the left of your "
                    "inventory.";
#endif
            cmd.push_back(CMD_MEMORISE_SPELL);
            cmd.push_back(CMD_CAST_SPELL);
        }
        break;

    case HINT_SKILL_RAISE:
        text << "One of your skills just got raised. You can train your skills "
                "or pick up new ones by performing the corresponding actions. "
                "To view or manage your skill set, type <w>%</w>.";
        cmd.push_back(CMD_DISPLAY_SKILLS);
        break;

    case HINT_GAINED_MAGICAL_SKILL:
        text << "Being skilled in a magical \"school\" makes it easier to "
                "learn and cast spells of this school. Many spells belong to "
                "a combination of several schools, in which case the average "
                "skill in these schools will decide on spellcasting success "
                "and power.";
        break;

    case HINT_GAINED_MELEE_SKILL:
        text << "Being skilled with a particular type of weapon will make it "
                "easier to fight with all weapons of this type, and make you "
                "deal more damage with them. It is generally recommended to "
                "concentrate your efforts on one or two weapon types to become "
                "more powerful in them. Some weapons are closely related, and "
                "being trained in one will ease training the other. This is "
                "true for the following pairs: Short Blades/Long Blades, "
                "Axes/Polearms, Polearms/Staves, and Axes/Maces.";
        break;

    case HINT_GAINED_RANGED_SKILL:
        text << "Being skilled in a particular type of ranged attack will let "
                "you deal more damage when using the appropriate weapons. It "
                "is usually best to concentrate on one type of ranged attack "
                "(including spells), and to add another one as back-up.";
        break;

    case HINT_CHOOSE_STAT:
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

    case HINT_YOU_ENCHANTED:
        text << "Enchantments of all types can befall you temporarily. "
                "Brief descriptions of these appear at the lower end of the "
                "stats area. Press <w>%</w> for more details. A list of all "
                "possible enchantments is in the manual (<w>%5</w>).";
        cmd.push_back(CMD_DISPLAY_CHARACTER_STATUS);
        cmd.push_back(CMD_DISPLAY_COMMANDS);
        break;

    case HINT_YOU_SICK:
        // Hack: reset hints_just_triggered, to force recursive calling of
        // learned_something_new().
        Hints.hints_just_triggered = false;
        learned_something_new(HINT_YOU_ENCHANTED);
        Hints.hints_just_triggered = true;
        text << "Chunks and corpses that are described as "
                "<brown>contaminated</brown> can make you sick when eating "
                "them. However, there's only a chance of this happening and "
                "food is sufficiently rare in the dungeon that you'll "
                "sometimes have to dine on them anyway and hope for the best.\n"
                "While sick, your hitpoints won't regenerate and sometimes "
                "an attribute may decrease. It wears off with time (";

        if (!i_feel_safe())
            text << "find a quiet corner and ";

        text << "wait with <w>%</w>"
#ifdef USE_TILE
                " or by clicking into the stats area"
#endif
                "), or you could <w>%</w>uaff a potion of healing.\n"
                "Note that if a chunk makes you sick, you won't get any "
                "nutrition out of it.";

        cmd.push_back(CMD_REST);
        cmd.push_back(CMD_QUAFF);
        break;

    case HINT_YOU_POISON:
        // Hack: reset hints_just_triggered, to force recursive calling of
        // learned_something_new().
        Hints.hints_just_triggered = false;
        learned_something_new(HINT_YOU_ENCHANTED);
        Hints.hints_just_triggered = true;
        text << "Poison will slowly reduce your HP. It wears off with time (";

        if (!i_feel_safe())
            text << "find a quiet corner and ";

        text << "wait with <w>%</w>"
#ifdef USE_TILE
                "or by clicking onto the stats area"
#endif
                "), or you could <w>%</w>uaff a potion of healing. ";
        cmd.push_back(CMD_REST);
        cmd.push_back(CMD_QUAFF);
        break;

    case HINT_YOU_ROTTING:
        // Hack: Reset hints_just_triggered, to force recursive calling of
        //       learned_something_new().
        Hints.hints_just_triggered = false;
        learned_something_new(HINT_YOU_ENCHANTED);
        Hints.hints_just_triggered = true;

        text << "Ugh, your flesh is rotting! Not only does this slowly "
                "reduce your HP, it also slowly reduces your <w>maximum</w> "
                "HP (your usual maximum HP will be indicated by a number in "
                "parentheses).\n"
                "While you can wait it out, you'll probably want to stop "
                "rotting as soon as possible by <w>%</w>uaffing a potion of "
                "healing, since the longer you wait the more your maximum HP "
                "will be reduced. Once you've stopped rotting you can restore "
                "your maximum HP to normal by drinking potions of healing and "
                "heal wounds while fully healed.";
        cmd.push_back(CMD_QUAFF);
        break;

    case HINT_YOU_CURSED:
        text << "Curses are comparatively harmless, but they do mean that "
                "you cannot remove cursed equipment and will have to suffer "
                "the (possibly) bad effects until you find and read a scroll "
                "of remove curse (though if you're wielding a cursed "
                "non-slicing weapon, you'll be unable to <w>%</w>hop up "
                "corpses into chunks). Weapons and armour can also be "
                "uncursed using the appropriate enchantment scrolls.";
        cmd.push_back(CMD_BUTCHER);
        break;

    case HINT_YOU_HUNGRY:
        text << "There are two ways to overcome hunger: food you started "
                "with or found, and self-made chunks from corpses. To get the "
                "latter, all you need to do is <w>%</w>hop up a corpse "
                "with a sharp implement. ";
        cmd.push_back(CMD_BUTCHER);

        if (_cant_butcher())
        {
            text << "Unfortunately you can't butcher corpses right now, "
                    "since the cursed weapon you're wielding can't slice up "
                    "meat, and you can't let go of it to wield one that "
                    "can.";
        }
        else
        {
            const int num = _num_butchery_tools();
            if (num == 0)
            {
                text << "However, you currently possess no sharp implements, "
                        "so you should pick up the first knife, dagger, sword "
                        "or axe you find. ";
            }
            else if (Hints.hints_type != HINT_MAGIC_CHAR)
                text << "Your starting weapon will do nicely. ";
            else if (num == 1)
                text << "The slicing weapon you picked up will do nicely. ";
            else
            {
                text << "One of the slicing weapons you picked up will do "
                        "nicely. ";
            }
        }

        text << "Try to dine on chunks in order to save permanent food.";

        if (Hints.hints_type == HINT_BERSERK_CHAR)
            text << "\nNote that you cannot Berserk while hungry or worse.";
        break;

    case HINT_YOU_STARVING:
        text << "You are now suffering from terrible hunger. You'll need to "
                "<w>%</w>at something quickly, or you'll die. The safest "
                "way to deal with this is to simply eat something from your "
                "inventory, rather than wait for a monster to leave a corpse. "
                "In a pinch, potions and fountains also can provide some "
                "nutrition, though not as much as food.";
        cmd.push_back(CMD_EAT);

        if (Hints.hints_type == HINT_MAGIC_CHAR)
            text << "\nNote that you cannot cast spells while starving.";
        break;

    case HINT_MULTI_PICKUP:
        text << "There's a more comfortable way to pick up several items at "
                "the same time. If you press <w>%</w> or <w>g</w> "
#ifdef USE_TILE
                ", or click your left mouse button "
#endif
                "twice you can choose items from a menu"
#ifdef USE_TILE
                ", either by pressing their letter, or by clicking the "
                "corresponding lines in the menu"
#endif
                ".\nThis takes fewer keystrokes but has no influence on the "
                "number of turns needed. Multi-pickup will be interrupted by "
                "monsters or other dangerous events.";
        cmd.push_back(CMD_PICKUP);
        break;

    case HINT_HEAVY_LOAD:
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
                "<w>%</w>rop some of the stuff you don't need or that's too "
                "heavy to lug around permanently.";
        cmd.push_back(CMD_DROP);

#ifdef USE_TILE
        text << " In the drop menu you can then comfortably select which "
                "items to drop by pressing their inventory letter, or by "
                "clicking on them.";
#endif

        if (Hints.hints_stashes)
        {
            text << "\n\nYou can easily find items you've left on the floor "
                    "with the <w>%</w> command, which will let you "
                    "seach for all known items in the dungeon. For example, "
                    "<w>% \"knife\"</w> will list all knives. You can "
                    "can then travel to one of the spots.";
            Hints.hints_stashes = false;
            cmd.push_back(CMD_SEARCH_STASHES);
            cmd.push_back(CMD_SEARCH_STASHES);
        }

        text << "\n\nBe warned that items that you leave on the floor can "
                "be picked up and used by monsters.";
        break;

    case HINT_ROTTEN_FOOD:
        text << "One or more of the chunks or corpses you carry has started "
                "to rot. Few species can digest these safely, so you might "
                "just as well <w>%</w>rop them now. When selecting items from "
                "a menu, there's a shortcut (<w>&</w>) to select all corpses, "
                "skeletons, and rotten chunks in your inventory at once.";
         cmd.push_back(CMD_DROP);
        break;

    case HINT_MAKE_CHUNKS:
        text << "How lucky! That monster left a corpse which you can now "
                "<w>%</w>hop up";
        cmd.push_back(CMD_BUTCHER);

        if (_cant_butcher())
        {
            text << "(or which you <w>could</w> chop up if it weren't for "
                    "the fact that you can't let go of your cursed "
                    "non-chopping weapon)";
        }
        else if (_num_butchery_tools() == 0)
        {
            text << "(or which you <w>could</w> chop up if you had a "
                    "chopping weapon; you should pick up the first knife, "
                    "dagger, sword or axe you find if you want to be able "
                    "to chop up corpses)";
        }
        text << ". One or more chunks will appear that you can then "
                "<w>%</w>at. Beware that some chunks may be, sometimes or "
                "always, hazardous. You can find out whether that might be the "
                "case by ";
        cmd.push_back(CMD_EAT);

#ifdef USE_TILE
        text << "hovering your mouse over the corpse or chunk.";
#else
        text << "<w>v</w>iewing a corpse or chunk on the floor or by having a "
                "look at it in your <w>%</w>nventory.";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
#endif
        break;

    case HINT_OFFER_CORPSE:
        if (!god_likes_fresh_corpses(you.religion)
            || you.hunger_state < HS_SATIATED)
        {
            return;
        }

        text << "Hey, that monster left a corpse! If you don't need it for "
                "food or other purposes, you can sacrifice it to "
             << god_name(you.religion)
             << " by <w>%</w>raying over it to offer it. ";
        cmd.push_back(CMD_PRAY);
        break;

    case HINT_SHIFT_RUN:
        text << "Walking around takes fewer keystrokes if you press "
                "<w>Shift-direction</w> or <w>/ direction</w>. "
                "That will let you run until a monster comes into sight or "
                "your character sees something interesting.";
        break;

    case HINT_MAP_VIEW:
        text << "As you explore a level, orientation can become difficult. "
                "Press <w>%</w> to bring up the level map. Typing <w>?</w> "
                "shows the list of level map commands. "
                "Most importantly, moving the cursor to a spot and pressing "
                "<w>.</w> or <w>Enter</w> lets your character move there on "
                "its own.";
        cmd.push_back(CMD_DISPLAY_MAP);

#ifdef USE_TILE
        text << "\nAlso, clicking on the right-side minimap with your "
                "<w>right mouse button</w> will zoom into that dungeon area. "
                "Clicking with the <w>left mouse button</w> instead will let "
                "you move there.";
#endif
        break;

    case HINT_AUTO_EXPLORE:
        if (!Hints.hints_explored)
            return;

        text << "Fully exploring a level and picking up all the interesting "
                "looking items can be tedious. To save on this tedium you "
                "can press <w>%</w> to auto-explore, which will "
                "automatically explore unmapped regions, automatically pick "
                "up interesting items, and stop if a monster or interesting "
                "dungeon feature (stairs, altar, etc.) is encountered.";
        cmd.push_back(CMD_EXPLORE);
        Hints.hints_explored = false;
        break;

    case HINT_DONE_EXPLORE:
        // XXX: You'll only get this message if you're using auto exploration.
        text << "Hey, you've finished exploring the dungeon on this level! "
                "You can search for stairs from the level map (<w>%</w>) "
                "by pressing <w>></w>. The cursor will jump to the nearest "
                "staircase, and by pressing <w>.</w> or <w>Enter</w> your "
                "character can move there, too. ";
        cmd.push_back(CMD_DISPLAY_MAP);

        if (Hints.hints_events[HINT_SEEN_STAIRS])
        {
            text << "In rare cases, you may have found no downstairs at all. "
                    "Try searching for secret doors in suspicious looking "
                    "spots; use <w>s</w>, <w>.</w> or for 100 turns with "
                    "<w>%</w> "
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
                    "<w>%</w> or <w>Shift-numpad 5</w>"
#ifdef USE_TILE
                    ", or by clicking on the stat area"
#endif
                    ".\n\n";
        }
        cmd.push_back(CMD_REST);
        break;

    case HINT_AUTO_EXCLUSION:
        // In the highly unlikely case the player encounters a
        // hostile statue or oklob plant during the hints mode...
        if (Hints.hints_explored)
        {
            // Hack: Reset hints_just_triggered, to force recursive calling of
            //       learned_something_new().
            Hints.hints_just_triggered = false;
            learned_something_new(HINT_AUTO_EXPLORE);
            Hints.hints_just_triggered = true;
        }
        text << "\nTo prevent autotravel or autoexplore taking you into "
                "dangerous territory, you can set travel exclusions by "
                "entering the map view (<w>%</w>) and then toggling the "
                "exclusion radius on the monster position with <w>e</w>. "
                "To make this easier some immobile monsters listed in the "
                "<w>auto_exclude</w> option (such as this one) are considered "
                "dangerous enough to warrant an automatic setting of an "
                "exclusion. It will be automatically cleared if you manage to "
                "kill the monster. You could also manually remove the "
                "exclusion with <w>%ee</w> but unless you remove this monster "
                "from the auto_exclude list, the exclusion will be reset the "
                "next turn.";
        cmd.push_back(CMD_DISPLAY_MAP);
        cmd.push_back(CMD_DISPLAY_MAP);
        break;

    case HINT_NEED_HEALING:
        text << "If you're low on hitpoints or magic and there's no urgent "
                "need to move, you can rest for a bit. Ideally, you should "
                "retreat to an area you've already explored and cleared "
                "of monsters before resting, since resting on the edge of "
                "the explored terrain increases the risk of rest being "
                "interrupted by a wandering monster. Press <w>%</w> or "
                "<w>shift-numpad-5</w>"
#ifdef USE_TILE
                ", or click into the stat area"
#endif
                " to do so.";
        cmd.push_back(CMD_REST);
        break;

    case HINT_NEED_POISON_HEALING:
        text << "Your poisoning could easily kill you, so now would be a "
                "good time to <w>%</w>uaff a potion of heal wounds or, "
                "better yet, a potion of healing. If you have seen neither "
                "of these so far, try unknown ones in your inventory. Good "
                "luck!";
        cmd.push_back(CMD_QUAFF);
        break;

    case HINT_INVISIBLE_DANGER:
        text << "Fighting against a monster you cannot see is difficult. "
                "Your best bet is probably a strategic retreat, be it via "
                "teleportation or by getting off the level. "
                "Or else, luring the monster into a corridor should at least "
                "make it easier for you to hit it.";

        // To prevent this text being immediately followed by the next one...
        Hints.hints_last_healed = you.num_turns - 30;
        break;

    case HINT_NEED_HEALING_INVIS:
        text << "You recently noticed an invisible monster, so unless you "
                "killed it or left the scene resting might not be safe. If you "
                "still need to replenish your hitpoints or magic, you'll have "
                "to quaff an appropriate potion. For normal resting you will "
                "first have to get away from the danger.";

        Hints.hints_last_healed = you.num_turns;
        break;

    case HINT_CAN_BERSERK:
        // Don't print this information if the player already knows it.
        if (Hints.hints_berserk_counter)
            return;

        text << "Against particularly difficult foes, you should use your "
                "Berserk <w>%</w>bility. Berserk will last longer if you "
                "kill a lot of monsters.";
        cmd.push_back(CMD_USE_ABILITY);
        break;

    case HINT_POSTBERSERK:
        text << "Berserking is extremely exhausting! It burns a lot of "
                "nutrition, and afterwards you are slowed down and "
                "occasionally even pass out. Press <w>%</w> to see your "
                "current exhaustion status.";
        cmd.push_back(CMD_DISPLAY_CHARACTER_STATUS);
        break;

    case HINT_RUN_AWAY:
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
                "you come back, so you might want to use a different set of "
                "stairs when you return.";

        if (you.religion == GOD_TROG && !you.berserk()
            && !you.duration[DUR_EXHAUSTED]
            && you.hunger_state >= HS_SATIATED)
        {
            text << "\nAlso, with "
                 << apostrophise(god_name(you.religion))
                 << " support you can use your Berserk ability (<w>%</w>) "
                    "to temporarily gain more hitpoints and greater "
                    "strength. ";
            cmd.push_back(CMD_USE_ABILITY);
        }
        break;

    case HINT_RETREAT_CASTER:
        text << "Without magical power you're unable to cast spells. While "
                "melee is a possibility, that's not where your strengths "
                "lie, so retreat (if possible) might be the better option.";

        if (_advise_use_wand())
        {
            text << "\n\nOr you could e<w>%</w>oke a wand to deal damage.";
            cmd.push_back(CMD_EVOKE);
        }
        break;

    case HINT_YOU_MUTATED:
        text << "Mutations can be obtained from several sources, among them "
                "potions, spell miscasts, and overuse of strong enchantments "
                "like invisibility. The only reliable way to get rid of "
                "mutations is with potions of cure mutation. There are about "
                "as many harmful as beneficial mutations, and most of them "
                "have three levels. Check your mutations with <w>%</w>.";
        cmd.push_back(CMD_DISPLAY_MUTATIONS);
        break;

    case HINT_NEW_ABILITY_GOD:
        switch (you.religion)
        {
        // Gods where first granted ability is active.
        case GOD_KIKUBAAQUDGHA: case GOD_YREDELEMNUL: case GOD_NEMELEX_XOBEH:
        case GOD_ZIN:           case GOD_OKAWARU:     case GOD_SIF_MUNA:
        case GOD_TROG:          case GOD_ELYVILON:    case GOD_LUGONU:
            text << "You just gained a divine ability. Press <w>%</w> to "
                    "take a look at your abilities or to use one of them.";
            cmd.push_back(CMD_USE_ABILITY);
            break;

        // Gods where first granted ability is passive.
        default:
            text << "You just gained a divine ability. Press <w>%</w> "
#ifdef USE_TILE
                    "or press <w>Shift</w> and <w>right-click</w> on the "
                    "player tile "
#endif
                    "to take a look at your abilities.";
            cmd.push_back(CMD_DISPLAY_RELIGION);
            break;
        }
        break;

    case HINT_NEW_ABILITY_MUT:
        text << "That mutation granted you a new ability. Press <w>%</w> to "
                "take a look at your abilities or to use one of them.";
        cmd.push_back(CMD_USE_ABILITY);
        break;

    case HINT_NEW_ABILITY_ITEM:
        text << "That item you just equipped granted you a new ability "
                "(un-equipping the item will remove the ability). "
                "Press <w>%</w> to take a look at your abilities or to "
                "use one of them.";
        cmd.push_back(CMD_USE_ABILITY);
        break;

    case HINT_CONVERT:
        _new_god_conduct();
        break;

    case HINT_GOD_DISPLEASED:
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
                "To do so, type <w>%</w>"
#ifdef USE_TILE
                " or press <w>Shift</w> and <w>right-click</w> on your avatar"
#endif
                ".";
        cmd.push_back(CMD_DISPLAY_RELIGION);
        break;

    case HINT_EXCOMMUNICATE:
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
                        "return to your old faith, you'll have to find an "
                        "altar dedicated to " << old_god_name << " where";
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
                            " didn't mind your converting to " << new_god_name
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
                            " didn't mind your converting to " << new_god_name
                         << ". That's because " << old_god_name << " is one of "
                            "the good gods who generally are rather forgiving "
                            "about change of faith - unless you switch over to "
                            "the path of evil, in which case their retribution "
                            "can be nasty indeed!";
                }
                else
                {
                    text << "Looks like " << old_god_name << " didn't "
                            "appreciate your converting to " << new_god_name
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
                        "your converting to " << new_god_name << "! (Actually, "
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

    case HINT_WIELD_WEAPON:
    {
        int wpn = you.equip[EQ_WEAPON];
        if (wpn != -1
            && you.inv[wpn].base_type == OBJ_WEAPONS
            && you.inv[wpn].cursed())
        {
            // Don't trigger if the wielded weapon is cursed.
            Hints.hints_events[seen_what] = true;
            Hints.hints_left++;
            return;
        }

        if (Hints.hints_type == HINT_RANGER_CHAR && wpn != -1
            && you.inv[wpn].base_type == OBJ_WEAPONS
            && you.inv[wpn].sub_type == WPN_BOW)
        {
            text << "You can easily switch between weapons in slots a and "
                    "b by pressing <w>%</w>.";
            cmd.push_back(CMD_WEAPON_SWAP);
        }
        else
        {
            text << "You can easily switch back to your weapon in slot a by "
                    "pressing <w>%</w>. To change the slot of an item, type "
                    "<w>%i</w> and choose the appropriate slots.";
            cmd.push_back(CMD_WEAPON_SWAP);
            cmd.push_back(CMD_ADJUST_INVENTORY);
        }
        break;
    }
    case HINT_FLEEING_MONSTER:
        if (Hints.hints_type != HINT_BERSERK_CHAR)
            return;

        text << "Now that monster is scared of you! Note that you do not "
                "absolutely have to follow it. Rather, you can let it run "
                "away. Sometimes, though, it can be useful to attack a "
                "fleeing creature by throwing something after it. If you "
                "have any darts or stones in your <w>%</w>nventory, you can "
                "look at one of them to read an explanation of how to do this.";
        cmd.push_back(CMD_DISPLAY_INVENTORY);
        break;

    case HINT_MONSTER_BRAND:
#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        if (const monster* m = monster_at(gc))
            tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_CAP_A), gc);
#endif
        text << "That monster looks a bit unusual. You might wish to examine "
                "it a bit more closely by "
#ifdef USE_TILE
                "hovering your mouse over its tile.";
#else
                "pressing <w>%</w> and moving the cursor onto its square.";
        cmd.push_back(CMD_LOOK_AROUND);
#endif
        break;

    case HINT_MONSTER_FRIENDLY:
    {
        const monster* m = monster_at(gc);

        if (!m)
            DELAY_EVENT;

#ifdef USE_TILE
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_CAP_A), gc);
#endif
        text << "That monster is friendly to you and will attack your "
                "enemies, though you'll get only half the experience for "
                "monsters killed by allies of what you'd get for killing them "
                "yourself. You can command your allies by pressing <w>%</w> "
                "to talk to them.";
        cmd.push_back(CMD_SHOUT);

        if (!mons_att_wont_attack(m->attitude))
        {
            text << "\nHowever, it is only <w>temporarily</w> friendly, and "
                    "will become dangerous again when this friendliness "
                    "wears off.";
        }
        break;
    }

    case HINT_MONSTER_SHOUT:
    {
        const monster* m = monster_at(gc);

        if (!m)
            DELAY_EVENT;

        // "Shouts" from zero experience monsters are boring, ignore
        // them.
        if (mons_class_flag(m->type, M_NO_EXP_GAIN))
        {
            Hints.hints_events[HINT_MONSTER_SHOUT] = true;
            return;
        }

        const bool vis = you.can_see(m);

#ifdef USE_TILE
        if (vis)
        {
            tiles.place_cursor(CURSOR_TUTORIAL, gc);
            tiles.add_text_tag(TAG_TUTORIAL, m->name(DESC_CAP_A), gc);
        }
#endif
        if (!vis)
        {
            text << "Uh-oh, some monster noticed you, either one that's "
                    "around a corner or one that's invisible. Plus, the "
                    "noise it made will alert other monsters in the "
                    "vicinity, who will come to check out what the commotion "
                    "was about.";
        }
        else if (mons_shouts(m->type, false) == S_SILENT)
        {
            text << "Uh-oh, that monster noticed you! Fortunately, it "
                    "didn't make any noise, but many monsters do make "
                    "noise when they notice you, which alerts other monsters "
                    "in the area, who will come to check out what the "
                    "commotion was about.";
        }
        else
        {
            text << "Uh-oh, that monster noticed you! Plus, the "
                    "noise it made will alert other monsters in the "
                    "vicinity, who will come to check out what the commotion "
                    "was about.";
        }
        break;
    }

    case HINT_MONSTER_LEFT_LOS:
    {
        const monster* m = monster_at(gc);

        if (!m || !you.can_see(m))
            DELAY_EVENT;

        text << m->name(DESC_CAP_THE, true) << " didn't vanish, but merely "
                "moved onto a square which you can't currently see. It's still "
                "nearby, unless something happens to it in the short amount of "
                "time it's out of sight.";
        break;
    }

    case HINT_SEEN_MONSTER:
    case HINT_SEEN_FIRST_OBJECT:
        // Handled in special functions.
        break;

    case HINT_SEEN_TOADSTOOL:
    {
        const monster* m = monster_at(gc);

        if (!m || !you.can_see(m))
            DELAY_EVENT;

        text << "Sometimes toadstools will grow on decaying corpses, and "
                "will wither away soon after appearing. Worshippers of "
                "Fedhas Madash, the plant god, can make use of them, "
                "but to everyone else they're just ugly dungeon decoration.";
        break;
    }

    case HINT_SEEN_ZERO_EXP_MON:
    {
        const monster* m = monster_at(gc);

        if (!m || !you.can_see(m))
            DELAY_EVENT;

        text << "That ";
#ifdef USE_TILE
        // need to highlight monster
        tiles.place_cursor(CURSOR_TUTORIAL, gc);
        tiles.add_text_tag(TAG_TUTORIAL, m);

        text << "is a ";
#else
        text << glyph_to_tagstr(get_mons_glyph(m)) << " is a ";
#endif
        text << m->name(DESC_PLAIN).c_str() << ". ";

        text << "While <w>technically</w> a monster, it's more like "
                "dungeon furniture, since it's harmless and doesn't move. "
                "If it's in your way you can attack and kill it like other "
                "monsters, but you won't get any experience for doing so. ";
        break;
    }

    case HINT_ABYSS:
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
                "encumbered or overburdened, then lighten up your load, and if "
                "the monsters are closing in, try to use items of speed to get "
                "away. Also, wherever possible, move in a direction slightly "
                "off from a compass direction (for example, north-by-northwest "
                "instead of north or northwest), as you're more likely to miss "
                "the exit if you keep heading solely in a compass direction.";
        break;

    case HINT_SPELL_MISCAST:
    {
        text << "You just miscast a spell. ";

        const item_def *shield = you.slot_item(EQ_SHIELD, false);
        if (!player_effectively_in_light_armour() || shield)
        {
            text << "Wearing heavy body armour or using a shield, especially a "
                    "large one, can severely hamper your spellcasting "
                    "abilities. You can check the effect of this by comparing "
                    "the success rates on the <w>%\?</w> screen with and "
                    "without the item being worn.\n\n";
            cmd.push_back(CMD_CAST_SPELL);
        }

        text << "If the spellcasting success chance is high (which can be "
                "checked by entering <w>%\?</w> or <w>%</w>) then a miscast "
                "merely means the spell is not working, along with a harmless "
                "side effect. "
                "However, for spells with a low success rate, there's a chance "
                "of contaminating yourself with magical energy, plus a chance "
                "of an additional harmful side effect. Normally this isn't a "
                "problem, since magical contamination bleeds off over time, "
                "but if you're repeatedly contaminated in a short amount of "
                "time you'll mutate or suffer from other ill side effects.\n\n";
        cmd.push_back(CMD_CAST_SPELL);
        cmd.push_back(CMD_DISPLAY_SPELLS);

        text << "Note that a miscast spell will still consume the full amount "
                "of MP and nutrition that a successfully cast spell would.";
        break;
    }
    case HINT_SPELL_HUNGER:
        text << "The spell you just cast made you hungrier; you can see how "
                "hungry spells make you by "
#ifdef USE_TILE
                "examining your spells in the spell display, or by "
#endif
                "entering <w>%\?!</w> or <w>%I</w>. "
                "The amount of nutrition consumed increases with the level of "
                "the spell and decreases depending on your intelligence stat "
                "and your Spellcasting skill. If both of these are high "
                "enough a spell might even not cost you any nutrition at all.";
        cmd.push_back(CMD_CAST_SPELL);
        cmd.push_back(CMD_DISPLAY_SPELLS);
        break;

    case HINT_GLOWING:
        text << "You've accumulated so much magical contamination that you're "
                "glowing! You usually acquire magical contamination from using "
                "some powerful magics, like invisibility, haste or potions of "
                "resistance. ";

        if (get_contamination_level() < 2)
        {
            text << "As long as the status only shows in grey nothing will "
                    "actually happen as a result of it, but as you continue "
                    "suffusing yourself with magical contamination you'll "
                    "eventually start glowing for real, which ";
        }
        else
        {
            text << "This normally isn't a problem as contamination slowly "
                    "bleeds off on its own, but it seems that you've "
                    "accumulated so much contamination over a short amount of "
                    "time that it ";
        }
        text << "can have nasty effects, such as mutate you or deal direct "
                "damage. In addition, glowing is going to make you much more "
                "noticeable.";
        break;

    case HINT_YOU_RESIST:
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
        cmd.push_back(CMD_RESISTS_SCREEN);
#endif
        break;

    case HINT_CAUGHT_IN_NET:
        text << "While you are held in a net, you cannot move around or engage "
                "monsters in combat. Instead, any movement you take is counted "
                "as an attempt to struggle free from the net. With a wielded "
                "bladed weapon you will be able to cut the net faster";

        if (Hints.hints_type == HINT_BERSERK_CHAR)
            text << ", especially if you're berserking while doing so";

        text << ". Small species may also wriggle out of a net, only damaging "
                "it a bit, so as to then <w>%</w>ire it at a monster.";
        cmd.push_back(CMD_FIRE);

        if (Hints.hints_type == HINT_MAGIC_CHAR)
        {
            text << " Note that casting spells is still very much possible, "
                    "as is using wands, scrolls and potions.";
        }
        else
        {
            text << " Note that using wands, scrolls and potions is still "
                    "very much possible.";
        }
        break;

    case HINT_YOU_SILENCE:
        redraw_screen();
        text << "While you are silenced, you cannot cast spells, read scrolls "
                "or use divine invocations. The same is true for any monster "
                "within the effect radius. The field of silence (recognizable "
                "by "
#ifdef USE_TILE
                "the special-looking frame tiles"
#else
                "different-coloured floor squares"
#endif
                ") is always centered on you and will move along with you. "
                "The radius will gradually shrink, eventually making you the "
                "only one affected, before the effect fades entirely.";
        break;

    case HINT_LOAD_SAVED_GAME:
    {
        text << "Welcome back! If it's been a while since you last played this "
                "character, you should take some time to refresh your memory "
                "of your character's progress. It is recommended to at least "
                "have a look through your <w>%</w>nventory, but you should "
                "also check ";
        cmd.push_back(CMD_DISPLAY_INVENTORY);

        std::vector<std::string> listed;
        if (you.spell_no > 0)
        {
            listed.push_back("your spells (<w>%?</w>)");
            cmd.push_back(CMD_CAST_SPELL);
        }
        if (!your_talents(false).empty())
        {
            listed.push_back("your <w>%</w>bilities");
            cmd.push_back(CMD_USE_ABILITY);
        }
        if (Hints.hints_type != HINT_MAGIC_CHAR || how_mutated())
        {
            listed.push_back("your set of mutations (<w>%</w>)");
            cmd.push_back(CMD_DISPLAY_MUTATIONS);
        }
        if (you.religion != GOD_NO_GOD)
        {
            listed.push_back("your religious standing (<w>%</w>)");
            cmd.push_back(CMD_DISPLAY_RELIGION);
        }

        listed.push_back("the message history (<w>%</w>)");
        listed.push_back("the character overview screen (<w>%</w>)");
        listed.push_back("the dungeon overview screen (<w>%</w>)");
        text << comma_separated_line(listed.begin(), listed.end()) << ".";
        cmd.push_back(CMD_REPLAY_MESSAGES);
        cmd.push_back(CMD_RESISTS_SCREEN);
        cmd.push_back(CMD_DISPLAY_OVERMAP);

        text << "\nAlternatively, you can dump all information pertaining to "
                "your character into a text file with the <w>%</w> command. "
                "You can then find said file in the <w>morgue/</w> directory (<w>"
             << you.your_name << ".txt</w>) and read it at your leisure. Also, "
                "such a file will automatically be created upon death (the "
                "filename will then also contain the date) but that won't be "
                "of much use to you now.";
        cmd.push_back(CMD_CHARACTER_DUMP);
        break;
    }
    default:
        text << "You've found something new (but I don't know what)!";
    }

    if (!text.str().empty())
    {
        std::string output = text.str();
        if (!cmd.empty())
            insert_commands(output, cmd);
        mpr(output, MSGCH_TUTORIAL);

        stop_running();
    }
}

formatted_string hints_abilities_info()
{
    std::ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    std::string broken = "This screen shows your character's set of talents. "
        "You can gain new abilities via certain items, through religion or by "
        "way of mutations. Activation of an ability usually comes at a cost, "
        "e.g. nutrition or Magic power. Press '<w>!</w>' or '<w>?</w>' to "
        "toggle between ability selection and description.";
    linebreak_string2(broken, _get_hints_cols());
    text << broken;

    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    return formatted_string::parse_string(text.str(), false);
}

// Explains the basics of the skill screen. Don't bother the player with the
// aptitude information. (Toggling is still possible, of course.)
void print_hints_skills_info()
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
    linebreak_string2(broken, _get_hints_cols());
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    formatted_string::parse_string(text.str(), false).display();
}

void print_hints_skills_description_info()
{
    textcolor(channel_to_colour(MSGCH_TUTORIAL));
    std::ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    std::string broken = "This screen shows the skill set of your character. "
                         "Press the letter of a skill to read its description, "
                         "or press <w>?</w> again to return to the skill "
                         "selection.";

    linebreak_string2(broken, _get_hints_cols());
    text << broken;
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    formatted_string::parse_string(text.str(), false).display();
}

// A short explanation of Crawl's target mode and its most important commands.
static std::string _hints_target_mode(bool spells = false)
{
    std::string result;
    result = "then be taken to target mode with the nearest monster or "
             "previous target already targeted. You can also cycle through "
             "all hostile monsters in sight with <w>+</w> or <w>-</w>. "
             "Once you're aiming at the correct monster, simply hit "
             "<w>f</w>, <w>Enter</w> or <w>.</w> to shoot at it. "
             "If you miss, <w>";

    command_type cmd;
    if (spells)
    {
        result += "%ap";
        cmd = CMD_CAST_SPELL;
    }
    else
    {
        result += "%f";
        cmd = CMD_FIRE;
    }

    result += "</w> fires at the same target again.";
    insert_commands(result, cmd, 0);

    return (result);
}

static std::string _hints_abilities(const item_def& item)
{
    std::string str = "To do this, ";

    std::vector<command_type> cmd;
    if (!item_is_equipped(item))
    {
        switch(item.base_type)
        {
        case OBJ_WEAPONS:
            str += "first <w>%</w>ield it";
            cmd.push_back(CMD_WIELD_WEAPON);
            break;
        case OBJ_ARMOUR:
            str += "first <w>%</w>ear it";
            cmd.push_back(CMD_WEAR_ARMOUR);
            break;
        case OBJ_JEWELLERY:
            str += "first <w>%</w>ut it on";
            cmd.push_back(CMD_WEAR_JEWELLERY);
            break;
        default:
            str += "<r>(BUG! this item shouldn't give an ability)</r>";
            break;
        }
        str += ", then ";
    }
    str += "enter the ability menu with <w>%</w>, and then "
           "choose the corresponding ability. Note that such an attempt of "
           "activation, especially by the untrained, is likely to fail.";
    cmd.push_back(CMD_USE_ABILITY);

    insert_commands(str, cmd);
    return (str);
}

static std::string _hints_throw_stuff(const item_def &item)
{
    std::string result;

    result  = "To do this, type <w>%</w> to fire, then <w>";
    result += item.slot;
    result += "</w> for ";
    result += (item.quantity > 1 ? "these" : "this");
    result += " ";
    result += item_base_name(item);
    result += (item.quantity > 1? "s" : "");
    result += ". You'll ";
    result += _hints_target_mode();

    insert_commands(result, CMD_FIRE, 0);
    return (result);
}

// Explains the most important commands necessary to use an item, and mentions
// special effects, etc.
// NOTE: For identified artefacts don't give all this information!
//       (The screen is likely to overflow.) Artefacts need special information
//       if they are evokable or grant resistances.
//       In any case, check whether we still have enough space for the
//       inscription prompt and answer.
void hints_describe_item(const item_def &item)
{
    std::ostringstream ostr;
    ostr << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    std::vector<command_type> cmd;

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
                    ostr << _hints_abilities(item);
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
                    cmd.push_back(CMD_RESISTS_SCREEN);
                    break;
                }
                return;
            }

            item_def *weap = you.slot_item(EQ_WEAPON, false);
            bool wielded = (weap && (*weap).slot == item.slot);
            bool long_text = false;

            if (!wielded)
            {
                ostr << "You can wield this weapon with <w>%</w>, or use "
                        "<w>%</w> to switch between the weapons in slot "
                        "a and b. (Use <w>%i</w> to adjust item slots.)";
                cmd.push_back(CMD_WIELD_WEAPON);
                cmd.push_back(CMD_WEAPON_SWAP);
                cmd.push_back(CMD_ADJUST_INVENTORY);

                // Weapon skill used by this weapon and the best weapon skill.
                int curr_wpskill, best_wpskill;

                // Maybe this is a launching weapon?
                if (is_range_weapon(item))
                {
                    // Then only compare with other launcher skills.
                    curr_wpskill = range_skill(item);
                    best_wpskill = best_skill(SK_SLINGS, SK_THROWING);
                }
                else
                {
                    // Compare with other melee weapons.
                    curr_wpskill = weapon_skill(item);
                    best_wpskill = best_skill(SK_SHORT_BLADES, SK_STAVES);
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
                         << "</w>. (Type <w>%</w> to see the skill "
                            "management screen for the actual numbers.)";

                    cmd.push_back(CMD_DISPLAY_SKILLS);
                    long_text = true;
                }
            }
            else // wielded weapon
            {
                if (is_range_weapon(item))
                {
                    ostr << "To attack a monster, ";
#ifdef USE_TILE
                    ostr << "if you have appropriate ammo quivered you can "
                            "<w>left mouse click</w> on the monster while "
                            "prssing the <w>Shift key</w>. Alternatively, "
                            "you can <w>left mouse click</w> on the tile for "
                            "the ammo you wish to fire, and then <w>left "
                            "mouse click</w> on the monster.\n\n";
                    ostr << "To launch ammunition using the keyboard, ";
#endif
                    ostr << "you only need to "
                            "<w>%</w>ire the appropriate type of ammunition. "
                            "You'll ";
                    ostr << _hints_target_mode();
                    cmd.push_back(CMD_FIRE);
                }
                else
                {
                    ostr << "To attack a monster, you can simply walk into it.";
                }
            }

            if (is_throwable(&you, item) && !long_text)
            {
                ostr << "\n\nSome weapons (including this one), can also be "
                        "<w>%</w>ired. ";
                cmd.push_back(CMD_FIRE);
                ostr << _hints_throw_stuff(item);
                long_text = true;
            }
            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n\nWeapons and armour that have unusual descriptions "
                     << "like this are much more likely to be of higher "
                     << "enchantment or have special properties, good or bad. "
                     << "The rarer the description, the greater the potential "
                     << "value of an item.";

                Hints.hints_events[HINT_SEEN_RANDART] = false;
            }
            if (item_known_cursed(item) && !long_text)
            {
                ostr << "\n\nOnce wielded, a cursed weapon won't leave your "
                        "hands again until the curse has been lifted by "
                        "reading a scroll of remove curse or one of the "
                        "enchantment scrolls.";

                if (!wielded && is_throwable(&you, item))
                    ostr << " (Throwing it is safe, though.)";

                Hints.hints_events[HINT_YOU_CURSED] = false;
            }
            Hints.hints_events[HINT_SEEN_WEAPON] = false;
            break;
       }
       case OBJ_MISSILES:
            if (is_throwable(&you, item))
            {
                ostr << item.name(DESC_CAP_YOUR)
                     << " can be <w>%</w>ired without the use of a launcher. ";
                ostr << _hints_throw_stuff(item);
                cmd.push_back(CMD_FIRE);
            }
            else if (is_launched(&you, you.weapon(), item))
            {
                ostr << "As you're already wielding the appropriate launcher, "
                        "you can simply ";
#ifdef USE_TILE
                ostr << "<w>left mouse click</w> on the monster you want "
                        "to hit while pressing the <w>Shift key</w>. "
                        "Alternatively, you can <w>left mouse click</w> on "
                        "this tile of the ammo you want to fire, and then "
                        "<w>left mouse click</w> on the monster you want "
                        "to hit.\n\n"

                        "To launch this ammo using the keyboard, you can "
                        "simply ";
#endif

                ostr << "<w>%</w>ire "
                     << (item.quantity > 1 ? "these" : "this")
                     << " " << item.name(DESC_BASENAME)
                     << (item.quantity > 1? "s" : "")
                     << ". You'll ";
                ostr << _hints_target_mode();
                cmd.push_back(CMD_FIRE);
            }
            else
            {
                ostr << "To shoot "
                     << (item.quantity > 1 ? "these" : "this")
                     << " " << item.name(DESC_BASENAME)
                     << (item.quantity > 1? "s" : "")
                     << ", first you need to <w>%</w>ield an appropriate "
                        "launcher.";
                cmd.push_back(CMD_WIELD_WEAPON);
            }
            Hints.hints_events[HINT_SEEN_MISSILES] = false;
            break;

       case OBJ_ARMOUR:
       {
            bool wearable = true;
            if (you.species == SP_CENTAUR && item.sub_type == ARM_BOOTS)
            {
                ostr << "As a Centaur you cannot wear boots. "
                        "(Type <w>%</w> to see a list of your mutations and "
                        "innate abilities.)";
                cmd.push_back(CMD_DISPLAY_MUTATIONS);
                wearable = false;
            }
            else if (you.species == SP_MINOTAUR && is_hard_helmet(item))
            {
                ostr << "As a Minotaur you cannot wear helmets. "
                        "(Type <w>%</w> to see a list of your mutations and "
                        "innate abilities.)";
                cmd.push_back(CMD_DISPLAY_MUTATIONS);
                wearable = false;
            }
            else if (item.sub_type == ARM_CENTAUR_BARDING)
            {
                ostr << "Only centaurs can wear centaur barding.";
                wearable = false;
            }
            else if (item.sub_type == ARM_NAGA_BARDING)
            {
                ostr << "Only nagas can wear naga barding.";
                wearable = false;
            }
            else
            {
                ostr << "You can wear pieces of armour with <w>%</w> and take "
                        "them off again with <w>%</w>"
#ifdef USE_TILE
                        ", or, alternatively, simply click on their tiles to "
                        "perform either action."
#endif
                        ".";
                cmd.push_back(CMD_WEAR_ARMOUR);
                cmd.push_back(CMD_REMOVE_ARMOUR);
            }

            if (Hints.hints_type == HINT_MAGIC_CHAR
                && get_armour_slot(item) == EQ_BODY_ARMOUR
                && !is_effectively_light_armour(&item))
            {
                ostr << "\nNote that body armour with high evasion penalties "
                        "may hinder your ability to learn and cast spells. "
                        "Light armour such as robes, leather armour or any "
                        "elven armour will be generally safe for any aspiring "
                        "spellcaster.";
            }
            else if (Hints.hints_type == HINT_MAGIC_CHAR
                     && is_shield(item))
            {
                ostr << "\nNote that shields will hinder you ability to "
                        "cast spells; the larger the shield, the bigger "
                        "the penalty.";
            }
            else if (Hints.hints_type == HINT_RANGER_CHAR
                     && is_shield(item))
            {
                ostr << "\nNote that wearing a shield will greatly decrease "
                        "the speed at which you can shoot arrows.";
            }

            if (!item_type_known(item)
                && (is_artefact(item)
                    || get_equip_desc(item) != ISFLAG_NO_DESC))
            {
                ostr << "\n\nWeapons and armour that have unusual descriptions "
                     << "like this are much more likely to be of higher "
                     << "enchantment or have special properties, good or bad. "
                     << "The rarer the description, the greater the potential "
                     << "value of an item.";

                Hints.hints_events[HINT_SEEN_RANDART] = false;
            }
            if (wearable)
            {
                if (item_known_cursed(item))
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
                    cmd.push_back(CMD_RESISTS_SCREEN);
                }
                if (gives_ability(item))
                {
                    ostr << "\n\nWhen worn, some types of armour (such as "
                            "this one) offer certain <w>%</w>bilities you can "
                            "activate. ";
                    ostr << _hints_abilities(item);
                    cmd.push_back(CMD_USE_ABILITY);
                }
            }
            Hints.hints_events[HINT_SEEN_ARMOUR] = false;
            break;
       }
       case OBJ_WANDS:
            ostr << "The magic within can be unleashed by evoking "
                    "(<w>%</w>) it.";
            cmd.push_back(CMD_EVOKE);
#ifdef USE_TILE
            ostr << " Alternatively, you can 1) <w>left mouse click</w> on "
                    "the monster you wish to target (or your player character "
                    "to target yourself) while pressing the <w>";
#ifdef UNIX
                    if (!tiles.is_fullscreen())
                        ostr << "Ctrl + Shift keys";
                    else
#endif
                        ostr << "Alt key";
            ostr << "</w> and pick the wand from the menu, or 2) "
                    "<w>left mouse click</w> on the wand tile and then "
                    "<w>left mouse click</w> on your target.";
#endif
            Hints.hints_events[HINT_SEEN_WAND] = false;
            break;

       case OBJ_FOOD:
            if (food_is_rotten(item) && !player_mutation_level(MUT_SAPROVOROUS))
            {
                ostr << "You can't eat rotten chunks, so you might just as "
                        "well ";
                if (!in_inventory(item))
                    ostr << "ignore them. ";
                else
                {
                    ostr << "<w>%</w>rop this. Use <w>%&</w> to select all "
                            "skeletons, corpses and rotten chunks in your "
                            "inventory. ";
                    cmd.push_back(CMD_DROP);
                    cmd.push_back(CMD_DROP);
                }
            }
            else
            {
                ostr << "Food can simply be <w>%</w>aten"
#ifdef USE_TILE
                        ", something you can also do by <w>left clicking</w> "
                        "on it"
#endif
                        ". ";
                cmd.push_back(CMD_EAT);

                if (item.sub_type == FOOD_CHUNK)
                {
                    ostr << "Note that most species refuse to eat raw meat "
                            "unless really hungry. ";

                    if (food_is_rotten(item))
                    {
                        ostr << "Even fewer can safely digest rotten meat, and "
                             "you're probably not part of that group.";
                    }
                }
            }
            Hints.hints_events[HINT_SEEN_FOOD] = false;
            break;

       case OBJ_SCROLLS:
            ostr << "Type <w>%</w> to read this scroll"
#ifdef USE_TILE
                    "or simply click on it with your <w>left mouse button</w>"
#endif
                    ".";
            cmd.push_back(CMD_READ);

            Hints.hints_events[HINT_SEEN_SCROLL] = false;
            break;

       case OBJ_JEWELLERY:
       {
            ostr << "Jewellery can be <w>%</w>ut on or <w>%</w>emoved "
                    "again"
#ifdef USE_TILE
                    ", though in Tiles, either can be done by clicking on the "
                    "item in your inventory"
#endif
                    ".";
            cmd.push_back(CMD_WEAR_JEWELLERY);
            cmd.push_back(CMD_REMOVE_JEWELLERY);

            if (item_known_cursed(item))
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
                cmd.push_back(CMD_RESISTS_SCREEN);
            }
            if (gives_ability(item))
            {
                ostr << "\n\nWhen worn, some types of jewellery (such as this "
                        "one) offer certain <w>%</w>bilities you can activate. ";
                cmd.push_back(CMD_USE_ABILITY);
                ostr << _hints_abilities(item);
            }
            Hints.hints_events[HINT_SEEN_JEWELLERY] = false;
            break;
       }
       case OBJ_POTIONS:
            ostr << "Type <w>%</w> to quaff this potion"
#ifdef USE_TILE
                    "or simply click on it with your <w>left mouse button</w>"
#endif
                    ".";
            cmd.push_back(CMD_QUAFF);
            Hints.hints_events[HINT_SEEN_POTION] = false;
            break;

       case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL)
            {
                ostr << "A manual can greatly help you in training a skill. "
                        "To use it, <w>%</w>ead it while your experience "
                        "pool (the number in brackets) is large. Note that "
                        "this will drain said pool, so only use this manual "
                        "if you think you need the skill in question.";
                cmd.push_back(CMD_READ);
            }
            else // It's a spellbook!
            {
                if (you.religion == GOD_TROG
                    && (item.sub_type != BOOK_DESTRUCTION
                        || !item_ident(item, ISFLAG_KNOW_TYPE)))
                {
                    if (!item_ident(item, ISFLAG_KNOW_TYPE))
                    {
                        ostr << "It's a book, you can <w>%</w>ead it.";
                        cmd.push_back(CMD_READ);
                    }
                    else
                    {
                        ostr << "A spellbook! You could <w>%</w>emorise some "
                                "spells and then cast them with <w>%</w>.";
                        cmd.push_back(CMD_MEMORISE_SPELL);
                        cmd.push_back(CMD_CAST_SPELL);
                    }
                    ostr << "\nAs a worshipper of "
                         << god_name(GOD_TROG)
                         << ", though, you might instead wish to burn this "
                            "tome of hated magic by using the corresponding "
                            "<w>%</w>bility. "
                            "Note that this only works on books that are lying "
                            "on the floor and not on your current square. ";
                    cmd.push_back(CMD_USE_ABILITY);
                }
                else if (!item_ident(item, ISFLAG_KNOW_TYPE))
                {
                    ostr << "It's a book, you can <w>%</w>ead it"
#ifdef USE_TILE
                            ", something that can also be achieved by clicking "
                            "on its tile in your inventory."
#endif
                            ".";
                    cmd.push_back(CMD_READ);
                }
                else if (item.sub_type == BOOK_DESTRUCTION)
                {
                    ostr << "This magical item can cause great destruction "
                            "- to you, or your surroundings. Use with care!";
                }
                else if (!you.skills[SK_SPELLCASTING])
                {
                    ostr << "A spellbook! You could <w>%</w>emorise some "
                            "spells and then cast them with <w>%</w>. ";
                    cmd.push_back(CMD_MEMORISE_SPELL);
                    cmd.push_back(CMD_CAST_SPELL);

                    ostr << "\nFor now, however, that will have to wait until "
                            "you've learned the basics of Spellcasting by "
                            "reading lots of scrolls.";
                }
                else // You actually can cast spells.
                {
                    if (player_can_memorise(item))
                    {
                        ostr << "Such a <lightblue>highlighted "
                                "spell</lightblue> can be <w>%</w>emorised "
                                "right away. ";
                        cmd.push_back(CMD_MEMORISE_SPELL);
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
                        ostr << "\n\nTo do magic, ";
#ifdef USE_TILE
                        ostr << "you can <w>left mouse click</w> on the "
                                "monster you wish to target (or on your "
                                "player character to cast a spell on "
                                "yourself) while pressing the <w>Control "
                                "key</w>, and then select a spell from the "
                                "menu. Or you can switch to the spellcasting "
                                "display by <w>clicking on the</w> "
                                "corresponding <w>tab</w>."
                                "\n\nAlternatively, ";
#endif
                        ostr << "you can type <w>%</w> and choose a "
                                "spell, e.g. <w>a</w> (check with <w>?</w>). "
                                "For attack spells you'll ";
                        cmd.push_back(CMD_CAST_SPELL);
                        ostr << _hints_target_mode(true);
                    }
                }
            }
            ostr << "\n";
            Hints.hints_events[HINT_SEEN_SPBOOK] = false;
            break;

       case OBJ_CORPSES:
            Hints.hints_events[HINT_SEEN_CARRION] = false;

            if (item.sub_type == CORPSE_SKELETON)
            {
                ostr << "Skeletons can be used as components for certain "
                        "necromantic spells. Apart from that, they are "
                        "largely useless.";

                if (in_inventory(item))
                {
                    ostr << " In the drop menu you can select all skeletons, "
                            "corpses, and rotten chunks in your inventory "
                            "at once with <w>%&</w>.";
                    cmd.push_back(CMD_DROP);
                }
                break;
            }

            ostr << "Corpses lying on the floor can be <w>%</w>hopped up with "
                    "a sharp implement to produce chunks for food (though they "
                    "may not be healthy)";
            cmd.push_back(CMD_BUTCHER);

            if (!food_is_rotten(item) && god_likes_fresh_corpses(you.religion))
            {
                ostr << ", or offered as a sacrifice to "
                     << god_name(you.religion)
                     << " <w>%</w>raying over them.";
                cmd.push_back(CMD_PRAY);
            }
            ostr << ". ";

            if (food_is_rotten(item))
            {
                ostr << "Rotten corpses won't be of any use to you, though, so "
                        "you might just as well ";
                if (!in_inventory(item))
                    ostr << "ignore them. ";
                else
                {
                    ostr << "<w>%</w>rop this. Use <w>%&</w> to select all "
                            "skeletons and rotten chunks or corpses in your "
                            "inventory. ";
                    cmd.push_back(CMD_DROP);
                    cmd.push_back(CMD_DROP);
                }
                ostr << "No god will accept such rotten sacrifice, either.";
            }
            else
            {
#ifdef USE_TILE
                ostr << " For an individual corpse in your inventory, the most "
                        "practical way to chop it up is to drop it by clicking "
                        "on it with your <w>left mouse button</w> while "
                        "<w>Shift</w> is pressed, and then repeat that command "
                        "for the corpse tile now lying on the floor.";
#endif
            }
            if (!in_inventory(item))
                break;

            ostr << "\n\nIf there are several items in your inventory you'd "
                    "like to drop, the most convenient way is to use the "
                    "<w>%</w>rop menu. On a related note, butchering "
                    "several corpses on a floor square is facilitated by "
                    "using the <w>%</w>hop prompt where <w>c</w> is a "
                    "valid synonym for <w>y</w>es or you can directly chop "
                    "<w>a</w>ll corpses.";
            cmd.push_back(CMD_DROP);
            cmd.push_back(CMD_BUTCHER);
            break;

       case OBJ_STAVES:
            if (item_is_rod(item))
            {
                if (!item_ident(item, ISFLAG_KNOW_TYPE))
                {
                    ostr << "\n\nTo find out what this rod might do, you have "
                            "to <w>%</w>ield it to see if you can use the "
                            "spells hidden within, then e<w>%</w>oke it to "
                            "actually do so"
#ifdef USE_TILE
                            ", both of which can be done by clicking on it"
#endif
                            ".";
                }
                else
                {
                    ostr << "\n\nYou can use this rod's magic by "
                            "<w>%</w>ielding and e<w>%</w>oking it"
#ifdef USE_TILE
                            ", both of which can be achieved by clicking on it"
#endif
                            ".";
                }
                cmd.push_back(CMD_WIELD_WEAPON);
                cmd.push_back(CMD_EVOKE_WIELDED);
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

                    cmd.push_back(CMD_RESISTS_SCREEN);
                    gives_resist = true;
                }

                if (!gives_resist && you.religion == GOD_TROG)
                {
                    ostr << "\n\nSeeing how "
                         << god_name(GOD_TROG, false)
                         << " frowns upon the use of magic, this staff will be "
                            "of little use to you and you might just as well "
                            "<w>%</w>rop it now.";
                    cmd.push_back(CMD_DROP);
                }
            }
            Hints.hints_events[HINT_SEEN_STAFF] = false;
            break;

       case OBJ_MISCELLANY:
            if (is_deck(item))
            {
                ostr << "Decks of cards are powerful magical items. Try "
                        "<w>%</w>ielding and e<w>%</w>oking it"
#ifdef USE_TILE
                        ", either of which can be done by clicking on it"
#endif
                        ". You can read about the effect of a card by "
                        "searching the game's database with <w>%/c</w>.";
                cmd.push_back(CMD_WIELD_WEAPON);
                cmd.push_back(CMD_EVOKE_WIELDED);
                cmd.push_back(CMD_DISPLAY_COMMANDS);
            }
            else
            {
                ostr << "Miscellaneous items sometimes harbour magical powers "
                        "that can be harnessed by e<w>%</w>oking the item.";
                cmd.push_back(CMD_EVOKE);
            }

            Hints.hints_events[HINT_SEEN_MISC] = false;
            break;

       default:
            return;
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";
    std::string broken = ostr.str();
    if (!cmd.empty())
        insert_commands(broken, cmd);
    linebreak_string2(broken, _get_hints_cols());
    cgotoxy(1, wherey() + 2);
    display_tagged_block(broken);
}        // hints_describe_item()

void hints_inscription_info(bool autoinscribe, std::string prompt)
{
    // Don't print anything if there's not enough space.
    if (wherey() >= get_number_of_lines() - 1)
        return;

    std::ostringstream text;
    text << "<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    bool longtext = false;
    if (wherey() <= get_number_of_lines() - (autoinscribe ? 10 : 8))
    {
        text << "\n"
         "Inscriptions are a powerful concept of Dungeon Crawl.\n"
         "You can inscribe items to differentiate them, or to comment on them, \n"
         "but also to set rules for item interaction. If you are new to Crawl, \n"
         "you can safely ignore this feature, though.";

        longtext = true;
    }

    if (autoinscribe && wherey() <= get_number_of_lines() - 6)
    {
        text << "\n"
         "Artefacts can be autoinscribed to give a brief overview of their \n"
         "known properties.";

        longtext = true;
    }
    text << "\n"
       "(In the main screen, press <w>?6</w> for more information.)\n";
    text << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    formatted_string::parse_string(text.str()).display();

    // Ask a second time, if it's been a longish interruption.
    if (longtext && !prompt.empty() && wherey() <= get_number_of_lines() - 2)
        formatted_string::parse_string(prompt).display();
}

// FIXME: With the new targeting system, the hints for interesting monsters
//        and features ("right-click/press v for more information") are no
//        longer getting displayed.
//        Players might still end up e'x'aming and particularly clicking on
//        but it's a lot more hit'n'miss now.
bool hints_pos_interesting(int x, int y)
{
    return (cloud_type_at(coord_def(x, y)) != CLOUD_NONE
            || _water_is_disturbed(x, y)
            || _hints_feat_interesting(grd[x][y]));
}

static bool _hints_feat_interesting(dungeon_feature_type feat)
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
    case DNGN_ENTER_PORTAL_VAULT:
        return (true);
    default:
        return (false);
    }
}

void hints_describe_pos(int x, int y)
{
    _hints_describe_disturbance(x, y);
    _hints_describe_cloud(x, y);
    _hints_describe_feature(x, y);
}

static void _hints_describe_feature(int x, int y)
{
    const dungeon_feature_type feat = grd[x][y];
    const coord_def            where(x, y);

    std::ostringstream ostr;
    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    bool boring = false;

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
            Hints.hints_events[HINT_SEEN_TRAP] = false;
            break;

       case DNGN_TRAP_NATURAL: // only shafts for now
            ostr << "The dungeon contains a number of natural obstacles such "
                    "as shafts, which lead one to three levels down. They "
                    "can't be disarmed, but you can safely pass over them "
                    "if you're levitating or flying.\n"
                    "If you want to jump down there, use <w>></w> to do so. "
                    "Be warned that getting back here might be difficult.";
            Hints.hints_events[HINT_SEEN_TRAP] = false;
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

            if (is_unknown_stair(where))
            {
                ostr << "\n\nYou have not yet passed through this particular "
                        "set of stairs. ";
            }

            Hints.hints_events[HINT_SEEN_STAIRS] = false;
            break;

       case DNGN_STONE_STAIRS_UP_I:
       case DNGN_STONE_STAIRS_UP_II:
       case DNGN_STONE_STAIRS_UP_III:
            if (you.absdepth0 < 1)
            {
                ostr << "These stairs lead out of the dungeon. Following them "
                        "will end the game. The only way to win is to "
                        "transport the fabled Orb of Zot outside.";
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
                if (is_unknown_stair(where))
                {
                    ostr << "\n\nYou have not yet passed through this "
                            "particular set of stairs. ";
                }
            }
            Hints.hints_events[HINT_SEEN_STAIRS] = false;
            break;

       case DNGN_ESCAPE_HATCH_DOWN:
       case DNGN_ESCAPE_HATCH_UP:
            ostr << "Escape hatches can be used to quickly leave a level with "
                    "<w><<</w> and <w>></w>, respectively. Note that you will "
                    "usually be unable to return right away.";

            Hints.hints_events[HINT_SEEN_ESCAPE_HATCH] = false;
            break;

       case DNGN_ENTER_PORTAL_VAULT:
            ostr << "This " << _describe_portal(where);
            Hints.hints_events[HINT_SEEN_PORTAL] = false;
            break;

       case DNGN_CLOSED_DOOR:
       case DNGN_DETECTED_SECRET_DOOR:
            if (!Hints.hints_explored)
            {
                ostr << "\nTo avoid accidentally opening a door you'd rather "
                        "remain closed during travel or autoexplore, you can "
                        "mark it with an exclusion from the map view "
                        "(<w>X</w>) with <w>ee</w> while your cursor is on the "
                        "grid in question. Such an exclusion will prevent "
                        "autotravel from ever entering that grid until you "
                        "remove the exclusion with another press of <w>Xe</w>.";
            }
            break;

       default:
            if (feat >= DNGN_ALTAR_FIRST_GOD && feat <= DNGN_ALTAR_LAST_GOD)
            {
                god_type altar_god = feat_altar_god(feat);

                // I think right now Sif Muna is the only god for whom
                // you can find altars early and who may refuse to accept
                // worship by one of the hint mode characters. (jpeg)
                if (altar_god == GOD_SIF_MUNA
                    && !player_can_join_god(altar_god))
                {
                    ostr << "As <w>p</w>raying on the altar will tell you, "
                         << god_name(altar_god) << " only accepts worship from "
                            "those who have already dabbled in magic. You can "
                            "find out more about this god by searching the "
                            "database with <w>?/g</w>.\n"
                            "For other gods, you'll be able to join the faith "
                            "by <w>p</w>raying at their altar.";
                }
                else if (you.religion == GOD_NO_GOD)
                {
                    ostr << "This is your chance to join a religion! In "
                            "general, the gods will help their followers, "
                            "bestowing powers of all sorts upon them, but many "
                            "of them demand a life of dedication, constant "
                            "tributes or entertainment in return.\n"
                            "You can get information about <w>"
                         << god_name(altar_god)
                         << "</w> by pressing <w>p</w> while standing on the "
                            "altar. Before taking up the responding faith "
                            "you'll be asked for confirmation.";
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
                    else // If we don't have anything to say, return early.
                        return;
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
                            ", or click with your <w>right mouse button</w> "
                            "on your avatar while pressing <w>Shift</w>"
#endif
                            ".";
                }
                Hints.hints_events[HINT_SEEN_ALTAR] = false;
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
            {
                // Describe blood-stains even for boring features.
                if (!is_bloodcovered(where))
                    return;
                boring = true;
            }
    }

    if (is_bloodcovered(where))
    {
        if (!boring)
            ostr << "\n\n";

        ostr << "Many forms of combat and some forms of magical attack "
                "will splatter the surrounings with blood (if the victim has "
                "any blood, that is). Some monsters can smell blood from "
                "a distance and will come looking for whatever the blood "
                "was spilled from.";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    std::string broken = ostr.str();
    linebreak_string2(broken, _get_hints_cols());
    display_tagged_block(broken);
}

static void _hints_describe_cloud(int x, int y)
{
    cloud_type ctype = cloud_type_at(coord_def(x, y));
    if (ctype == CLOUD_NONE)
        return;

    std::string cname = cloud_name_at_index(env.cgrid(coord_def(x, y)));

    std::ostringstream ostr;

    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    ostr << "The " << cname << " ";

    if (ends_with(cname, "s"))
        ostr << "are ";
    else
        ostr << "is ";

    bool need_cloud = false;
    switch (ctype)
    {
    case CLOUD_BLACK_SMOKE:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_TLOC_ENERGY:
    case CLOUD_PURPLE_SMOKE:
    case CLOUD_MIST:
    case CLOUD_MAGIC_TRAIL:
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
    linebreak_string2(broken, _get_hints_cols());
    display_tagged_block(broken);
}

static void _hints_describe_disturbance(int x, int y)
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
    linebreak_string2(broken, _get_hints_cols());
    display_tagged_block(broken);
}

static bool _water_is_disturbed(int x, int y)
{
    const coord_def c(x,y);
    const monster* mon = monster_at(c);

    if (!mon || grd(c) != DNGN_SHALLOW_WATER || !you.see_cell(c))
        return (false);

    return (!mon->visible_to(&you) && !mons_flies(mon));
}

bool hints_monster_interesting(const monster* mons)
{
    if (mons_is_unique(mons->type) || mons->type == MONS_PLAYER_GHOST)
        return (true);

    // Highlighted in some way.
    if (_mons_is_highlighted(mons))
        return (true);

    // The monster is (seriously) out of depth.
    if (you.level_type == LEVEL_DUNGEON
        && mons_level(mons->type) >= you.absdepth0 + 8)
    {
        return (true);
    }
    return (false);
}

void hints_describe_monster(const monster_info& mi, bool has_stat_desc)
{
    std::ostringstream ostr;
    ostr << "\n\n<" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    bool dangerous = false;
    if (mons_is_unique(mi.type))
    {
        ostr << "Did you think you were the only adventurer in the dungeon? "
                "Well, you thought wrong! These unique adversaries often "
                "possess skills that normal monster wouldn't, so be "
                "careful.\n\n";
        dangerous = true;
    }
    else if (mi.type == MONS_PLAYER_GHOST)
    {
        ostr << "The ghost of a deceased adventurer, it would like nothing "
                "better than to send you the same way.\n\n";
        dangerous = true;
    }
    else
    {
        const char ch = mons_base_char(mi.type);
        if (ch >= '1' && ch <= '5')
        {
            ostr << "This monster is a demon of the "
                 << (ch == '1' ? "highest" :
                     ch == '2' ? "second-highest" :
                     ch == '3' ? "middle" :
                     ch == '4' ? "second-lowest" :
                     ch == '5' ? "lowest"
                               : "buggy")
                 << " tier.\n\n";
        }

        // Don't call friendly monsters dangerous.
        if (!mons_att_wont_attack(mi.attitude))
        {
            // 8 is the default value for the note-taking of OOD monsters.
            // Since I'm too lazy to come up with any measurement of my own
            // I'll simply reuse that one.
            const int level_diff = mons_level(mi.type) - (you.absdepth0 + 8);

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
    }

    if (mi.is(MB_BERSERK))
    {
        ostr << "A berserking monster is bloodthirsty and fighting madly. "
                "Such a blood rage makes it particularly dangerous!\n\n";
        dangerous = true;
    }

    // Monster is highlighted.
    if (mi.attitude == ATT_FRIENDLY)
    {
        ostr << "Friendly monsters will follow you around and attempt to aid "
                "you in battle. You can order your allies by <w>t</w>alking "
                "to them.";

        if (!mons_att_wont_attack(mi.attitude))
        {
            ostr << "\n\nHowever, it is only <w>temporarily</w> friendly, "
                    "and will become dangerous again when this friendliness "
                    "wears off.";
        }
    }
    else if (dangerous)
    {
        if (!Hints.hints_explored && (mi.is(MB_WANDERING) || mi.is(MB_UNAWARE)))
        {
            ostr << "You can easily mark its square as dangerous to avoid "
                    "accidentally entering into its field of view when using "
                    "auto-explore or auto-travel. To do so, enter targeting "
                    "mode with <w>x</w> and then press <w>e</w> when your "
                    "cursor is hovering over the monster's grid. Doing so will "
                    "mark this grid and all surrounding ones within a radius "
                    "of 8 as \"excluded\" ones that explore or travel modus "
                    "won't enter.";
        }
        else
        {
            ostr << "This might be a good time to run away";

            if (you.religion == GOD_TROG && !you.berserk()
                && !you.duration[DUR_EXHAUSTED]
                && you.hunger_state >= HS_SATIATED)
            {
                ostr << " or apply your Berserk <w>a</w>bility";
            }
            ostr << ".";
        }
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mi.is(MB_STABBABLE))
    {
        ostr << "Apparently "
             << mons_pronoun((monster_type) mi.type, PRONOUN_NOCAP)
             << " has not noticed you - yet. Note that you do not have to "
                "engage every monster you meet. Sometimes, discretion is the "
                "better part of valour.";
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mi.is(MB_DISTRACTED))
    {
        ostr << "Apparently "
             << mons_pronoun((monster_type) mi.type, PRONOUN_NOCAP)
             << " has been distracted by something. You could use this "
                "opportunity to sneak up on this monster - or to sneak away.";
    }

    if (!dangerous && !has_stat_desc)
    {
        ostr << "\nThis monster doesn't appear to have any resistances or "
                "susceptibilities. It cannot fly and is of average speed. "
                "Examining other, possibly more high-level monsters can give "
                "important clues as to how to deal with them.";
    }

    ostr << "</" << colour_to_str(channel_to_colour(MSGCH_TUTORIAL)) << ">";

    std::string broken = ostr.str();
    linebreak_string2(broken, _get_hints_cols());
    display_tagged_block(broken);
}

void hints_observe_cell(const coord_def& gc)
{
    if (feat_is_escape_hatch(grd(gc)))
        learned_something_new(HINT_SEEN_ESCAPE_HATCH, gc);
    else if (feat_is_branch_stairs(grd(gc)))
        learned_something_new(HINT_SEEN_BRANCH, gc);
    else if (is_feature('>', gc))
        learned_something_new(HINT_SEEN_STAIRS, gc);
    else if (is_feature('_', gc))
        learned_something_new(HINT_SEEN_ALTAR, gc);
    else if (is_feature('^', gc))
        learned_something_new(HINT_SEEN_TRAP, gc);
    else if (feat_is_closed_door(grd(gc)))
        learned_something_new(HINT_SEEN_DOOR, gc);
    else if (grd(gc) == DNGN_ENTER_SHOP)
        learned_something_new(HINT_SEEN_SHOP, gc);
    else if (grd(gc) == DNGN_ENTER_PORTAL_VAULT)
        learned_something_new(HINT_SEEN_PORTAL, gc);

    const int it = you.visible_igrd(gc);
    if (it != NON_ITEM)
    {
        const item_def& item(mitm[it]);

        if (Options.feature_item_brand != CHATTR_NORMAL
            && (is_feature('>', gc) || is_feature('<', gc)))
        {
            learned_something_new(HINT_STAIR_BRAND, gc);
        }
        else if (Options.trap_item_brand != CHATTR_NORMAL
                 && is_feature('^', gc))
        {
            learned_something_new(HINT_TRAP_BRAND, gc);
        }
        else if (Options.heap_brand != CHATTR_NORMAL && item.link != NON_ITEM)
            learned_something_new(HINT_HEAP_BRAND, gc);
    }
}
