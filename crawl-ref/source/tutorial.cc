/*
 *  Created for Crawl Reference by JPEG on $Date: 2007-01-11$
 */

#include "tutorial.h"
#include <cstring>
#include <sstream>

#include "cio.h"
#include "command.h"
#include "files.h"
#include "food.h"
#include "format.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "monstuff.h"
#include "newgame.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"

static species_type get_tutorial_species(unsigned int type);
static job_type get_tutorial_job(unsigned int type); 


//#define TUTORIAL_DEBUG
#define TUTORIAL_VERSION 111

static int get_tutorial_cols()
{
    int ncols = get_number_of_cols();
    return (ncols > 80? 80 : ncols);
}

void save_tutorial( FILE* fp )
{
    writeLong( fp, TUTORIAL_VERSION);
    writeShort( fp, Options.tutorial_type);
    for ( long i = 0; i < TUT_EVENTS_NUM; ++i )
        writeShort( fp, Options.tutorial_events[i] );
}

void load_tutorial( FILE* fp )
{
    Options.tutorial_left = 0;

    int version = readLong(fp);
    if (version != TUTORIAL_VERSION)
        return; 
        
    Options.tutorial_type = readShort(fp);
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
    clrscr();

    gotoxy(1,1);
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

    while (1)
    {
        char keyn = c_getch();

        if (keyn == '*')
            keyn = 'a' + random2(TUT_TYPES_NUM);
        
        // choose character for tutorial game and set starting values
        if (keyn >= 'a' && keyn <= 'a' + TUT_TYPES_NUM - 1)
        {
            Options.tutorial_type = keyn - 'a';
            you.species = get_tutorial_species(Options.tutorial_type);
            you.char_class = get_tutorial_job(Options.tutorial_type);
            
            // activate all triggers
            Options.tutorial_events.init(true);
            Options.tutorial_left = TUT_EVENTS_NUM;
            
            // store whether explore, stash search or travelling was used
            Options.tut_explored = true;
            Options.tut_stashes = true;
            Options.tut_travel = true;
            
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
            // set_startup_options(); 
            you.species = SP_UNKNOWN;
            you.char_class = JOB_UNKNOWN;
            Options.race = 0;
            Options.cls = 0;
        }

        switch (keyn)
        {
        case CK_BKSP:
            choose_race();
            return false;
        case ' ':
            choose_class();
            return false;
        case 'X':
            cprintf(EOL "Goodbye!");
            end(0);
            return false;       // as if
        }
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

    cprintf("%c - %s %s %s" EOL,
            letter, species_name(get_tutorial_species(type), 1).c_str(),
            get_class_name(get_tutorial_job(type)), desc);
}

static species_type get_tutorial_species(unsigned int type)
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
static job_type get_tutorial_job(unsigned int type)
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
    std::ostringstream istr;
    
    istr << "<white>Welcome to Dungeon Crawl!</white>" EOL EOL
         << "Your object is to lead a "
         << species_name(get_tutorial_species(Options.tutorial_type), 1)
         << " " << get_class_name(get_tutorial_job(Options.tutorial_type))
         <<
        " safely through the depths of the dungeon, retrieving the "
        "fabled orb of Zot and returning it to the surface. "
        " In the beginning, however, let discovery be your "
        "main goal. Try to delve as deeply as possible but beware; "
        "death lurks around every corner." EOL EOL
        "For the moment, just remember the following keys "
        "and their functions:" EOL
        "  <white>?</white> - shows the items and the commands" EOL
        "  <white>S</white> - saves the game, to be resumed later "
        "(but note that death is permanent)" EOL
        "  <white>x</white> - examines something in your vicinity" EOL EOL
        "This tutorial will help you play Crawl without reading any "
        "documentation. If you feel intrigued, there is more information "
        "available in these files (all of which can also be read in-game):"
        EOL
        "  <lightblue>readme.txt</lightblue>         - "
        "A very short guide to Crawl." EOL        
        "  <lightblue>manual.txt</lightblue>         - "
        "This contains all details on races, magic, skills, etc." EOL
        "  <lightblue>crawl_options.txt</lightblue>  - "
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
      case TUT_SEEN_ESCAPE_HATCH:
          return "seen first escape hatch";
      case TUT_SEEN_TRAP:
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
      case TUT_POSTBERSERK:
          return "learned about Berserk aftereffects";
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

    result = "<magenta>"
        "What you see here is the typical Crawl screen. The upper left map "
        "shows your hero as the <white>@<magenta> in the center. The parts "
        "of the map you remember but cannot currently see will be greyed "
        "out."
        "</magenta>" EOL;        
    result += " --more--                               Press <white>Escape</white> to skip the basics";

    linebreak_string2(result,get_tutorial_cols());
    return formatted_string::parse_block(result, false);
}

static formatted_string tutorial_stats_intro()
{
    std::ostringstream istr;

    // Note: must fill up everything to override the map
    istr << "<magenta>"
        "To the right, important properties of \n"
        "the character are displayed. The most \n"
        "basic one is your health, measured as \n"
        "<white>HP: " << you.hp << "/" << you.hp_max << "<magenta>. ";

    if (Options.tutorial_type==TUT_MAGIC_CHAR)
        istr << "  ";

    istr <<
        "These are your current out \n"
        "of maximum health points. When health \n"
        "drops to zero, you die.               \n"
        "<white>Magic: "
         << you.magic_points << "/" << you.max_magic_points <<
        "<magenta> is your energy for casting \n"
        "spells, although more mundane actions \n"
        "often draw from Magic, too.           \n"
        "Further down, <white>Str<magenta>ength, <white>Dex<magenta>terity and \n"
        "<white>Int<magenta>elligence are shown and provide an \n"
        "all-around account of the character's \n"
        "attributes.                           \n"
        "<lightgrey>"
        "                                      \n"
        " --more--                               Press <white>Escape</white> to skip the basics\n"
        "                                      \n"
        "                                      \n";
    return formatted_string::parse_block(istr.str(), false);
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
    std::string text =
        "To move your character, use the numpad; try Numlock both on and off. "
        "If your system has no number pad, or if you are familiar with the vi "
        "keys, movement is also possible with <w>hjklyubn<magenta>. A basic "
        "command list can be found under <w>?<magenta>, and the most "
        "important commands will be explained to you as it becomes necessary.";
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
         "playing experience your characters will last longer.", MSGCH_TUTORIAL);
    mpr( "Perhaps the following advice can improve your playing style:", MSGCH_TUTORIAL);
    more();

    if (Options.tutorial_type == TUT_MAGIC_CHAR 
        && Options.tut_spell_counter < Options.tut_melee_counter )
    {
        text = "As a Conjurer your main weapon should be offensive magic. Cast "
               "spells more often! Remember to rest when your Magic is low.";
    }
    else if (Options.tutorial_type == TUT_BERSERK_CHAR && you.religion == GOD_TROG
             && Options.tut_berserk_counter <= 3 )
    {
        text =  "Don't forget to go berserk when fighting particularly "
                "difficult foes. It is risky, but makes you faster "
                "and beefier.";
    }
    else if (Options.tutorial_type == TUT_RANGER_CHAR 
             && 2*Options.tut_throw_counter < Options.tut_melee_counter )
    {
        text = "Your bow and arrows are extremely powerful against "
               "distant monsters. Be sure to collect all arrows lying "
               "around in the dungeon."; 
    }
    else
    {
      int hint = random2(6);
      // If a character has been unusually busy with projectiles and spells
      // give some other hint rather than the first one.
      if (Options.tut_throw_counter + Options.tut_spell_counter
              >= Options.tut_melee_counter && hint == 0)
      {
         hint = random2(5)+1;
      }

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

    Options.tutorial_events.init(false);
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

    Options.tutorial_events.init(false);
}

// occasionally remind religious characters of sacrifices
void tutorial_dissection_reminder()
{
    if (Options.tut_just_triggered || !Options.tutorial_left)
        return;

    // when hungry, give appropriate message or at least don't suggest sacrifice
    if (you.hunger_state < HS_SATIATED)
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
        std::string text;
        text += "If you don't need to eat it, consider <w>D<magenta>issecting "
                "this corpse under <w>p<magenta>rayer as a sacrifice to ";
        text += god_name(you.religion);
        text += ". Whenever you <w>v<magenta>iew a corpse while in tutorial mode "
                "you can reread this information.";

        print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
        Options.tut_just_triggered = true;
    }
}

// occasionally remind injured characters of resting
void tutorial_healing_reminder()
{
    if (!Options.tutorial_left)
        return;
        
    if (you.duration[DUR_POISONING] && 2*you.hp < you.hp_max)
    {
        if (Options.tutorial_events[TUT_NEED_POISON_HEALING])
            learned_something_new(TUT_NEED_POISON_HEALING);
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

            std::string text;
            text =  "Remember to rest between fights and to enter unexplored "
                    "terrain with full hitpoints and magic. For resting, "
                    "press <w>5<magenta> or <w>Shift-numpad 5<magenta>.";
                    
            if (you.religion == GOD_TROG && !you.duration[DUR_BERSERKER]
                && !you.duration[DUR_EXHAUSTED] && you.hunger_state >= HS_SATIATED)
            {
              text += "\nAlso, berserking might help you not to lose so many "
                      "hitpoints in the first place. To use your abilities type "
                      "<w>a<magenta>.";
            }
            print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
            Options.tut_just_triggered = 1;
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

void tutorial_first_monster(const monsters &mon)
{
    if (!Options.tutorial_events[TUT_SEEN_MONSTER])
        return;

    // crude hack:
    // if the first monster is sleeping wake it
    // (highlighting is an unnecessary complication)
    if (get_mons_colour(&mon) != (&mon)->colour)
    {
        noisy(1, mon.x, mon.y);
        viewwindow(true, false);
    }
    
    unsigned ch;
    unsigned short col;
    get_mons_glyph(&mon, &ch, &col);

    std::string text = "<magenta>That ";
    text += colour_to_tag(col);
    text += ch;
    text += "<magenta> is a monster, usually depicted by a letter. Some typical "
            "early monsters look like <brown>r<magenta>, <w>g<magenta>, "
            "<lightgray>b<magenta> or <brown>K<magenta>. You can gain information "
            "about it by pressing <w>x<magenta> and moving the cursor on the "
            "monster."
            "\nTo attack this monster with your wielded weapon, just move into "
            "it.";
            
    print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);

    if (Options.tutorial_type == TUT_RANGER_CHAR)
    {
        text =  "However, as a hunter you will want to deal with it using your "
                "bow. If you have a look at your bow with <w>v<magenta>, you'll "
                "find an explanation of how to do this. First <w>w<magenta>ield "
                "it, then follow the instructions.";
        print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
    }
    else if (Options.tutorial_type == TUT_MAGIC_CHAR)
    {
        text =  "However, as a conjurer you will want to deal with it using magic. "
                "If you have a look at your spellbook with <w>v<magenta>, you'll "
                "find an explanation of how to do this.";
        print_formatted_paragraph(text, get_tutorial_cols(), MSGCH_TUTORIAL);
    }

    Options.tutorial_events[TUT_SEEN_MONSTER] = 0;
    Options.tutorial_left--;
    Options.tut_just_triggered = 1;
}

void tutorial_first_item(const item_def &item)
{
    if (!Options.tutorial_events[TUT_SEEN_FIRST_OBJECT]
        || Options.tut_just_triggered)
    {
        return;
    }

    unsigned ch;
    unsigned short col;
    get_item_glyph(&item, &ch, &col);

    if (ch == ' ') // happens if monster standing on dropped corpse or item
       return;

    std::string text = "<magenta>That ";
    text += colour_to_tag(col);
    text += ch;
    text += "<magenta> is an item. If you move there and press <w>g<magenta> or "
            "<w>,<magenta> you will pick it up. Generally, items are shown by "
            "non-letter symbols like <w>%?!\"=()[<magenta>. Once it is in your "
            "inventory, you can drop it again with <w>d<magenta>. Several types "
            "of objects will usually be picked up automatically."
            "\nAny time you <w>v<magenta>iew an item, you can read about its "
            "properties and description.";
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

    std::ostringstream text;
    unsigned ch;
    unsigned short colour;
    const int ex = x - you.x_pos + 9;
    const int ey = y - you.y_pos + 9;
    int object;

    switch(seen_what)
    {
      case TUT_SEEN_POTION:          
          text << "You have picked up your first potion ('<w>!<magenta>'). "
                  "Use <w>q<magenta> to drink (quaff) it.";
          break;
          
      case TUT_SEEN_SCROLL:
          text << "You have picked up your first scroll ('<w>?<magenta>'). "
                  "Type <w>r<magenta> to read it.";
          break;
          
      case TUT_SEEN_WAND:
          text << "You have picked up your first wand ('<w>/<magenta>'). "
                  "Type <w>z<magenta> to zap it.";
          break;
          
      case TUT_SEEN_SPBOOK:
          get_item_symbol(DNGN_ITEM_BOOK, &ch, &colour);
          text << "You have picked up a book ('<w>"
               << static_cast<char>(ch)
               << "'<magenta>) that you can read by typing <w>r<magenta>. "
                  "If it's a spellbook you'll then be able to memorise "
                  "spells via <w>M<magenta> and cast a memorised spell with "
                  "<w>Z<magenta>.";
          
          if (!you.skills[SK_SPELLCASTING])
          {
              text << "\nHowever, first you will have to get accustomed to "
                      "spellcasting by reading lots of scrolls.";
          }
          if (you.religion == GOD_TROG)
          {
               more();
               text << "\n\nAs a worshipper of "
                    << god_name(GOD_TROG)
                    << ", though, you might instead wish to burn those tomes of "
                       "hated magic by using the corresponding "
                       "<w>a<magenta>bility.";
          }
          text << "\nDuring the tutorial you can reread this information at "
                  "any time by <w>v<magenta>iewing the item in question.";
          break;
          
      case TUT_SEEN_WEAPON:
          text << "This is the first weapon ('<w>(<magenta>') you've picked up. "
                  "Use <w>w<magenta> to wield it, but be aware that this weapon "
                  "might train a different skill from your current one. You can "
                  "view the weapon's properties with <w>v<magenta>.";

          if (Options.tutorial_type == TUT_BERSERK_CHAR)
          {
              text << "\nAs you're already trained in Axes you should stick "
                      "with these. Checking other axes can be worthwhile.";
          }
          break;
          
      case TUT_SEEN_MISSILES:
          text << "This is the first stack of missiles ('<w>)<magenta>') you've "
                  "picked up. Darts can be thrown by hand, but other missile types "
                  "like arrows and needles require a launcher and training in "
                  "using it to be really effective. <w>v<magenta> gives more "
                  "information about both missiles and launcher.";

          if (Options.tutorial_type == TUT_RANGER_CHAR)
          {
              text << "\nAs you're already trained in Bows you should stick with "
                      "arrows and collect more of them in the dungeon.";
          }
          else if (Options.tutorial_type == TUT_MAGIC_CHAR)
          {
              text << "\nHowever, as a spellslinger you don't really need another "
                      "type of ranged attack, unless there's another effect in "
                      "addition to damage.";
          }
          else
          {
              text << "\nFor now you might be best off with sticking to darts or "
                      "stones for ranged attacks.";
          }
          break;
          
      case TUT_SEEN_ARMOUR:
          text << "This is the first piece of armour ('<w>[<magenta>') you've "
                  "picked up. Use <w>W<magenta> to wear it and <w>T<magenta> to "
                  "take it off again. You can view its properties with "
                  "<w>v<magenta>.";

          if (you.species == SP_CENTAUR || you.species == SP_MINOTAUR)
          {
              text << "\nNote that as a " << species_name(you.species, 1)
                   << " you will be unable to wear "
                   << (you.species == SP_CENTAUR ? "boots" : "helmets");
          }
          break;
          
      case TUT_SEEN_RANDART:
          text << "Weapons and armour that have unusual descriptions like this "
                  "are much more likely to be of higher enchantment or have "
                  "special properties, good or bad. The rarer the description, "
                  "the greater the potential value of an item.";
          break;
          
      case TUT_SEEN_FOOD:
          text << "You have picked up some food ('<w>%<magenta>'). You can eat it "
                  "by typing <w>e<magenta>.";
          break;
          
      case TUT_SEEN_CARRION:
          text << "You have picked up a corpse ('<w>%<magenta>'). When a corpse "
                  "is lying on the ground, you can <w>D<magenta>issect with a "
                  "sharp implement. Once hungry you can <w>e<magenta>at the "
                  "resulting chunks (though they may not be healthy).";
                  
          if (god_likes_butchery(you.religion))
          {
              text << " During prayer you can offer corpses to "
                   << god_name(you.religion)
                   << " by dissecting them, as well. Note that the gods will not "
                   << "accept rotting flesh.";
          }
          text << "\nDuring the tutorial you can reread this information at "
                  "any time by <w>v<magenta>iewing the item in question.";
          break;
          
      case TUT_SEEN_JEWELLERY:
          text << "You have picked up a a piece of jewellery, either a ring "
                  "('<w>=<magenta>') or an amulet ('<w>\"<magenta>'). Type "
                  "<w>P<magenta> to put it on and <w>R<magenta> to remove it. You "
                  "can view it with <w>v<magenta> although often magic is "
                  "necessary to reveal its true nature.";
          break;
          
      case TUT_SEEN_MISC:
          text << "This is a curious object indeed. You can play around with it "
                  "to find out what it does by <w>w<magenta>ielding and "
                  "<w>E<magenta>voking it.";
          break;
          
      case TUT_SEEN_STAFF:
          get_item_symbol(DNGN_ITEM_STAVE, &ch, &colour);
          text << "You have picked up a magic staff or a rod, both of which are "
                  "represented by '<w>"
               << static_cast<char>(ch)
               << "<magenta>'. Both must be <w>w<magenta>ielded to be of use. "
                  "Magicians use staves to increase their power in certain spell "
                  "schools. By contrast, a rod allows the casting of certain "
                  "spells even without magic knowledge simply by "
                  "<w>E<magenta>voking it. For the latter the power depends on "
                  "your Evocations skill.";
          text << "\nDuring the tutorial you can reread this information at "
                  "any time by <w>v<magenta>iewing the item in question.";
          break;
          
      case TUT_SEEN_STAIRS:
          if (you.num_turns < 1)
              return;

          object = env.show[ex][ey];
          colour = env.show_col[ex][ey];
          get_item_symbol( object, &ch, &colour );

          text << "The " << colour_to_tag(colour) << static_cast<char>(ch)
               << "<magenta> are some downstairs. You can enter the next (deeper) "
                  "level by following them down (<w>><magenta>). To get back to "
                  "this level again, press <w><<<magenta> while standing on the "
                  "upstairs.";

          break;
          
      case TUT_SEEN_ESCAPE_HATCH:
          if (you.num_turns < 1)
              return;

          object = env.show[ex][ey];
          colour = env.show_col[ex][ey];
          get_item_symbol( object, &ch, &colour );
          
          text << "These " << colour_to_tag(colour) << static_cast<char>(ch);
          if (ch == '<')
               text << "<";
          text << "<magenta> are some kind of escape hatch. You can use them to "
                  "quickly leave a level with <w><<<magenta> and <w>><magenta>, "
                  "respectively, but will usually be unable to return right away.";
          break;
          
      case TUT_SEEN_TRAP:
          object = env.show[ex][ey];
          colour = env.show_col[ex][ey];
          get_item_symbol( object, &ch, &colour );
          if (ch == ' ' || colour == BLACK)
              colour = LIGHTCYAN;

          text << "Oops... you just triggered a trap. An unwary adventurer will "
                  "occasionally stumble into one of these nasty constructions "
                  "depicted by " << colour_to_tag(colour) << "^<magenta>. They "
                  "can do physical damage (with darts or needles, for example) "
                  "or have other, more magical effects, like teleportation.";
          break;
          
      case TUT_SEEN_ALTAR:
          object = env.show[ex][ey];
          colour = env.show_col[ex][ey];
          get_item_symbol( object, &ch, &colour );
          
          text << "The " << colour_to_tag(colour) << static_cast<char>(ch)
               << "<magenta> is an altar. You can get information about it by pressing "
                  "<w>p<magenta> while standing on the square. Before taking up "
                  "the responding faith you'll be asked for confirmation.";
          break;
          
      case TUT_SEEN_SHOP:
          text << "The <yellow>" << stringize_glyph(get_screen_glyph(x,y))
               << "<magenta> is a shop. You can enter it by typing "
                  "<w><<<magenta>.";
          break;
          
      case TUT_SEEN_DOOR:
          if (you.num_turns < 1)
              return;

          text << "The <w>" << stringize_glyph(get_screen_glyph(x,y))
               << "<magenta> is a closed door. You can open it by walking into it. "
                  "Sometimes it is useful to close a door. Do so by pressing "
                  "<w>c<magenta>, followed by the direction.";
          break;
          
      case TUT_KILLED_MONSTER:
          text << "Congratulations, your character just gained some experience by "
                  "killing this monster! Every action will use up some of it to "
                  "train certain skills. For example, fighting monsters ";
                  
          if (Options.tutorial_type == TUT_BERSERK_CHAR)
              text << "in melee battle will raise your Axes and Fighting skills.";
          else if (Options.tutorial_type == TUT_RANGER_CHAR)
              text << "using bow and arrows will raise your Bows skill.";
          else // if (Options.tutorial_type == TUT_MAGIC_CHAR)
              text << "with offensive magic will raise your Conjurations and "
                      "Spellcasting skills.";

          if (you.religion == GOD_TROG)
              text << " Also, kills of living creatures are automatically "
                      "dedicated to "
                   << god_name(you.religion)
                   << ".";
          break;
          
      case TUT_NEW_LEVEL:
          text << "Well done! Reaching a new experience level is always a nice "
                  "event: you get more health and magic points, and occasionally "
                  "increases to your attributes (strength, dexterity, intelligence).";
                  
          if (Options.tutorial_type == TUT_MAGIC_CHAR)
          {
              text << "\nAlso, new experience levels let you learn more spells "
                      "(the Spellcasting skill also does this). For now, "
                      "you should try to memorise the second spell of your "
                      "starting book with <w>Mcb<magenta>, which can then be "
                      "zapped with <w>Zb<magenta>.";
          }
          break;
          
      case TUT_SKILL_RAISE:
          text << "One of your skills just got raised. To view or manage your "
                  "skill set, type <w>m<magenta>.";
          break;
          
      case TUT_YOU_ENCHANTED:
          text << "Enchantments of all types can befall you temporarily. "
                  "Brief descriptions of these appear at the lower end of the stats "
                  "area. Press <w>@<magenta> for more details. A list of all "
                  "possible enchantments is in the manual.";
          break;
          
      case TUT_YOU_SICK:
          learned_something_new(TUT_YOU_ENCHANTED);
          text << "Corpses can be spoiled or inedible, making you sick. "
                  "Also, some monsters' flesh is less palatable than others'. "
                  "While sick, your hitpoints won't regenerate and sometimes "
                  "an attribute may decrease. It wears off with time (wait with "
                  "<w>5<magenta>) or you can quaff a potion of healing. "
                  "Also you can always press <w>@<magenta> to see your current "
                  "status. ";
          break;
          
      case TUT_YOU_POISON:
          learned_something_new(TUT_YOU_ENCHANTED);
          text << "Poison will slowly reduce your hp. It wears off with time "
                  "(wait with <w>5<magenta>) or you could quaff a potion of "
                  "healing. Also you can always press <w>@<magenta> to see your "
                  "current status. ";
          break;
          
      case TUT_YOU_CURSED:
          text << "Curses are comparatively harmless, but they do mean that you "
                  "cannot remove cursed equipment and will have to suffer the "
                  "(possibly) bad effects until you find and read a scroll of "
                  "remove curse. Weapons can also be uncursed using enchanting "
                  "scrolls.";
          break;
          
      case TUT_YOU_HUNGRY:
          text << "There are two ways to overcome hunger: food you started "
                  "with or found, and selfmade chunks from corpses. To get the "
                  "latter, all you need to do is <w>D<magenta>issect a corpse "
                  "with a sharp implement. Your starting weapon will do nicely. "
                  "Try to dine on chunks in order to save permanent food.";
          break;
          
      case TUT_YOU_STARVING:
          text << "You are now suffering from terrible hunger. You'll need to "
                  "<w>e<magenta>at something quickly, or you'll die. The safest "
                  "way to deal with this is to simply eat something from your "
                  "inventory rather than wait for a monster to leave a corpse.";
          break;
          
      case TUT_MULTI_PICKUP:
          text << "There's a more comfortable way to pick up several items at the "
                  "same time. If you press <w>,<magenta> or <w>g<magenta> twice "
                  "you can choose items from a menu. This takes less keystrokes "
                  "but has no influence on the number of turns needed. Multi-pickup "
                  "will be interrupted by monsters or other dangerous events.";
          break;
          
      case TUT_HEAVY_LOAD:
          if (you.burden_state != BS_UNENCUMBERED)
              text << "It is not usually a good idea to run around encumbered; it "
                      "slows you down and increases your hunger.";
          else
              text << "Sadly, your inventory is limited to 52 items, and it appears "
                      "your knapsack is full.";

          text << " However, this is easy enough to rectify: simply <w>d<magenta>rop "
                  "some of the stuff you don't need or that's too heavy to lug "
                  "around permanently.";
          break;
          
      case TUT_ROTTEN_FOOD:
          text << "One or more of the chunks or corpses you carry has started to "
                  "rot. Few races can digest these safely, so you might just as "
                  "well <w>d<magenta>rop them now.";
          break;
          
      case TUT_MAKE_CHUNKS:
          text << "How lucky! That monster left a corpse which you can now "
                  "<w>D<magenta>issect. One or more chunks will appear that you "
                  "can then <w>e<magenta>at. Beware that some chunks may be, "
                  "sometimes or always, hazardous. Only experience can help "
                  "you here.";
                  
          if (you.duration[DUR_PRAYER]
              && (god_likes_butchery(you.religion)
                  || god_hates_butchery(you.religion)))
          {
              text << "\nRemember, though, to wait until your prayer is over, or "
                      "the corpse will instead be sacrificed to "
                   << god_name(you.religion)
                   << ".";
          }
          break;

      case TUT_OFFER_CORPSE:
          if (!god_likes_butchery(you.religion) || you.hunger_state <= HS_HUNGRY)
              return;

          text << "Hey, that monster left a corpse! If you don't need it for "
                  "food or other purposes, you can sacrifice it to "
               << god_name(you.religion)
               << " by <w>D<magenta>issecting it while <w>p<magenta>raying.";
          break;
          
      case TUT_SHIFT_RUN:
          text << "Walking around takes less keystrokes if you press "
                  "<w>Shift-direction<magenta> or <w>/ <w>direction<magenta>. "
                  "That will let you run until a monster comes into sight or "
                  "your character sees something interesting.";
          break;
          
      case TUT_MAP_VIEW:
          text << "As you explore a level, orientation can become difficult. "
                  "Press <w>X<magenta> to bring up the level map. Typing "
                  "<w>?<magenta> shows the list of level map commands. "
                  "Most importantly, moving the cursor to a spot and pressing "
                  "<w>.<magenta> or <w>Enter<magenta> lets your character move "
                  "there on its own.";
          break;
          
      case TUT_DONE_EXPLORE:
          text << "You have explored the dungeon on this level. The downstairs look "
                  "like '<w>><magenta>'. Proceed there and press <w>><magenta> to "
                  "go down. ";

          if (Options.tutorial_events[TUT_SEEN_STAIRS])
          {
              text << "In rare cases, you may have found no downstairs at all. "
                      "Try searching for secret doors in suspicious looking spots; "
                      "use <w>s<magenta>, <w>.<magenta> or <w>5<magenta> to do so.";
          }
          else
          {
              text << "Each level of Crawl has three white up and three white down "
                      "stairs. Unexplored parts can often be accessed via another "
                      "level or through secret doors. To find the latter, search "
                      "the adjacent squares of walls for one turn with <w>s<magenta> "
                      "or <w>.<magenta>, or for 100 turns with <w>5<magenta> or "
                      "<w>Shift-numpad 5.";
          }
          break;
          
      case TUT_NEED_HEALING:
          text << "If you're low on hitpoints or magic and there's no urgent need "
                  "to move, you can rest for a bit. Press <w>5<magenta> or "
                  "<w>shift-numpad-5<magenta> to do so.";
          break;
          
      case TUT_NEED_POISON_HEALING:
          text << "Your poisoning could easily kill you, so now would be a good "
                  "time to <w>q<magenta>uaff a potion of heal wounds or, better "
                  "yet, a potion of healing. If you have seen neither of these so "
                  "far, try unknown ones in your inventory. Good luck!";
          break;
          
      case TUT_POSTBERSERK:
          text << "Berserking is extremely exhausting! It burns a lot of nutrition, "
                  "and afterwards you are slowed down and occasionally even pass "
                  "out. Press <w>@<magenta> to see your current status.";
          break;
          
      case TUT_RUN_AWAY:
          text << "Whenever you've got only a few hitpoints left and you're in "
                  "danger of dying, check your options carefully. Often, retreat or "
                  "use of some item might be a viable alternative to fighting on.";
                  
          if (you.species == SP_CENTAUR)
              text << " As a four-legged centaur you are particularly quick - "
                      "running is an option!";
                      
          if (Options.tutorial_type == TUT_BERSERK_CHAR && you.religion == GOD_TROG
              && !you.duration[DUR_BERSERKER] && !you.duration[DUR_EXHAUSTED]
              && you.hunger_state >= HS_SATIATED)
          {
              text << "\nAlso, with "
                   << god_name(you.religion)
                   << "'s support you can use your Berserk ability (<w>a<magenta>) "
                      "to temporarily gain more hitpoints and greater strength. ";
          }
          break;
          
      case TUT_RETREAT_CASTER:
          text << "Without magical power you're unable to cast spells. While "
                  "melee is a possibility, that's not where your strengths lie, "
                  "so retreat (if possible) might be the better option.";
          break;
          
      case TUT_YOU_MUTATED:
          text << "Mutations can be obtained from several sources, among them "
                  "potions, spell miscasts, and overuse of strong enchantments "
                  "like Invisibility. The only reliable way to get rid of mutations "
                  "is with potions of cure mutation. There are about as many "
                  "harmful as beneficial mutations, and most of them have three "
                  "levels. Check your mutations with <w>A<magenta>.";
          break;
          
      case TUT_NEW_ABILITY:
          text <<  "You just gained a new ability. Press <w>a<magenta> to take a "
                   "look at your abilities or to use one of them.";
          break;
          
      case TUT_WIELD_WEAPON:
          if (Options.tutorial_type == TUT_RANGER_CHAR
                  && you.inv[ you.equip[EQ_WEAPON] ].sub_type == WPN_BOW)
          {
              text << "You can easily switch between weapons in slots a and "
                      "b by pressing <w>'<magenta>.";
          }
          break;
          
      case TUT_FLEEING_MONSTER:
          if (!Options.tutorial_type == TUT_BERSERK_CHAR)
              return;
              
          text << "While unsporting, it is sometimes useful to attack a fleeing "
                  "monster by throwing something after it. To do this, press "
                  "<w>t<magenta>, choose a throwing weapon, e.g. one of your "
                  "spears, use <w>+<magenta> to select a monster and press "
                  "<w>.<magenta>, <w>f<magenta> or <w>Enter<magenta>. The closest "
                  "monster will be autoselected. If you've got the fire_order "
                  "option set you can directly use <w>ff<magenta> or "
                  "<w>f+.<magenta> instead; the game will pick the first weapon "
                  "that fits the option.";
          break;
          
      case TUT_MONSTER_BRAND:
          text << "That monster looks a bit unusual. You might wish to examine "
                  "it a bit more closely by pressing <w>x<magenta> and moving "
                  "the cursor onto its square.";
          break;
          
      case TUT_SEEN_MONSTER:
      case TUT_SEEN_FIRST_OBJECT:
          break;
          
      default:
          text << "You've found something new (but I don't know what)!";
    }

    if ( !text.str().empty() )
    {
        std::string s = text.str();
        print_formatted_paragraph(s, get_tutorial_cols(), MSGCH_TUTORIAL);
    }

    Options.tut_just_triggered = true;
    Options.tutorial_events[seen_what] = 0;
    Options.tutorial_left--;
}

formatted_string tut_abilities_info()
{
    std::ostringstream text;
    text << "<magenta>"
        "This screen shows your character's set of talents. You can gain new   " EOL
        "abilities via certain items, through religion or by way of mutations. " EOL
        "Activation of an ability usually comes at a cost, e.g. nutrition or   " EOL
        "Magic power. ";
    
    if (you.religion != GOD_NO_GOD)
    {
       text <<
         "<w>Renounce Religion<magenta> will make your character leave your god" EOL
         "(and usually anger said god)";
       if (you.religion == GOD_TROG)
       {
         text << ", while <w>Berserk<magenta> temporarily increases your" EOL
                 "damage output in melee fights";
       }
       text << ".";
    }
    return formatted_string::parse_string(text.str(), false);
}

// a short explanation of Crawl's target mode
// and the most important commands
static std::string tut_target_mode(bool spells = false)
{
   std::string result;
   result = "then be taken to target mode with the nearest monster or previous "
            "target already targetted. You can also cycle through all hostile "
            "monsters in sight with <w>+<magenta> or <w>-<magenta>. "
            "Once you're aiming at the correct monster, simply hit "
            "<w>f<magenta>, <w>Enter<magenta> or <w>.<magenta> to shoot at it. "
            "If you miss, <w>";
          
   if (spells)
       result += "Zap";
   else
       result += "ff";
       
   result += "<magenta> fires at the same target again.";
   
   return (result);
}

static std::string tut_abilities()
{
   return ("To do this enter the ability menu with <w>a<magenta>, and then "
           "choose the corresponding ability. Note that such an attempt of "
           "activation, especially by the untrained, is likely to fail.");
}

static std::string tut_throw_stuff(item_def &item)
{
    std::string result;
    
    result  = "To do this, type <w>t<magenta> to throw, then <w>";
    result += item.slot;
    result += "<magenta> for ";
    result += (item.quantity > 1 ? "these" : "this");
    result += " ";
    result += item_base_name(item);
    result += (item.quantity > 1? "s" : "");
    result += ". You'll ";
    result += tut_target_mode();

    return (result);
}

void tutorial_describe_item(item_def &item)
{
    std::ostringstream ostr;
    ostr << "<magenta>";
    switch (item.base_type)
    {
       case OBJ_WEAPONS:
       {
            // for identified artefacts don't give all this information
            // (The screen is likely to overflow.)
            if (is_artefact(item) && item_type_known(item))
            {
                // exception: you can activate it
                if (gives_ability(item) && wherey() <= get_number_of_lines() - 5)
                {
                    ostr << "When wielded, some weapons (such as this one) "
                            "offer abilities that can be evoked. ";
                    ostr << tut_abilities();
                    break;
                } // or if it grants a resistance
                else if (gives_resistance(item)
                         && wherey() <= get_number_of_lines() - 3)
                {
                    ostr << "\nThis weapon offers its wearer protection from "
                            "certain sources. For an overview of your resistances "
                            "(among other things) type <w>%<magenta>.";
                    break;
                }
                return;
            }
            
            item_def *weap = you.slot_item(EQ_WEAPON);
            bool wielded = (weap && (*weap).slot == item.slot);
            bool long_text = false;

            if (!wielded)
            {
               ostr << "You can wield this weapon with <w>w<magenta>, or use "
                       "<w>'<magenta> to switch between the weapons in slot "
                       "a and b. (Use <w>=<magenta> to adjust item slots.)";
                    
               // weapon skill used by this weapon and the best weapon skill
               int curr_wpskill, best_wpskill;
               
               // maybe this is a launching weapon
               if (is_range_weapon(item))
               {
                   // then only compare with other launcher skills
                   curr_wpskill = range_skill(item);
                   best_wpskill = best_skill(SK_SLINGS, SK_DARTS, 99);
               }
               else 
               {
                   // compare with other melee weapons
                   curr_wpskill = weapon_skill(item);
                   best_wpskill = best_skill(SK_SHORT_BLADES, SK_STAVES, 99);
                   // maybe unarmed is better
                   if (you.skills[SK_UNARMED_COMBAT] > you.skills[best_wpskill])
                       best_wpskill = SK_UNARMED_COMBAT;
               }
                   
               if (you.skills[curr_wpskill] + 2 < you.skills[best_wpskill])
               {
                      ostr << "\nOn second look you've been training in <w>"
                           << skill_name(best_wpskill)
                           << "<magenta> for a while, so maybe you should "
                              "continue training that rather than <w>"
                           << skill_name(curr_wpskill)
                           << "<magenta>. (Type <w>m<magenta> to see the skill "
                              "management screen for the actual numbers.)";
                      long_text = true;
               }
            }
            else // wielded weapon
            {
                if (is_range_weapon(item))
                {
                    ostr << "To attack a monster, you only need to "
                            "<w>f<magenta>ire the appropriate type of ammunition. "
                            "You'll ";
                    ostr << tut_target_mode();
                }
                else
                {
                    ostr << "To attack a monster, you can simply walk into it.";
                }
            }
            if (is_throwable(item, you.body_size()) && !long_text)
            {
                ostr << "\n\nSome weapons (including this one), can also be "
                        "<w>t<magenta>hrown. ";
                ostr << tut_throw_stuff(item);
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
                     
                Options.tutorial_events[TUT_SEEN_RANDART] = 0;
            }
            if (item_known_cursed( item ) && !long_text)
            {
                ostr << "\n\nOnce wielded, a cursed weapon won't leave your hands "
                        "again until the curse has been lifted by reading a "
                        "scroll of remove curse or one of the enchantment scrolls.";
                        
                if (!wielded && is_throwable(item, you.body_size()))
                    ostr << " (Throwing it is safe, though.)";
                    
                Options.tutorial_events[TUT_YOU_CURSED] = 0;
            }
            Options.tutorial_events[TUT_SEEN_WEAPON] = 0;
            break;
       }
       case OBJ_MISSILES:
            if ( is_throwable(item, player_size()) )
            {
                ostr << item.name(DESC_CAP_YOUR)
                     << " can be thrown without the use of a launcher. ";
                ostr << tut_throw_stuff(item);
            }
            else if (is_launched(&you, you.weapon(), item))
            {
                ostr << "As you're already wielding the appropriate launcher, "
                        "you can simply <w>f<magenta>ire "
                     << (item.quantity > 1 ? "these" : "this")
                     << " " << item.name(DESC_BASENAME)
                     << (item.quantity > 1? "s" : "")
                     << ". You'll ";
                ostr << tut_target_mode();
            }
            else
            {
                ostr << "To shoot "
                     << (item.quantity > 1 ? "these" : "this")
                     << " " << item.name(DESC_BASENAME)
                     << (item.quantity > 1? "s" : "")
                     << ", first you need to <w>w<magenta>ield an appropriate "
                        "launcher.";
            }
            Options.tutorial_events[TUT_SEEN_MISSILES] = 0;
            break;
            
       case OBJ_ARMOUR:
       {
            bool wearable = true;
            if (you.species == SP_CENTAUR && item.sub_type == ARM_BOOTS)
            {
                ostr << "As a Centaur you cannot wear boots. "
                        "(Type <w>A<magenta> to see a list of your mutations "
                        "and innate abilities.)";
                wearable = false;
            }
            else if (you.species == SP_MINOTAUR && item.sub_type == ARM_HELMET
                     && get_helmet_type(item) != THELM_CAP
                     && get_helmet_type(item) != THELM_WIZARD_HAT)
            {
                ostr << "As a Minotaur you cannot wear helmets. "
                        "(Type <w>A<magenta> to see a list of your mutations "
                        "and innate abilities.)";
                wearable = false;
            }
            else
            {
                ostr << "You can wear pieces of armour with <w>W<magenta> and take "
                        "them off again with <w>T<magenta>.";
            }

            if (!item_type_known(item) &&
                (is_artefact(item) || get_equip_desc( item ) != ISFLAG_NO_DESC))
            {
                ostr << "\n\nWeapons and armour that have unusual descriptions "
                     << "like this are much more likely to be of higher "
                     << "enchantment or have special properties, good or bad. "
                     << "The rarer the description, the greater the potential "
                     << "value of an item.";

                Options.tutorial_events[TUT_SEEN_RANDART] = 0;
            }
            if (item_known_cursed( item ) && wearable)
            {
                ostr << "\nA cursed piece of armour, once worn, cannot be removed "
                        "again until the curse has been lifted by reading a "
                        "scroll of remove curse.";
            }
            if (gives_resistance(item))
            {
                    ostr << "\n\nThis armour offers its wearer protection from "
                            "certain sources. For an overview of your resistances "
                            "(among other things) type <w>%<magenta>.";
            }
            if (is_artefact(item) && gives_ability(item))
            {
                ostr << "\nWhen worn, some types of armour (such as this one) "
                        "offer abilities that can be evoked. ";
                ostr << tut_abilities();
            }
            Options.tutorial_events[TUT_SEEN_ARMOUR] = 0;
            break;
       }
       case OBJ_WANDS:
            ostr << "The magic within can be unleashed by <w>z<magenta>apping it.";
            Options.tutorial_events[TUT_SEEN_WAND] = 0;
            break;
            
       case OBJ_FOOD:
            ostr << "Food can simply be <w>e<magenta>aten. ";
            if (item.sub_type == FOOD_CHUNK)
            {
                ostr << "Note that most species refuse to eat raw meat unless "
                        "really hungry. ";
                if (item.special < 100)
                {
                    ostr << "Even fewer can safely digest rotten meat, and you're "
                            "probably not part of this group.";
                }
            }
            Options.tutorial_events[TUT_SEEN_FOOD] = 0;
            break;
            
       case OBJ_SCROLLS:
            ostr << "Type <w>r<magenta> to read this scroll. ";
            Options.tutorial_events[TUT_SEEN_SCROLL] = 0;
            break;
            
       case OBJ_JEWELLERY:
       {
            ostr << "Jewellery can be <w>P<magenta>ut on or <w>R<magenta>emoved "
                    "again. ";

            if (item_known_cursed( item ))
            {
                ostr << "\nA cursed piece of jewellery will cling to its "
                        "unfortunate wearer's neck or fingers until the curse is "
                        "finally lifted when he or she reads a scroll of remove "
                        "curse.";
            }
            if (gives_resistance(item))
            {
                    ostr << "\n\nThis "
                         << (item.sub_type < NUM_RINGS ? "ring" : "amulet")
                         << " offers its wearer protection "
                            "from certain sources. For an overview of your "
                            "resistances (among other things) type <w>%<magenta>.";
            }
            if (gives_ability(item))
            {
                ostr << "\nWhen worn, some types of jewellery (such as this one) "
                        "offer abilities that can be evoked. ";
                ostr << tut_abilities();
            }
            Options.tutorial_events[TUT_SEEN_JEWELLERY] = 0;
            break;
       }
       case OBJ_POTIONS:
            ostr << "Use <w>q<magenta> to quaff this potion. ";
            Options.tutorial_events[TUT_SEEN_POTION] = 0;
            break;
            
       case OBJ_BOOKS:
            if (!item_ident(item, ISFLAG_KNOW_TYPE))
            {
                ostr << "It's a book, you can <w>r<magenta>ead it. ";
            }
            else
            {
                if (item.sub_type == BOOK_MANUAL)
                {
                    ostr << "A manual can greatly help you in training a skill. "
                            "To use it, <w>r<magenta>ead it while your experience "
                            "pool (the number in brackets) is full. Note that "
                            "this will drain said pool, so only use this manual "
                            "if you think you need the skill in question.";
                }
                else // it's a spellbook
                {
                    if (you.religion == GOD_TROG)
                    {
                         ostr << "A spellbook! You could <w>M<magenta>emorize some "
                                 "spells and then cast them with <w>Z<magenta>. ";
                         ostr << "\nAs a worshipper of "
                              << god_name(GOD_TROG)
                              << ", though, you might instead wish to burn this "
                                 "tome of hated magic by using the corresponding "
                                 "<w>a<magenta>bility. "
                                 "Note that this only works on books that are "
                                 "lying on the floor and not on your current "
                                 "square. ";
                    }
                    else if (!you.skills[SK_SPELLCASTING])
                    {
                         ostr << "A spellbook! You could <w>M<magenta>emorize some "
                                 "spells and then cast them with <w>Z<magenta>. ";
                         ostr << "\nFor now, however, that will have to wait "
                                 "until you've learned the basics of Spellcasting "
                                 "by reading lots of scrolls.";
                    }
                    else // actually can cast spells
                    {
                         if (player_can_memorise(item))
                         {
                              ostr << "Such a <lightblue>highlighted spell<magenta> "
                                      "can be <w>M<magenta>emorised right away. ";
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
                             ostr << "\n\nTo do magic, type <w>Z<magenta> and "
                                     "choose a spell, e.g. <w>a<magenta> (check "
                                     "with <w>?<magenta>). For attack spells "
                                     "you'll ";
                             ostr << tut_target_mode(true);
                         }
                    }
                }
            }
            Options.tutorial_events[TUT_SEEN_SPBOOK] = 0;
            break;
            
       case OBJ_CORPSES:
            ostr << "Corpses lying on the floor can be <w>D<magenta>issected "
                    "with a sharp implement to produce chunks for food "
                    "(though they may not be healthy)";

            if (god_likes_butchery(you.religion))
            {
                ostr << ", or as a sacrifice to "
                     << god_name(you.religion)
                     << " (while <w>p<magenta>raying)";
            }

            ostr << ". ";
            if (item.special < 100)
                ostr << "Rotten corpses won't be of any use to you, though, so "
                        "you might just as well <w>d<magenta>rop this.";
            Options.tutorial_events[TUT_SEEN_CARRION] = 0;
            break;
            
       case OBJ_STAVES:
            ostr << "Staffs have to be <w>w<magenta>ielded to be of use. "
                    "There are staves that help spellcasters, and others that "
                    "allow you to harness spells hidden within.";
            if (!item_ident(item, ISFLAG_KNOW_TYPE))
            {
                ostr << "\n\nTo find out what this staff might do, you have to "
                        "wield it, then <w>E<magenta>voke it to see if anything "
                        "interesting happens. If nothing happens, it's probably "
                        "an ancient staff capable of enhancing certain spell "
                        "schools while suppressing others. ";
                if (you.spell_no)
                {
                    ostr << "You can find out which one, by casting spells while "
                            "wielding this staff. Eventually, the staff might "
                            "react to a spell of its school and identify itself.";
                }
            }
            else if (item.sub_type <= STAFF_AIR && gives_resistance(item))
            {
                ostr << "\nThis staff offers its wielder protection from "
                        "certain sources. For an overview of your resistances "
                        "(among other things) type <w>%<magenta>.";
            }

            Options.tutorial_events[TUT_SEEN_STAFF] = 0;
            break;
            
       case OBJ_MISCELLANY:
            ostr << "Miscellanous items sometimes harbour magical powers. Try "
                    "<w>w<magenta>ielding and <w>E<magenta>voking it.";
            Options.tutorial_events[TUT_SEEN_MISC] = 0;
            break;
            
       default:
            return;
    }

    std::string broken = ostr.str();
    linebreak_string2(broken, get_tutorial_cols());
    gotoxy(1, wherey() + 2);
    formatted_string::parse_block(broken, false).display();
} // tutorial_describe_item

bool tutorial_feat_interesting(dungeon_feature_type feat)
{
    if (feat >= DNGN_ALTAR_ZIN && feat <= DNGN_ALTAR_BEOGH)
        return true;
    if (feat >= DNGN_ENTER_ORCISH_MINES && feat <= DNGN_ENTER_SHOALS)
        return true;
        
    switch (feat)
    {
       case DNGN_ORCISH_IDOL:
       case DNGN_GRANITE_STATUE:
       case DNGN_TRAP_MAGICAL:
       case DNGN_TRAP_MECHANICAL:
       case DNGN_STONE_STAIRS_DOWN_I:
       case DNGN_STONE_STAIRS_DOWN_II:
       case DNGN_STONE_STAIRS_DOWN_III:
       case DNGN_STONE_STAIRS_UP_I:
       case DNGN_STONE_STAIRS_UP_II:
       case DNGN_STONE_STAIRS_UP_III:
       case DNGN_ROCK_STAIRS_DOWN:
       case DNGN_ROCK_STAIRS_UP:
            return true;
       default:
            return false;
    }
}

void tutorial_describe_feature(dungeon_feature_type feat)
{
    std::ostringstream ostr;
    ostr << "<magenta>";
    
    switch (feat)
    {
       case DNGN_ORCISH_IDOL:
       case DNGN_GRANITE_STATUE:
       case DNGN_TRAP_MAGICAL:
       case DNGN_TRAP_MECHANICAL:
            ostr << "These nasty constructions can do physical damage (with "
                    "darts or needles, for example) or have other, more magical "
                    "effects. ";
            if (feat == DNGN_TRAP_MECHANICAL)
            {
                ostr << "You can attempt to deactivate the mechanical type by "
                        "standing next to it and then pressing <w>Shift<magenta> "
                        "and the direction of the trap. Note that this usually "
                        "causes the trap to go off, so it can be quite a "
                        "dangerous task.";
            }
            Options.tutorial_events[TUT_SEEN_TRAP] = 0;
            break;
       case DNGN_STONE_STAIRS_DOWN_I:
       case DNGN_STONE_STAIRS_DOWN_II:
       case DNGN_STONE_STAIRS_DOWN_III:
            ostr << "You can enter the next (deeper) level by following them "
                    "down (<w>><magenta>). To get back to this level again, "
                    "press <w><<<magenta> while standing on the upstairs.";
            Options.tutorial_events[TUT_SEEN_STAIRS] = 0;
            break;
       case DNGN_STONE_STAIRS_UP_I:
       case DNGN_STONE_STAIRS_UP_II:
       case DNGN_STONE_STAIRS_UP_III:
            if (you.your_level < 1)
            {
                ostr << "These stairs lead out of the dungeon. Following them "
                        "will end the game. The only way to win is to transport "
                        "the fabled orb of Zot outside.";
            }
            else
            {
                ostr << "You can enter the previous (lower) level by following "
                        "these up (<w><<<magenta>). To get back to this level "
                        "again, press <w>><magenta> while standing on the "
                        "downstairs.";
            }
            Options.tutorial_events[TUT_SEEN_STAIRS] = 0;
            break;

       case DNGN_ROCK_STAIRS_DOWN:
       case DNGN_ROCK_STAIRS_UP:
            ostr << "Escape hatches can be used to quickly leave a level with "
                    "<w><<<magenta> and <w>><magenta>, respectively. Note that "
                    "you will usually be unable to return right away.";
            Options.tutorial_events[TUT_SEEN_ESCAPE_HATCH] = 0;
            break;
            
       case DNGN_ALTAR_ZIN:
       case DNGN_ALTAR_SHINING_ONE:
       case DNGN_ALTAR_KIKUBAAQUDGHA:
       case DNGN_ALTAR_YREDELEMNUL:
       case DNGN_ALTAR_XOM:
       case DNGN_ALTAR_VEHUMET:
       case DNGN_ALTAR_OKAWARU:
       case DNGN_ALTAR_MAKHLEB:
       case DNGN_ALTAR_SIF_MUNA:
       case DNGN_ALTAR_TROG:
       case DNGN_ALTAR_NEMELEX_XOBEH:
       case DNGN_ALTAR_ELYVILON:
       case DNGN_ALTAR_LUGONU:
       case DNGN_ALTAR_BEOGH:
       {
            god_type altar_god = grid_altar_god(feat);

            if (you.religion == GOD_NO_GOD)
            {
                ostr << "This is your chance to join a religion! In general, the "
                        "gods will help their followers, bestowing powers of all "
                        "sorts upon them, but many of them demand a life of "
                        "dedication, constant tributes or entertainment in return. "
                        "\nYou can get information about <w>"
                     << god_name(altar_god)
                     << "<magenta> by pressing <w>p<magenta> while standing on "
                        "the altar. Before taking up the responding faith you'll "
                        "be asked for confirmation.";
            }
            else
            {
                if (you.religion == altar_god)
                {
                    ostr << "If "
                         << god_name(you.religion)
                         << " likes to have items or corpses sacrificed on altars, "
                            "here you can do this by <w>d<magenta>ropping them, "
                            "then <w>p<magenta>raying. As a follower, pressing "
                            "<w>^<magenta> allows you to check "
                         << god_name(you.religion)
                         << "'s likes and dislikes at any time.";
                }
                else
                {
                    ostr << god_name(you.religion)
                         << " probably won't like it if you switch allegiance, "
                            "but having a look won't hurt: to get information on <w>";
                    ostr << god_name(altar_god);
                    ostr << "<magenta>, press <w>p<magenta> while standing on the "
                            "altar. Before taking up the responding faith (and "
                            "abandoning your current one!) you'll be asked for "
                            "confirmation."
                            "\nTo see your current standing with "
                         << god_name(you.religion)
                         << " press <w>^<magenta>.";
                }
            }
            Options.tutorial_events[TUT_SEEN_ALTAR] = 0;
            break;
       }
       case DNGN_ENTER_ORCISH_MINES:
       case DNGN_ENTER_HIVE:
       case DNGN_ENTER_LAIR:
       case DNGN_ENTER_SLIME_PITS:
       case DNGN_ENTER_VAULTS:
       case DNGN_ENTER_CRYPT:
       case DNGN_ENTER_HALL_OF_BLADES:
       case DNGN_ENTER_ZOT:
       case DNGN_ENTER_TEMPLE:
       case DNGN_ENTER_SNAKE_PIT:
       case DNGN_ENTER_ELVEN_HALLS:
       case DNGN_ENTER_TOMB:
       case DNGN_ENTER_SWAMP:
       case DNGN_ENTER_SHOALS:
            ostr << "An entryway into one of the many dungeon branches in Crawl. ";
            if (feat != DNGN_ENTER_TEMPLE)
                ostr << "Beware, sometimes these can be deadly!";
            break;

       default:
            return;
    }

    std::string broken = ostr.str();
    linebreak_string2(broken, get_tutorial_cols());
    formatted_string::parse_block(broken, false).display();
} // tutorial_describe_feature

bool tutorial_monster_interesting(const monsters *mons)
{
    if (mons_is_unique(mons->type) || mons->type == MONS_PLAYER_GHOST)
        return true;
        
    // highlighted in some way
    if (get_mons_colour(mons) != mons->colour)
        return true;

    // monster is (seriously) out of depth
    if (you.level_type == LEVEL_DUNGEON &&
        mons_level(mons->type) >= you.your_level + Options.ood_interesting)
    {
        return true;
    }
    return false;
}

void tutorial_describe_monster(const monsters *mons)
{
    std::ostringstream ostr;
    ostr << "<magenta>";

    if (mons_is_unique(mons->type))
    {
        ostr << "Did you think you were the only adventurer in the dungeon? "
                "Well, you thought wrong! These unique adversaries often "
                "possess skills that normal monster wouldn't, so be careful.";
    }
    else if(mons->type == MONS_PLAYER_GHOST)
    {
        ostr << "The ghost of a deceased adventurer, it would like nothing "
                "better than to send you the same way.";
    }
    else
    {
       int level_diff
           = mons_level(mons->type) - (you.your_level + Options.ood_interesting);

       if (you.level_type == LEVEL_DUNGEON && level_diff >= 0)
       {
           ostr << "This kind of monster is usually only encountered "
                << (level_diff > 5 ? "much " : "")
                << "deeper in the dungeon, so it's probably "
                << (level_diff > 5 ? "extremely" : "very")
                << " dangerous!\n\n";
       }
    }
    
    if (mons->has_ench(ENCH_BERSERK))
        ostr << "A berserking monster is bloodthirsty and fighting madly.\n";

    if (mons_friendly(mons))
    {
        ostr << "Friendly monsters will follow you around and attempt to aid "
                "you in battle.";
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mons_looks_stabbable(mons))
    {
        ostr << "Apparently it has not noticed you - yet.";
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mons_looks_distracted(mons))
    {
        ostr << "Apparently it has been distracted by something.";
    }

    std::string broken = ostr.str();
    linebreak_string2(broken, get_tutorial_cols());
    formatted_string::parse_block(broken, false).display();
} // tutorial_describe_monster
