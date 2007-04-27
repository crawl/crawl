/*
 *  Created for Crawl Reference by JPEG on $Date: 2007-01-11$
 */

#include "tutorial.h"
#include <cstring>

#include "command.h"
#include "files.h"
#include "initfile.h"
#include "itemprop.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "newgame.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "spl-util.h"
#include "stuff.h"
#include "view.h"

//#define TUTORIAL_DEBUG
#define TUTORIAL_VERSION 110

static int get_tutorial_cols()
{
    int ncols = get_number_of_cols();
    return (ncols > 80? 80 : ncols);
}

void save_tutorial( FILE* fp )
{
    writeLong( fp, TUTORIAL_VERSION);
    writeShort( fp, Options.tutorial_type);
    for ( unsigned i = 0; i < TUT_EVENTS_NUM; ++i )
        writeShort( fp, Options.tutorial_events[i] );
}

void load_tutorial( FILE* fp )
{
    Options.tutorial_left = 0;

    int version = readLong(fp);
    Options.tutorial_type = readShort(fp);
    if (version != TUTORIAL_VERSION)
        return; 
    for ( long i = 0; i < TUT_EVENTS_NUM; ++i )
    {
        Options.tutorial_events[i] = readShort(fp);
        Options.tutorial_left += Options.tutorial_events[i];
    }
}

// override init file definition for some options
void init_tutorial_options()
{
    if (!Options.tutorial_left)
        return;

    Options.delay_message_clear = false;
    Options.auto_list = true;
}

// tutorial selection screen and choice
bool pick_tutorial()
{
    char keyn;
    bool printed = false;
    
tut_query:
    if (!printed)
    {
        clrscr();

        textcolor( WHITE );
        cprintf("You must be new here indeed!");

        cprintf(EOL EOL);
        textcolor( CYAN );
        cprintf("You can be:");
        cprintf(EOL EOL);

        textcolor( LIGHTGREY );

        for (int i = 0; i < TUT_TYPES_NUM; i++)
            print_tutorial_menu(i);
        
        textcolor( BROWN );
        cprintf(EOL "SPACE - Back to class selection; Bksp - Back to race selection; X - Quit"
                EOL "* - Random tutorial" 
                EOL);
        printed = true;
    }
    keyn = c_getch();

    if (keyn == '*')
        keyn = 'a' + random2(TUT_TYPES_NUM);

    // choose character for tutorial game and set starting values
    if (keyn >= 'a' && keyn <= 'a' + TUT_TYPES_NUM - 1)
    {
        Options.tutorial_type = keyn - 'a';
        you.species = get_tutorial_species(Options.tutorial_type);
        you.char_class = get_tutorial_job(Options.tutorial_type);

        // activate all triggers
        for (int i = 0; i < TUT_EVENTS_NUM; i++)
            Options.tutorial_events[i] = 1;
        Options.tutorial_left = TUT_EVENTS_NUM;

        // store whether explore, stash search or travelling was used
        Options.tut_explored = 1;
        Options.tut_stashes = 1;
        Options.tut_travel = 1; 

        // used to compare which fighting means was used most often
        Options.tut_spell_counter = 0;
        Options.tut_throw_counter = 0;
        Options.tut_berserk_counter = 0;
        Options.tut_melee_counter = 0;
        
        // for occasional healing reminders
        Options.tut_last_healed = 0;

        Options.random_pick = true; // random choice of starting spellbook
        Options.weapon = WPN_HAND_AXE; // easiest choice for fighters
        return true;
    }

    if (keyn == CK_BKSP || keyn == ' ')
    {
        // in this case, undo previous choices
//        set_startup_options(); 
        you.species = 0; you.char_class = JOB_UNKNOWN;
        Options.race = 0; Options.cls = 0;
    }

    switch (keyn)
    {
       case CK_BKSP:
           choose_race();
           break;
       case ' ':
           choose_class();
           break;
       case 'X':
           cprintf(EOL "Goodbye!");
           end(0);
           break;
       default:
           printed = false;
           goto tut_query;
    }
    
    return false;
}

void print_tutorial_menu(unsigned int type)
{
    char letter = 'a' + type;
    char desc[100];
    
    switch(type)
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

    cprintf("%c - %s %s %s" EOL, letter, species_name(get_tutorial_species(type), 1),
            get_class_name(get_tutorial_job(type)), desc);
}

unsigned int get_tutorial_species(unsigned int type)
{
    switch(type)
    {
      case TUT_BERSERK_CHAR:
          return SP_MINOTAUR;
      case TUT_MAGIC_CHAR:
          return SP_DEEP_ELF;
      case TUT_RANGER_CHAR:
          return SP_CENTAUR;
      default: // use something fancy for debugging
          return SP_KENKU;
    }
}

// TO DO: check whether job and species are compatible...
unsigned int get_tutorial_job(unsigned int type)
{
    switch(type)
    {
      case TUT_BERSERK_CHAR:
          return JOB_BERSERKER;
      case TUT_MAGIC_CHAR:
          return JOB_CONJURER;
      case TUT_RANGER_CHAR:
          return JOB_HUNTER;
      default: // use something fancy for debugging
          return JOB_NECROMANCER;
    }
}

// the tutorial welcome screen
static formatted_string tut_starting_info(unsigned int width)
{
    std::string result; // the entire page
    std::string text; // one paragraph
    
    result += "<white>Welcome to Dungeon Crawl!</white>" EOL EOL;
    
    text += "Your object is to lead a ";
    snprintf(info, INFO_SIZE, "%s %s ", species_name(get_tutorial_species(Options.tutorial_type), 1), 
             get_class_name(get_tutorial_job(Options.tutorial_type)));
    text += info;
    text += "safely through the depths of the dungeon, retrieving the fabled orb of Zot and "
            "returning it to the surface. In the beginning, however, let discovery be your "
            "main goal. Try to delve as deeply as possible but beware; death lurks around "
            "every corner." EOL EOL;
    linebreak_string2(text, width);
    result += formatted_string::parse_string(text);
                
    result += "For the moment, just remember the following keys and their functions:" EOL;
    result += "  <white>?</white> - shows the items and the commands" EOL;
    result += "  <white>S</white> - saves the game, to be resumed later (but note that death is permanent)" EOL;
    result += "  <white>x</white> - examine something in your vicinity" EOL EOL;

    text = "This tutorial will help you play Crawl without reading any documentation. "
           "If you feel intrigued, there is more information available in these files "
           "(all of which can also be read in-game):" EOL;
    linebreak_string2(text, width);
    result += formatted_string::parse_string(text);

    result += "  <lightblue>readme.txt</lightblue>         - A very short guide to Crawl." EOL;
    result += "  <lightblue>manual.txt</lightblue>         - This contains all details on races, magic, skills, etc." EOL;
    result += "  <lightblue>crawl_options.txt</lightblue>  - Crawl's interface is highly configurable. This document " EOL;
    result += "                       explains all the options." EOL;
    result += EOL;        
    result += "Press <white>Space</white> to proceed to the basics (the screen division and movement)." EOL;
    result += "Press <white>Esc</white> to fast forward to the game start.";


    return formatted_string::parse_block(result);
}

#ifdef TUTORIAL_DEBUG
static std::string tut_debug_list(int event)
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
      case TUT_SEEN_JEWELLERY:
          return "seen first jewellery";
      case TUT_SEEN_MISC:
          return "seen first misc. item";
      case TUT_SEEN_MONSTER:
          return "seen first monster";
      case TUT_SEEN_STAIRS:
          return "seen first stairs";
      case TUT_SEEN_TRAPS:
          return "encountered a trap";
      case TUT_SEEN_ALTAR:
          return "seen an altar";
      case TUT_SEEN_SHOP:
          return "seen a shop";
      case TUT_SEEN_DOOR:
          return "seen a closed door";
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
      case TUT_YOU_CURSED:
          return "had something cursed";
      case TUT_YOU_HUNGRY:
          return "felt hungry";
      case TUT_YOU_STARVING:
          return "were starving";
      case TUT_MAKE_CHUNKS:
          return "learned about chunks";
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
      case TUT_POSTBERSERK:
          return "learned about Berserk aftereffects";
      case TUT_RUN_AWAY:
          return "were told to run away";
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
      default:
          return "faced a bug";
    }
}

static formatted_string tutorial_debug()
{
    std::string result;
    bool lbreak = false;
    snprintf(info, INFO_SIZE, "Tutorial Debug Screen");
    
    int i = get_tutorial_cols()/2-1 - strlen(info) / 2;
    result += std::string(i, ' ');
    result += "<white>";
    result += info;
    result += "</white>" EOL EOL;

    result += "<lightblue>";
    for (i=0; i < TUT_EVENTS_NUM; i++)
    {
        snprintf(info, INFO_SIZE, "%d: %d (%s)", 
                 i, Options.tutorial_events[i], tut_debug_list(i).c_str());
        result += info;
        
        // break text into 2 columns where possible
        if (strlen(info) >= get_tutorial_cols()/2 || lbreak)
        {
            result += EOL;
            lbreak = false;
        }
        else
        {
            result += std::string(get_tutorial_cols()/2-1 - strlen(info), ' ');
            lbreak = true;
        }
    }
    result += "</lightblue>" EOL EOL;
 
    snprintf(info, INFO_SIZE, "tutorial_left: %d\n", Options.tutorial_left);
    result += info;
    result += EOL;
 
    snprintf(info, INFO_SIZE, "You are a %s %s, and the tutorial will reflect that.",
             species_name(get_tutorial_species(Options.tutorial_type), 1), 
             get_class_name(get_tutorial_job(Options.tutorial_type)));
 
    result += info;

    return formatted_string::parse_string(result);
}
#endif // debug

static formatted_string tutorial_map_intro()
{
    std::string result;

    result =  "<magenta>"
              "What you see here is the typical Crawl screen. The upper left map "
              "shows your hero as the <white>@<magenta> in the center. The parts "
              "of the map you remember but cannot currently see, will be greyed "
              "out."
              "</magenta>" EOL;
    result += " --more--                               Press <white>Escape</white> to skip the basics";

    linebreak_string2(result,get_tutorial_cols());
    return formatted_string::parse_block(result, false);
}

static formatted_string tutorial_stats_intro()
{
    std::string result;
    result += "<magenta>";

    result += "To the right, important properties of \n";
    result += "the character are displayed. The most \n";
    result += "basic one is your health, measured as \n";
    snprintf(info, INFO_SIZE, "<white>HP: %d/%d<magenta>. ",
             you.hp, you.hp_max);
    result += info; 
    if (Options.tutorial_type==TUT_MAGIC_CHAR)
        result += "  ";
    result += "These are your current out \n";
    result += "of maximum health points. When health \n";
    result += "drops to zero, you die.               \n";
    snprintf(info, INFO_SIZE, "<white>Magic: %d/%d<magenta>", you.magic_points, you.max_magic_points);
    result += info; 
    result += " is your energy for casting \n";
    result += "spells, although more mundane actions \n";
    result += "often draw from Magic, too.           \n";
    result += "Further down, <white>Str<magenta>ength, <white>Dex<magenta>terity and \n";
    result += "<white>Int<magenta>elligence are shown and provide an \n";
    result += "all-around account of the character's \n";
    result += "attributes.                           \n";

    result += "<lightgrey>";
    result += "                                      \n";
    result += " --more--                               Press <white>Escape</white> to skip the basics\n";
    result += "                                      \n";
    result += "                                      \n";

    return formatted_string::parse_block(result, false);
}

static formatted_string tutorial_message_intro()
{
    std::string result;

    result =  "<magenta>"
              "This lower part of the screen is reserved for messages. Everything "
              "related to the tutorial is shown in this colour. If you missed "
              "something, previous messages can be read again with "
              "<white>Ctrl-P<magenta>."
              "</magenta>" EOL;
    result += " --more--                               Press <white>Escape</white> to skip the basics";

    linebreak_string2(result,get_tutorial_cols());
    return formatted_string::parse_block(result, false);
}

static void tutorial_movement_info()
{
    std::string text;
    text =  "To move your character, use the numpad; try Numlock both on and off. "
            "If your system has no number pad, or if you are familiar with the vi "
            "keys, movement is also possible with <w>hjklyubn<magenta>. A basic "
            "command list can be found under <w>?<magenta>, and the most important "
            "commands will be explained to you as it becomes necessary. ";
    mesclr();
    print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
}

// copied from display_mutations and adapted
void tut_starting_screen()
{

    int x1, x2, y1, y2;
    int MAX_INFO = 4;
#ifdef TUTORIAL_DEBUG
    MAX_INFO++;
#endif
    char ch;

    for (int i=0; i<=MAX_INFO; i++)
    {
        x1 = 1; y1 = 1;
        x2 = get_tutorial_cols(); y2 = get_number_of_lines();

        if (i==1 || i==3)
        {
            y1 = 18; // message window
        }
        else if (i==2)
        {
            x2 = 40; // map window
            y2 = 18;
        }

        if (i==0)
            clrscr();

        gotoxy(x1,y1);

        if (i==0)
            tut_starting_info(x2).display();
        else if (i==1)
            tutorial_map_intro().display();
        else if (i==2)
            tutorial_stats_intro().display();
        else if (i==3)
            tutorial_message_intro().display();
        else if (i==4)
            tutorial_movement_info();
        else
        {
#ifdef TUTORIAL_DEBUG
            clrscr();
            gotoxy(x1,y1);
            tutorial_debug().display();
#else
            continue;
#endif
        }

        ch = c_getch();
        redraw_screen();
        if (ch == ESCAPE)
            break;
    }
}

// once a tutorial character dies, offer some playing hints
void tutorial_death_screen()
{
    Options.tutorial_left = 0;
    std::string text;

    mpr( "Condolences! Your character's premature death is a sad, but "
         "common occurence in Crawl. Rest assured that with diligence and "
         "playing experience your characters will last longer.\n", MSGCH_TUTORIAL);
    mpr( "Perhaps the following advice can improve your playing style:", MSGCH_TUTORIAL);
    more();

    if (Options.tutorial_type == TUT_MAGIC_CHAR 
          && Options.tut_spell_counter < Options.tut_melee_counter )
        text = "As a Conjurer your main weapon should be offensive magic. Cast "
                "spells more often! Remember to rest when your Magic is low.";
    else if (Options.tutorial_type == TUT_BERSERK_CHAR 
    	     && Options.tut_berserk_counter < 1 )
    {
        text =  "Don't forget to go berserk when fighting particularly "
                "difficult foes. It is risky, but makes you faster "
                "and beefier. Also try to pray prior to battles so that ";
        text += god_name(you.religion);
        text += " will soon feel like providing more abilities.";
    }
    else if (Options.tutorial_type == TUT_RANGER_CHAR 
    	     && 2*Options.tut_throw_counter < Options.tut_melee_counter )
        text = "Your bow and arrows are extremely powerful against "
               "distant monsters. Be sure to collect all arrows lying "
               "around in the dungeon."; 
    else
    {
      int hint = random2(6);
      // If a character has been unusually busy with projectiles and spells
      // give some other hint rather than the first one.
      if (Options.tut_throw_counter + Options.tut_spell_counter >= Options.tut_melee_counter
            && hint == 0)
         hint = random2(5)+1;

      switch(hint)
      {
        case 0:
            text = "Always consider using projectiles, wands or spells before "
                   "engaging monsters in close combat.";
            break;
        case 1:
            text = "Learn when to run away from things you can't handle - this is "
                   "important! It is often wise to skip a particularly dangerous "
                   "level. But don't overdo this as monsters will only get harder "
                   "the deeper you delve.";
            break;
        case 2:
            text = "Rest between encounters. In Crawl, searching and resting are "
                   "one and the same. To search for one turn, press <w>s<magenta>, "
                   "<w>.<magenta>, <w>delete<magenta> or <w>keypad-5<magenta>. "
                   "Pressing <w>5<magenta> or <w>shift-and-keypad-5<magenta> will "
                   "let you rest for a longer time (you will stop resting when "
                   "fully healed).";
            break;
        case 3:
            text =  "Remember to use those scrolls, potions or wands you've found. "
                    "Very often, you cannot expect to identify everything with the "
                    "scroll only. Learn to improvise: identify through usage.";
            break;
        case 4:
            text =  "If a particular encounter feels overwhelming don't forget to "
                    "use emergency items early on. A scroll of teleportation or a "
                    "potion of speed can really save your bacon.";
            break;
        case 5:
            text =  "Never fight more than one monster, if you can help it. Always "
                    "back into a corridor so that they are forced to fight you one "
                    "on one.";
            break;
        default:
            text =  "Sorry, no hint this time, though there should have been one.";
      }
    }
    print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
    more();

    mpr( "See you next game!", MSGCH_TUTORIAL);

    for ( long i = 0; i < TUT_EVENTS_NUM; ++i )
        Options.tutorial_events[i] = 0;
}

// if a character survives until Xp 5, the tutorial is declared finished
// and they get a more advanced playing hint, depending on what they might
// know by now
void tutorial_finished()
{
    std::string text;

    Options.tutorial_left = 0;
    text =  "Congrats! You survived until the end of this tutorial - be sure to "
            "try the other ones as well. Note that the help screen (<w>?<magenta>) "
            "will look different from now on. Here's a last playing hint:";
    print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
    more();

    if (Options.tut_explored)
    {
        text =  "Walking around and exploring levels gets easier by using "
                "auto-explore (<w>Ctrl-O<magenta>). You can even make Crawl "
                "automatically pick up interesting items by setting the "
                "option <w>explore_greedy=true<magenta> in the init file.";
    }
    else if (Options.tut_travel)
    {
        text =  "There is a convenient way for travelling between far away "
                "dungeon levels: press <w>Ctrl-G<magenta> and enter the desired "
                "destination. If your travel gets interrupted, issueing "
                "<w>Ctrl-G Enter<magenta> will continue it.";
    }
    else if (Options.tut_stashes)
    {
        text =  "You can search among all items existing in the dungeon with "
                "the <w>Ctrl-F<magenta> command. For example, "
                "<w>Ctrl-F \"knife\"<magenta> will list all knives. You can then "
                "travel to one of the spots. It is even possible to enter words "
                "like <w>\"shop\"<magenta> or <w>\"altar\"<magenta>.";
    }
    else
    {
        int hint = random2(3);
        switch (hint)
        {
          case 0:
              text =  "The game keeps an automated logbook for your characters. Use "
                      "<w>?:<magenta> to read it. You can enter notes manually with "
                      "the <w>:<magenta> command. Once your character perishes, two "
                      "morgue files are left in the <w>/morgue<magenta> directory. "
                      "The one ending in .txt contains a copy of your logbook. "
                      "During play, you can create a dump file with <w>#<magenta>.";
              break;
          case 1:
              text =  "Crawl has a macro function built in: press <w>~m<magenta> "
                      "to define a macro by first specifying a trigger key (say, "
                      "<w>F1<magenta>) and a command sequence, for example "
                      "<w>Za+.<magenta>. The latter will make the <w>F1<magenta> "
                      "key always zap the spell in slot a at the nearest monster. "
                      "For more information on macros, type <w>?~<magenta>.";
              break;
          case 2:
              text =  "The interface can be greatly customised. All options are "
                      "explained in the file <w>crawl_options.txt<magenta> which "
                      "can be found in the <w>/docs<magenta> directory. The "
                      "options themselves are set in <w>init.txt<magenta> or "
                      "<w>.crawlrc<magenta>. Crawl will complain if it can't "
                      "find either file.\n";
               break;
          default:
              text =  "Oops... No hint for now. Better luck next time!";
        } 
    }
    print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
    more();

    for ( long i = 0; i < TUT_EVENTS_NUM; ++i )
        Options.tutorial_events[i] = 0;
}

// occasionally remind religious characters of praying
void tutorial_prayer_reminder()
{
    if (Options.tut_just_triggered)
        return;

    if (coinflip()) // always would be too annoying
    {
        std::string text;
        text =  "Remember to <w>p<magenta>ray before battle, so as to dedicate "
                "your kills to ";
        text += god_name(you.religion);
        text += ". Should the monster leave a corpse, consider "
                "<w>D<magenta>issecting it as a sacrifice to ";
        text += god_name(you.religion);
        text += ", as well.";
        print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
    }
}

// occasionally remind injured characters of resting
void tutorial_healing_reminder()
{
    if (you.poisoning && 2*you.hp < you.hp_max)
    {
        if (Options.tutorial_events[TUT_NEED_POISON_HEALING])
            learned_something_new(TUT_NEED_POISON_HEALING);
    }
    else
    {
        if (Options.tutorial_events[TUT_NEED_HEALING]) 
            learned_something_new(TUT_NEED_HEALING);
        else if (you.num_turns - Options.tut_last_healed >= 50 && !you.poisoning)
        {
            if (Options.tut_just_triggered)
                return;

            std::string text;
            text =  "Remember to rest between fights and to enter unexplored "
                    "terrain with full hitpoints and magic. For resting, "
                    "press <w>5<magenta> or <w>Shift-numpad 5<magenta>.";
            print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
        }
        Options.tut_last_healed = you.num_turns;
    }
}

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
      default: /* nothing to be done */
          return;
    }
}

static std::string colour_to_tag(int col, bool closed = false)
{
    std::string tag = "<";
    if (closed)
        tag += "/";
    tag += colour_to_str(col);
    tag += ">";
    
    return tag;
}

void tutorial_first_monster(const monsters& mon)
{
    if (!Options.tutorial_events[TUT_SEEN_MONSTER])
        return;

    unsigned short ch, col;
    get_mons_glyph(&mon, &ch, &col);

    std::string text = "<magenta>That ";
    text += colour_to_tag(col);
    text += ch;
    text += "<magenta> is a monster, usually depicted by a letter. Some typical "
            "early monsters look like <brown>r<magenta>, <w>g<magenta>, "
            "<lightgray>b<magenta> or <brown>K<magenta>. You can gain "
            "information about it by pressing <w>x<magenta>, moving the cursor "
            "on the monster and then pressing <w>v<magenta>. To attack it with "
            "your wielded weapon, just move into it.";
    print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);

    more();

    if (Options.tutorial_type == TUT_RANGER_CHAR)
    {
        text =  "However, as a hunter you will want to deal with it using your "
                "bow. Do this as follows: <w>wbf+.<magenta> where <w>wb<magenta> "
                "wields the bow, <w>f<magenta> fires appropriate ammunition "
                "(your arrows) and enters targeting mode. <w>+<magenta> and "
                "<w>-<magenta> allow you to select the proper monster. Finally, "
                "<w>Enter<magenta> or <w>.<magenta> fire. If you miss, "
                "<w>ff<magenta> fires at the previous target again.";
        print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
    }
    else if (Options.tutorial_type == TUT_MAGIC_CHAR)
    {
        text =  "However, as a conjurer you will want to deal with it using magic."
                "Do this as follows: <w>Za+.<magenta> where <w>Za<magenta> zaps "
                "the first spell you know, ";
        text += spell_title(get_spell_by_letter('a'));
        text += ", and enters targeting mode. <w>+<magenta> and <w>-<magenta> "
                "allow you to select the proper target. Finally, <w>Enter<magenta> "
                "or <w>.<magenta> fire. If you miss, <w>Zap<magenta> will "
                "fire at the previous target again.";
        print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
    }

    Options.tutorial_events[TUT_SEEN_MONSTER] = 0;
    Options.tutorial_left--;
    Options.tut_just_triggered = 1;
}

void tutorial_first_item(const item_def& item)
{
    if (!Options.tutorial_events[TUT_SEEN_FIRST_OBJECT] || Options.tut_just_triggered)
        return;

    unsigned short ch, col;
    get_item_glyph(&item, &ch, &col);

    std::string text = "<magenta>That ";
    text += colour_to_tag(col);
    text += ch;
    text += "<magenta> is an item. If you move there and press <w>g<magenta> or "
            "<w>,<magenta> you will pick it up. Generally, items are shown by "
            "non-letter symbols like <w>%?!\"=()[<magenta>. Once it is in your "
            "inventory, you can drop it again with <w>d<magenta>. Several types "
            "of objects will usually be picked up automatically.";
    print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);

    Options.tutorial_events[TUT_SEEN_FIRST_OBJECT] = 0;
    Options.tutorial_left--;
    Options.tut_just_triggered = 1;
}

// Here most of the tutorial messages for various triggers are handled.
void learned_something_new(tutorial_event_type seen_what, int x, int y)
{ 
    // already learned about that
    if (!Options.tutorial_events[seen_what])
        return;

    // don't trigger twice in the same turn 
    if (Options.tut_just_triggered)
        return;

    std::string text;
    unsigned short ch, colour;
    const int ex = x - you.x_pos + 9;
    const int ey = y - you.y_pos + 9;
    int object;

    switch(seen_what)
    {
      case TUT_SEEN_POTION:
          text =  "You have picked up your first potion ('<w>!<magenta>'). Use "
                  "<w>q<magenta> to drink (quaff) it.";
          break;
      case TUT_SEEN_SCROLL:
          text =  "You have picked up your first scroll ('<w>?<magenta>'). Type "
                  "<w>r<magenta> to read it.";
          break;
      case TUT_SEEN_WAND:
          text =  "You have picked up your first wand ('<w>/<magenta>'). Type "
                  "<w>z<magenta> to zap it.";
          break;
      case TUT_SEEN_SPBOOK:
          get_item_symbol(DNGN_ITEM_BOOK, &ch, &colour);
          text =  "You have picked up a spellbook ('<w>";
          text += ch;
          text += "'<magenta>). You can read it by typing <w>r<magenta>, "
                  "memorise spells via <w>M<magenta> and cast a memorised spell "
                  "with <w>Z<magenta>.";

          if (!you.skills[SK_SPELLCASTING])
          {
              text += "\nHowever, first you will have to get accustomed to "
                      "spellcasting by reading lots of scrolls.";
          }
          break;
      case TUT_SEEN_WEAPON:
          text =  "This is the first weapon ('<w>(<magenta>') you've picked up. "
                  "Use <w>w<magenta> to wield it, but be aware that this weapon "
                  "might train a different skill from your current one. You can "
                  "view the weapon's properties with <w>v<magenta>.";
          if (Options.tutorial_type == TUT_BERSERK_CHAR)
          {
              text += "\nAs you're already trained in Axes you should stick with "
                      "these. Checking other axes can be worthwhile.";
          }
          break;
      case TUT_SEEN_MISSILES:
          text =  "This is the first stack of missiles ('<w>)<magenta>') you've "
                  "picked up. Darts can be thrown by hand, but other missile types "
                  "like arrows and needles require a launcher and training in "
                  "using it to be really effective. <w>v<magenta> gives more "
                  "information about both missiles and launcher.";
          if (Options.tutorial_type == TUT_RANGER_CHAR)
          {
              text += "\nAs you're already trained in Bows you should stick with "
                      "arrows and collect more of them in the dungeon.";
          }
          else if (Options.tutorial_type == TUT_MAGIC_CHAR)
          {
              text += "\nHowever, as a spellslinger you don't really need another "
                      "type of ranged attack, unless there's another effect in "
                      "addition to damage.";
          }
          else
          {
              text += "\nFor now you might be best off with sticking to darts or "
                      "stones for ranged attacks.";
          }
          break;
      case TUT_SEEN_ARMOUR:
          text =  "This is the first piece of armour ('<w>[<magenta>') you've "
                  "picked up. Use <w>W<magenta> to wear it and <w>T<magenta> to "
                  "take it off again. You can view its properties with "
                  "<w>v<magenta>.";

          if (you.species == SP_CENTAUR || you.species == SP_MINOTAUR)
          {
              snprintf( info, INFO_SIZE, "\nNote that as a %s, you will be unable to wear %s.",
              species_name(you.species, 1), you.species == SP_CENTAUR ? "boots" : "helmets");
              text += info;
          }
          break;
      case TUT_SEEN_RANDART:
          text =  "Weapons and armour that have unusual descriptions like this "
                  "are much more likely to be of higher enchantment or have "
                  "special properties, good or bad. The rarer the description, "
                  "the greater the potential value of an item.";
          break;
      case TUT_SEEN_FOOD:
          text =  "You have picked up some food ('<w>%<magenta>'). You can eat it "
                  "by typing <w>e<magenta>.";
          break;
      case TUT_SEEN_CARRION:
          text =  "You have picked up a corpse ('<w>%<magenta>'). When a corpse "
                  "is lying on the ground, you can <w>D<magenta>issect with a "
                  "sharp implement. Once hungry you can <w>e<magenta>at the "
                  "resulting chunks (though they may not be healthy).";
          if (Options.tutorial_type == TUT_BERSERK_CHAR)
          {
             text += " During prayer you can offer corpses to ";
             text += god_name(you.religion);
             text += " by dissecting them, as well.";
          }
          break;
      case TUT_SEEN_JEWELLERY:
          text =  "You have picked up a a piece of jewellery, either a ring "
                  "('<w>=<magenta>') or an amulet ('<w>\"<magenta>'). Type "
                  "<w>P<magenta> to put it on and <w>R<magenta> to remove it. You "
                  "can view it with <w>v<magenta> although often magic is "
                  "necessary to reveal its true nature.";
          break;
      case TUT_SEEN_MISC:
          text =  "This is a curious object indeed. You can play around with it "
                  "to find out what it does by <w>w<magenta>ielding and "
                  "<w>E<magenta>voking it.";
          break;
      case TUT_SEEN_STAFF:
          get_item_symbol(DNGN_ITEM_STAVE, &ch, &colour);
          text =  "You have picked up a magic staff or a rod, both of which are "
                  "represented by '<w>";
          text += ch;
          text += "<magenta>'. Both must be <w>i<magenta>elded to be of use. "
                  "Magicians use staves to increase their power in certain spell "
                  "schools. By contrast, a rod allows the casting of certain "
                  "spells even without magic knowledge simply by "
                  "<w>E<magenta>voking it. For the latter the power depends on "
                  "your Evocations skill.";
          break;
      case TUT_SEEN_STAIRS:
          if (you.num_turns < 1)
          	return;

          object = env.show[ex][ey];
          colour = env.show_col[ex][ey];
          get_item_symbol( object, &ch, &colour );

          text =  "The <w>";
          text += colour_to_tag(colour);
          text += ch;
          text += "<magenta> are some downstairs. You can enter the next (deeper) "
                  "level by following them down (<w>><magenta>). To get back to "
                  "this level again, press <w><<<magenta> while standing on the "
                  "upstairs.";

          break;
      case TUT_SEEN_TRAPS:
          text =  "Oops... you just triggered a trap. An unwary adventurer will "
                  "occasionally stumble into one of these nasty constructions "
                  "depicted by <w>^<magenta>. They can do physical damage (with "
                  "darts or needles, for example) or have other, more magical "
                  "effects, like teleportation.";
          break;
      case TUT_SEEN_ALTAR:
          object = env.show[ex][ey];
          colour = env.show_col[ex][ey];
          get_item_symbol( object, &ch, &colour );
          
          text =  "The ";
          text += colour_to_tag(colour);
          text += ch;
          text += "<magenta> is an altar. You can get information about it by pressing "
                  "<w>p<magenta> while standing on the square. Before taking up "
                  "the responding faith you'll be asked for confirmation.";
          break;
      case TUT_SEEN_SHOP:
          text =  "The <yellow>";
          text += get_screen_glyph(x,y);
          text += "<magenta> is a shop. You can enter it by typing <w><<<magenta>.";
          break;
      case TUT_SEEN_DOOR:
          if (you.num_turns < 1)
          	return;

          text =  "The <w>";
          text += get_screen_glyph(x,y);
          text += "<magenta> is a closed door. You can open it by walking into it. "
                  "Sometimes it is useful to close a door. Do so by pressing "
                  "<w>c<magenta>, followed by the direction.";
          break;
      case TUT_KILLED_MONSTER:
          text =  "Congratulations, your character just gained some experience by "
                  "killing this monster! Every action will use up some of it to "
                  "train certain skills. For example, fighting monsters ";
          if (Options.tutorial_type == TUT_BERSERK_CHAR)
              text += "in melee battle will raise your Axes and Fighting skills.";
          else if (Options.tutorial_type == TUT_RANGER_CHAR)
              text += "using bow and arrows will raise your Bows and Ranged Combat "
                      "skills.";
          else // if (Options.tutorial_type == TUT_MAGIC_CHAR)
              text += "with offensive magic will raise your Conjurations and "
                      "Spellcasting skills.";

          if (you.religion != GOD_NO_GOD)
          {
              text += "\nTo dedicate your kills to ";
              text += god_name(you.religion);
              text += " <w>p<magenta>ray before battle. Not all gods will be "
                      "pleased by doing this.";
          }
          break;
      case TUT_NEW_LEVEL:
          text =  "Well done! Reaching a new experience level is always a nice "
                  "event: you get more health and magic points, and occasionally "
                  "increases to your attributes (strength, dexterity, intelligence).";
          if (Options.tutorial_type == TUT_MAGIC_CHAR)
          {
              text += "\nAlso, new experience levels let you learn more spells "
                      "(the Spellcasting skill also does this). For now, "
                      "you should try to memorise the second spell of your "
                      "starting book with <w>Mcb<magenta>, which can then be "
                      "zapped with <w>Zb<magenta>.";
          }
          break;
      case TUT_SKILL_RAISE:
          text =  "One of your skills just got raised. Type <w>m<magenta> to take "
                  "a look at your skills screen.";
          break;
      case TUT_YOU_ENCHANTED:
          text =  "Enchantments of all types can befall you temporarily. "
                  "Brief descriptions of these appear at the lower end of the stats "
                  "area. Press <w>@<magenta> for more details. A list of all "
                  "possible enchantments is in the manual.";
          break;
      case TUT_YOU_SICK:
          learned_something_new(TUT_YOU_ENCHANTED);
          text =  "Corpses can be spoiled or inedible, making you sick. "
                  "Also, some monsters' flesh is less palatable than others'. "
                  "While sick, your hitpoints won't regenerate and sometimes "
                  "an attribute may decrease. It wears off with time (wait with "
                  "<w>5<magenta>) or you can quaff a potion of healing.";
          break;
      case TUT_YOU_POISON:
          learned_something_new(TUT_YOU_ENCHANTED);
          text =  "Poison will slowly reduce your hp. It wears off with time "
                  "(wait with <w>5<magenta>) or you could quaff a potion of "
                  "healing.";
          break;
      case TUT_YOU_CURSED:
          text =  "Curses are comparatively harmless, but they do mean that you "
                  "cannot remove cursed equipment and will have to suffer the "
                  "(possibly) bad effects until you find and read a scroll of "
                  "remove curse. Weapons can also be uncursed using enchanting "
                  "scrolls.";
          break;
      case TUT_YOU_HUNGRY:
          text =  "There are two ways to overcome hunger: food you started "
                  "with or found, and selfmade chunks from corpses. To get the "
                  "latter, all you need to do is <w>D<magenta> a corpse with a "
                  "sharp implement. Your starting weapon will do nicely. "
                  "Try to dine on chunks in order to save permanent food.";
          break;
      case TUT_YOU_STARVING:
          text =  "You are now suffering from terrible hunger. You'll need to "
                  "<w>e<magenta>at something quickly, or you'll die. The safest "
                  "way to deal with this is to simply eat something from your "
                  "inventory rather than wait for a monster to leave a corpse.";
          break;
      case TUT_MULTI_PICKUP:
          text =  "There's a more comfortable way to pick up several items at the "
                  "same time. If you press <w>,<magenta> or <w>g<magenta> twice "
                  "you can choose items from a menu. This takes less keystrokes "
                  "but has no influence on the number of turns needed. Multi-pickup "
                  "will be interrupted by monsters or other dangerous events.";
          break;
      case TUT_HEAVY_LOAD:
          if (you.burden_state != BS_UNENCUMBERED)
              text =  "It is not usually a good idea to run around encumbered; it "
                      "slows you down and increases your hunger.";
          else
              text =  "Sadly, your inventory is limited to 52 items, and it appears "
                      "your knapsack is full.";

          text += " However, this is easy enough to rectify: simply <w>d<magenta>rop "
                  "some of the stuff you don't need or that's too heavy to lug "
                  "around permanently.";
          break;
      case TUT_ROTTEN_FOOD:
          text =  "One or more of the chunks or corpses you carry has started to "
                  "rot. Few races can digest these safely, so you might just as "
                  "well <w>d<magenta>rop them now.";
          if (you.religion == GOD_TROG || you.religion == GOD_MAKHLEB ||
              you.religion == GOD_OKAWARU)
          {
              text += "\nIf it is a rotting corpse you carry now might be a good "
                      "time to <w>d<magenta>rop and <w>D<magenta>issect it during "
                      "prayer (<w>p<magenta>) as an offer to ";
              text += god_name(you.religion);
              text += ".";
          }
          break;
      case TUT_MAKE_CHUNKS:
          text =  "How lucky! That monster left a corpse which you can now "
                  "<w>D<magenta>issect. One or more chunks will appear that you can "
                  "then <w>e<magenta>at. Beware that some chunks may be, "
                  "sometimes or always, hazardous. Only experience can help "
                  "you here.";

          if (you.duration[DUR_PRAYER] &&
                (you.religion == GOD_OKAWARU || you.religion == GOD_MAKHLEB ||
                 you.religion == GOD_TROG || you.religion == GOD_ELYVILON))
          {
              text += "\nNote that dissection under prayer offers the corpse to ";
              text += god_name(you.religion);
              text += " - check your god's attitude about this with <w>^<magenta>.";
          }
          break;
      case TUT_SHIFT_RUN:
          text =  "Walking around takes less keystrokes if you press "
                  "<w>Shift-direction<magenta> or <w>/ direction<magenta>. "
                  "That will let you run until a monster comes into sight or "
                  "your character sees something interesting.";
          break;
      case TUT_MAP_VIEW:
          text =  "As you explore a level, orientation can become difficult. "
                  "Press <w>X<magenta> to bring up the level map. Typing "
                  "<w>?<magenta> shows the list of level map commands. "
                  "Most importantly, moving the cursor to a spot and pressing "
                  "<w>.<magenta> or <w>Enter<magenta> lets your character move "
                  "there on its own.";
          break;
      case TUT_DONE_EXPLORE:
          text =  "You have explored the dungeon on this level. The downstairs look "
                  "like '<w>><magenta>'. Proceed there and press <w>><magenta> to "
                  "go down. ";

          if (Options.tutorial_events[TUT_SEEN_STAIRS])
          {
              text += "In rare cases, you may have found no downstairs at all. "
                      "Try searching for secret doors in suspicious looking spots; "
                      "use <w>s<magenta>, <w>.<magenta> or <w>5<magenta> to do so.";
          }
          else
          {
              text += "Each level of Crawl has three white up and three white down "
                      "stairs. Unexplored parts can often be accessed via another "
                      "level or through secret doors. To find the latter, search "
                      "the adjacent squares of walls for one turn with <w>s<magenta> "
                      "or <w>.<magenta>, or for 100 turns with <w>5<magenta> or "
                      "<w>Shift-numpad 5.";
          }
          break;
      case TUT_NEED_HEALING:
          text =  "If you're low on hitpoints or magic and there's no urgent need "
                  "to move, you can rest for a bit. Press <w>5<magenta> or "
                  "<w>shift-numpad-5<magenta> to do so.";
          break;
      case TUT_NEED_POISON_HEALING:
          text =  "Your poisoning could easily kill you, so now would be a good "
                  "time to <w>q<magenta>uaff a potion of heal wounds or, better "
                  "yet, a potion of healing. If you have seen neither of these so "
                  "far, try unknown ones in your inventory. Good luck!";
          break;
      case TUT_POSTBERSERK:
          text =  "Berserking is extremely exhausting! It burns a lot of nutrition, "
                  "and afterwards you are slowed down and occasionally even pass "
                  "out.";
          break;
      case TUT_RUN_AWAY:
          text =  "Whenever you've got only a few hitpoints left and you're in "
                  "danger of dying, check your options carefully. Often, retreat or "
                  "use of some item might be a viable alternative to fighting on.";
          if (you.species == SP_CENTAUR)
              text += " As a four-legged centaur you are particularly quick - "
                      "running is an option! ";
          if (Options.tutorial_type == TUT_BERSERK_CHAR && !you.berserker)
          {
              text += "\nAlso, with ";
              text += god_name(you.religion);
              text += "'s support you can use your Berserk ability (<w>a<magenta>) "
                      "to temporarily gain more hitpoints and greater strength. ";
          }
          break;
      case TUT_YOU_MUTATED:
          text =  "Mutations can be obtained from several sources, among them "
                  "potions, spell miscasts, and overuse of strong enchantments "
                  "like Invisibility. The only reliable way to get rid of mutations "
                  "is with potions of cure mutation. There are about as many "
                  "harmful as beneficial mutations, and most of them have three "
                  "levels. Check your mutations with <w>A<magenta>.";
          break;
      case TUT_NEW_ABILITY:
          text =  "You just gained a new ability. Press <w>a<magenta> to take a "
                  "look at your abilities or to use one of them.";
          break;
      case TUT_WIELD_WEAPON:
          text =  "You might want to <w>w<magenta>ield a more suitable implement "
                  "when attacking monsters.";
          if (Options.tutorial_type == TUT_RANGER_CHAR
                  && you.inv[ you.equip[EQ_WEAPON] ].sub_type == WPN_BOW)
          {
              text += "You can easily switch between weapons in slots a and "
                      "b by pressing <w>'<magenta>.";
          }
          break;
      case TUT_SEEN_MONSTER:
      case TUT_SEEN_FIRST_OBJECT:
          break;
    case TUT_EVENTS_NUM:
        text += "You've found something new (but I don't know what)!";
    }

    if ( !text.empty() )
       print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);

    Options.tut_just_triggered = true;
    Options.tutorial_events[seen_what] = 0;
    Options.tutorial_left--;
}
