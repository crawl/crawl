/*
 *  Created for Crawl Reference by JPEG on $Date: 2007-01-11$
 */

#include "AppHdr.h"
#include "tutorial.h"
#include <cstring>

#include "command.h"
#include "files.h"
#include "itemprop.h"
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
#define TUTORIAL_VERSION 101
int INFO2_SIZE = 500;
char info2[500];

static std::string tut_debug_list(int event);

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
        cprintf(EOL "SPACE - Back to class selection; Bksp - Back to race selection"
                EOL "* - Random tutorial; X - Quit" 
                EOL);
        printed = true;
    }       
    keyn = c_getch();
    
    if (keyn == '*')
        keyn = 'a' + random2(TUT_TYPES_NUM);
    
    // choose character for tutorial game
    if (keyn >= 'a' && keyn <= 'a' + TUT_TYPES_NUM - 1)
    {
        Options.tutorial_type = keyn - 'a';
        for (int i = 0; i < TUT_EVENTS_NUM; i++)
            Options.tutorial_events[i] = 1;
            
        Options.tut_explored = 1;
        Options.tut_stashes = 1;
        Options.tut_travel = 1; 
        // possibly combine to specialty_counter or something
        Options.tut_spell_counter = 0;
        Options.tut_throw_counter = 0;
        Options.tut_berserk_counter = 0;
        Options.tut_last_healed = 0;
            
        Options.tutorial_left = TUT_EVENTS_NUM;
        you.species = get_tutorial_species(Options.tutorial_type);
        you.char_class = get_tutorial_job(Options.tutorial_type);
        
        Options.random_pick = true; // random choice of starting spellbook
        Options.weapon = WPN_HAND_AXE; // easiest choice for dwarves
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
/*
  case TUT_MELEE_CHAR:
  strcpy(desc, "(Standard melee oriented character)");
  break;
*/                  
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
        snprintf(info, INFO_SIZE, "Error in print_tutorial_menu: type = %d", type);
        mpr( info );
        break;  
    }    
    
    cprintf("%c - %s %s %s" EOL, letter, species_name(get_tutorial_species(type), 1),
            get_class_name(get_tutorial_job(type)), desc);
}    

unsigned int get_tutorial_species(unsigned int type)
{
    switch(type)
    {
/*          
            case TUT_MELEE_CHAR:
            return SP_MOUNTAIN_DWARF;
*/
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
/*
  case TUT_MELEE_CHAR:
  return JOB_FIGHTER;   
*/
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

static formatted_string formatted_paragraph(std::string text, unsigned int width)
{
    std::string result;
    
    size_t start = 0, pos = 0, end = 0;
        
    for (; end < strlen(text.c_str()); pos = end+1)
    {
        // get next "word"
        end = text.find(' ', pos);
        if ( end >= strlen(text.c_str()) )
            pos = end;

        if (end - start >= width || end >= strlen(text.c_str()) )
        {
            end = pos;
            result += text.substr(start, end-start-1);
            result += EOL;
            start = pos;
        }
    }
    result += EOL;
    
    return formatted_string::parse_string(result);
}

static formatted_string tut_starting_info(unsigned int width)
{
    std::string result; // the entire page
    std::string text; // one paragraph
    
    result += "<magenta>Welcome to Dungeon Crawl!</magenta>" EOL EOL;
    
    text += "Your object is to lead a ";
    snprintf(info, INFO_SIZE, "%s %s ", species_name(get_tutorial_species(Options.tutorial_type), 1), 
             get_class_name(get_tutorial_job(Options.tutorial_type)));
    text += info2;
    text += "safely through the depths of the dungeon, retrieving the fabled orb of Zot and "
        "returning it to the surface. In the beginning, however, let discovery be your "
        "main goal: try to delve as deep as possible but beware, as death lurks around "
        "every corner here."; 
    result += formatted_paragraph(text, width);
                
    result += "For the moment, just remember the following keys and their functions:" EOL;
    result += "  <white>?</white> - shows the items and the commands" EOL;
    result += "  <white>S</white> - saves the game, to be resumed later (but note that death is permanent)" EOL;
    result += "  <white>x</white> - examine something in your vicinity" EOL EOL;

    text = "This tutorial will help you playing Crawl without reading any documentation. "
        "If you feel intrigued, there is more information available in these files "
        "(all of which can also be read in-game):";
    result += formatted_paragraph(text,width); 
        
    result += "  <darkgray>readme.txt</darkgray>         - A very short guide to Crawl." EOL;
    result += "  <darkgray>manual.txt</darkgray>         - This contains all details on races, magic, skills, etc." EOL;
    result += "  <darkgray>crawl_options.txt</darkgray>  - Crawl's interface is highly configurable. This document " EOL;
    result += "                       explains all the options (which themselves are set in " EOL;
    result += "                       the init.txt or .crawlrc files)." EOL;
    result += EOL;        
    result += "Press <white>Space</white> to proceed to the basics (the screen division and movement)." EOL;
    result += "Press <white>Esc</white> to fast forward to the game start." EOL EOL;


    return formatted_string::parse_string(result);
}    

formatted_string tutorial_debug()
{
    std::string result;
    bool lbreak = false;
    snprintf(info, INFO2_SIZE, "Tutorial Debug Screen");
    
    int i = 39 - strlen(info) / 2;
    result += std::string(i, ' ');
    result += "<white>";
    result += info;
    result += "</white>" EOL EOL;

    result += "<darkgray>";
    for (i=0; i < TUT_EVENTS_NUM; i++)
    {
        snprintf(info, INFO_SIZE, "%d: %d (%s)", 
                 i, Options.tutorial_events[i], tut_debug_list(i).c_str());
        result += info;
        
        // break text into 2 columns where possible
        if (strlen(info) > 39 || lbreak)
        {
            result += EOL;
            lbreak = false;
        }           
        else
        {
            result += std::string(39 - strlen(info), ' ');
            lbreak = true;
        }       
    }       
    result += "</darkgray>" EOL EOL;
    
    snprintf(info, INFO_SIZE, "You are a %s %s, and the tutorial will reflect that.",
             species_name(get_tutorial_species(Options.tutorial_type), 1), 
             get_class_name(get_tutorial_job(Options.tutorial_type)));
 
    result += info;
    
    return formatted_string::parse_string(result);
}    

std::string tut_debug_list(int event)
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
    case TUT_SEEN_RANDART: // doesn't work for now
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
/*
  case TUT_AUTOEXPLORE:
  return "learned about autoexplore";
*/           
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

static formatted_string tutorial_map_intro()
{
    std::string result;
    
    result += "<magenta>";
    result += "What you see here is the typical Crawl screen. The upper left map shows your " EOL;
    result += "hero as the <white>@<magenta> in the center. The parts of the map you remember but cannot " EOL;
    result += "currently see, will be greyed out." EOL;
    result += "</magenta>";
    result += "[--more--                                 Press <white>Escape</white> to skip the basics]";

    return formatted_string::parse_string(result);
}    

static formatted_string tutorial_stats_intro()
{
    std::string result;
    result += "<magenta>";

    result += "To the right, important properties of " EOL;
    result += "the character are displayed. The most " EOL;
    result += "basic one is your health, measured as " EOL;
    snprintf(info, INFO_SIZE, "<white>HP: %d/%d<magenta>. ", you.hp, you.hp_max);
    result += info; 
    if (Options.tutorial_type==TUT_MAGIC_CHAR) result += "  ";
    result += "These are your current out " EOL;
    result += "of maximum health points. When health " EOL;
    result += "drops to zero, you die.               " EOL;
    snprintf(info, INFO_SIZE, "<white>Magic: %d/%d<magenta>", you.magic_points, you.max_magic_points);
    result += info; 
    result += " is your energy for casting " EOL;
    result += "spells, although more mundane actions " EOL;
    result += "often draw from Magic, too.           " EOL;
    result += "Further down, <white>Str<magenta>ength, <white>Dex<magenta>terity and " EOL;
    result += "<white>Int<magenta>elligence are shown and provide an " EOL;
    result += "all-around account of the character's " EOL;
    result += "attributes.                           " EOL;

    result += "</magenta>";
    result += "                                      " EOL;    
    result += "[--more--                                 Press <white>Escape</white> to skip the basics]" EOL;
    result += "                                      " EOL;    
    result += "                                      " EOL;    
    
    return formatted_string::parse_string(result);
}    

static formatted_string tutorial_message_intro()
{
    std::string result;

    result += "<magenta>";
    result += "This lower part of the screen is reserved for messages. Everything related to " EOL;
    result += "the tutorial is shown in this colour. If you missed something, previous messages " EOL;
    result += "can be shown again with <white>Ctrl-P<magenta>. " EOL;
    result += "</magenta>";
    result += "[--more--                                 Press <white>Escape</white> to skip the basics]";
    
    return formatted_string::parse_string(result);
}    

static void tutorial_movement_info()
{
    snprintf(info, INFO_SIZE, "To move your character, use the numpad; try Numlock both on and off. If your ");
    formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
    snprintf(info, INFO_SIZE, "system has no number pad, or if you are familiar with the vi keys, movement is ");
    formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
    snprintf(info, INFO_SIZE, "also possible with <w>hjklyubn<magenta>. A basic command list can be found under <w>?<magenta>, and the ");
    formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
    snprintf(info, INFO_SIZE, "most important commands will be explained to you as it becomes necessary. ");
    formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
}

// copied from display_mutations and adapted    
void tut_starting_screen()
{
#ifdef DOS_TERM
    char buffer[4800];
#endif
    int x1, x2, y1, y2;
    int MAX_INFO = 4;
#ifdef TUTORIAL_DEBUG
    MAX_INFO++;
#endif    
    char ch;
    
    for (int i=0; i<=MAX_INFO; i++)
    {
        x1 = 1; y1 = 1;
        x2 = 80; y2 = 25;
     
        if (i==1 || i==3)
        {
            y1 = 19; // message window
        }    
        else if (i==2)
        {
            x2 = 40; // map window
            y2 = 18;
        }    
        
#ifdef DOS_TERM
        window(x1, y1, x2, y2);
        gettext(x1, y1, x2, y2, buffer);
#endif
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
        
#ifdef DOS_TERM
        puttext(x1, y1, x2, y2, buffer);
#endif
        redraw_screen();
        if (ch == ESCAPE)
            break;
            
    }
    // too important to break off early
    
}

void tutorial_death_screen()
{
    Options.tutorial_left = 0;
    
    mpr( "Welcome to Crawl! Stuff like this happens all the time.", MSGCH_TUTORIAL, 0);
    mpr( "Okay, that didn't go too well. Maybe you need to change your tactics. Here's one out of several hints:", MSGCH_TUTORIAL);
    more();
    
    int hint = random2(6);
    switch(hint)
    {
    case 0:
        snprintf(info2, INFO2_SIZE, "Remember to use those scrolls, potions or wands you've found. "
                 "Very often, you cannot expect to identify everything with the scroll only. Learn to improvise: "
                 "identify through usage.");
        mpr(info2, MSGCH_TUTORIAL);
        break;
    case 1:
        snprintf(info2, INFO2_SIZE, "Learn when to run away from things you can't handle - this is important! "
                 "It is often wise to skip a dangerous level. But don't overdo this as monsters will only get harder "
                 "the deeper you get.");
        mpr(info2, MSGCH_TUTORIAL);
        break;
    case 2: 
//                                     1   5   10   5   20    5   30    5   40    5   50    5   60    5   70    5   80 
        snprintf(info, INFO_SIZE, "Rest between encounters. In Crawl, searching and resting are one and the same.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "To search for one turn, press <w>s<magenta>, <w>.<magenta>, <w>delete<magenta> or <w>keypad-5<magenta>. Pressing <w>5<magenta> or");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "<w>shift-and-keypad-5<magenta> will let you rest for a longer time (you will stop resting");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "when fully healed).");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case 3:
        snprintf(info2, INFO2_SIZE, "Always consider using projectiles before engaging monsters in close combat.");
        mpr(info2, MSGCH_TUTORIAL);
        break; // check Options.tut_throw_counter
    case 4:
        snprintf(info2, INFO2_SIZE, "If you are badly wounded, you can run away from monsters to buy some time. "
                 "Try losing them in corridors or find a place where you can run around in circles to heal while the "
                 "monster chases you.");
        mpr(info2, MSGCH_TUTORIAL);
        break;
    case 5:
        snprintf(info2, INFO2_SIZE, "Never fight more than one monster if you can help it. Always back into a "
                 "corridor so that they must fight you one on one.");
        mpr(info2, MSGCH_TUTORIAL);
        break;
    default:
        snprintf(info2, INFO2_SIZE, "Sorry, no hint this time, although there should have been one.");
        mpr(info2, MSGCH_TUTORIAL);
    } 
    more();
    if (Options.tutorial_type == TUT_MAGIC_CHAR && Options.tut_spell_counter < 10 )
        mpr( "As a Conjurer your main weapon should be offensive magic. Cast spells more often!", MSGCH_TUTORIAL);
    else if (Options.tutorial_type == TUT_BERSERK_CHAR && Options.tut_berserk_counter < 1 )
        mpr( "Also remember to use Berserk when fighting stronger foes.", MSGCH_TUTORIAL);
    else if (Options.tutorial_type == TUT_RANGER_CHAR && Options.tut_throw_counter < 10 )
        mpr( "Also remember to use your bow and arrows against distant monsters.", MSGCH_TUTORIAL);
        
    mpr( "See you next game!", MSGCH_TUTORIAL);

    for ( long i = 0; i < TUT_EVENTS_NUM; ++i )
        Options.tutorial_events[i] = 0;
}    

void tutorial_finished()
{
    Options.tutorial_left = 0;
    snprintf(info, INFO_SIZE, "That's it for now: For the time being you've learned enough, so we'll let you ");
    mpr(info, MSGCH_TUTORIAL);
    snprintf(info, INFO_SIZE, "find out the rest for yourself. If you haven't already done so you might try a ");
    mpr(info, MSGCH_TUTORIAL);
    snprintf(info, INFO_SIZE, "different tutorial sometime to gain an insight into other aspects of Crawl. ");
    mpr(info, MSGCH_TUTORIAL);
    snprintf(info, INFO_SIZE, "Note that from now on the help screen (<w>?<magenta>) will look a bit different. Let us say");
    formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
    snprintf(info, INFO_SIZE, "goodbye with a playing hint:");          
    mpr(info, MSGCH_TUTORIAL);
    more();
    
    if (Options.tut_explored)
    {
        snprintf(info, INFO_SIZE, "Walking around and exploring levels gets easier by using auto-explore (<w>Ctrl-O<magenta>).");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
    }    
    else if (Options.tut_travel)
    {
//                                 1   5   10   5   20    5   30    5   40    5   50    5   60    5   70    5   80 
        snprintf(info, INFO_SIZE, "There is a convenient way for travelling between far away dungeon levels: press");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "<w>Ctrl-G<magenta> and enter the desired destination. If your travel gets interrupted,");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "issueing <w>Ctrl-G Enter<magenta> will continue it.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
    }       
    else if (Options.tut_stashes)
    {
        snprintf(info, INFO_SIZE, "To keep an account on the great many items scattered throughout the dungeon,");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "use the <w>Ctrl-F<magenta> command. This lets you find anything that matches the word you");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "entered. For example, <w>Ctrl-F \"knife\"<magenta> will list all places where knives have");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "been seen. Also, by pressing the letter of one the spots, you will be able to");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "travel there automatically. It is even possible to search for dungeon features");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "like <w>\"shop\"<magenta>, <w>\"altar\"<magenta>, or for more general terms like <w>\"blade\"<magenta>. The latter");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "will search for all weapons falling into either the Short Blades or the Long");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "Blades classes.");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "For those familiar with regular expressions: these can be used here as well.");
        mpr(info, MSGCH_TUTORIAL);
    }    
    else
    {
        switch (random2(2)+3)
        {
        case 3:    
            snprintf(info, INFO_SIZE, "The game will keep an automated logbook for your characters. Use <w>?:<magenta> to read it.");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "You can also enter notes manually with the <w>:<magenta> command. Once your character");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "perishes, two morgue files will be left (look in the <w>/morgue<magenta> directory). The");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "one ending in txt contains a copy of your logbook.");
            mpr(info, MSGCH_TUTORIAL);
            break;          
        case 4:
            snprintf(info, INFO_SIZE, "Crawl has a macro function built in: press <w>~m<magenta> to define a macro by first");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "specifying a trigger key (say, <w>F<magenta>) and a command sequence. So, for example");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "<w>~mFZa+.<magenta> will make the <w>F<magenta> key always zap the spell in slot a on the nearest");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "monster. Do not overwrite crucial commands! The functions keys F1-F12 are");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "generally safe. The <w>~<magenta> also lets you define keybindings. These are useful when");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "some keys do not work as expected on your system. In addition, you can define");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "macro-type sequences for the level map or targeting modes only.");
            mpr(info, MSGCH_TUTORIAL);
            break;
            /*
              case 5: 
              snprintf(info2, INFO2_SIZE, "You can use the <w>{<magenta> command to inscribe some words on one of your items. "
              "Besides general item-related notes, there are some further functions for this: inscribing <w>@w1<magenta> on a weapon "
              "lets you wield it with <w>w1<magenta>, regardless of the inventory letter (<w>1<magenta> can be replaced by any digit and <w>w<magenta> "
              "could be any action key or <w>*<magenta>, meaning all actions). Inscribing <w>!d<magenta> will ask you before dropping the item.");
              break;
              case 6:
              snprintf(info2, INFO2_SIZE, "The interface can be greatly customised. All options are explained in the file "
              "<w>crawl_options.txt<magenta> which should be in the <w>/docs<magenta> directory. The options are set in <w>init.txt<magenta> or <w>white .crawlrc<magenta>. "
              "Crawl will complain when it can't find either.\n"
              "Options of the yes/no (or true/false) type appear the opposite of their default, e.g. <w>\"#show_turns = false\"<magenta>. "
              "Just removing the comment sign <w><magenta> will change the behaviour of such options. Other options require numbers/words as "
              "arguments - read the doc file for these.");
              break;    
            */
        default: 
            snprintf(info2, INFO2_SIZE, "Oops... No hint for now. Better luck next time!");
        } 
    }    
    mpr("Have a crunchy Crawling!", MSGCH_TUTORIAL, 0);
    more();
    
    Options.tutorial_left = 0;
    for ( long i = 0; i < TUT_EVENTS_NUM; ++i )
        Options.tutorial_events[i] = 0;
}    

void tutorial_prayer_reminder()
{
    if (coinflip())
    {// always would be too annoying
        snprintf(info, INFO_SIZE, "Remember to <w>p<magenta>ray before battle, so as to dedicate your kills to %s. ", god_name(you.religion));
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "Should the monster leave a corpse, consider <w>D<magenta>issecting it as a sacrifice to");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "%s, as well.", god_name(you.religion));
        mpr(info, MSGCH_TUTORIAL);
    }       
}    

void tutorial_healing_reminder()
{
    if (you.poison)
    {
        if (Options.tutorial_events[TUT_NEED_POISON_HEALING])
            return;
        learned_something_new(TUT_NEED_POISON_HEALING);
    }    
    else
    {
        if (Options.tutorial_events[TUT_NEED_HEALING]) 
            learned_something_new(TUT_NEED_HEALING);
        else if (you.num_turns - Options.tut_last_healed >= 50 && !you.poison)
        {
            snprintf(info, INFO_SIZE, "Remember to rest between fights and to enter unexplored terrain with full");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "hitpoints and/or magic. To do this, press <w>5<magenta>.");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        }           
        Options.tut_last_healed = you.num_turns;
    }       
}    

void taken_new_item(unsigned char item_type)
{
/*    
      if (Options.tutorial_events[TUT_SEEN_FIRST_OBJECT])
      learned_something_new(TUT_SEEN_FIRST_OBJECT);
*/             
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
    case OBJ_STAVES:
    case OBJ_MISCELLANY:
        learned_something_new(TUT_SEEN_MISC);
        break;
    default: /* nothing to be done */
        return;
    }    
}    

void tutorial_first_monster(monsters mon)
{
    if (!Options.tutorial_events[TUT_SEEN_MONSTER])
        return;
        
    std::string help = "<magenta>That ";
    formatted_string st = formatted_string::parse_string(help);
    st.formatted_string::textcolor(channel_to_colour(MSGCH_TUTORIAL));
    st.formatted_string::add_glyph(&mon);
    help = " is a monster, usually depicted by letters, e.g. typical early monsters ";
    st += formatted_string::parse_string(help);
    formatted_mpr(st, MSGCH_TUTORIAL);
    
    learned_something_new(TUT_SEEN_MONSTER);
}    

void tutorial_first_item(item_def item)
{
    if (!Options.tutorial_events[TUT_SEEN_FIRST_OBJECT] || Options.tut_just_triggered)
        return;

    std::string help = "<magenta>That ";
    formatted_string st = formatted_string::parse_string(help);
    st.formatted_string::textcolor(channel_to_colour(MSGCH_TUTORIAL));
    st.formatted_string::add_glyph(&item);
    help = " is an item. If you move to this spot and press <w>g<magenta> or <w>,<magenta> you can pick it up.";
    st += formatted_string::parse_string(help);
    formatted_mpr(st, MSGCH_TUTORIAL);
    
    learned_something_new(TUT_SEEN_FIRST_OBJECT);
}     

void learned_something_new(unsigned int seen_what, int x, int y)
{ 
    // already learned about that
    if (!Options.tutorial_events[seen_what])
        return;
        
    // don't trigger twice in the same turn 
    if (Options.tut_just_triggered && seen_what != TUT_SEEN_MONSTER)
        return;
        
    // If seeing something right at game start wait for input
/*    if (!you.num_turns && !you.turn_is_over && getch() == 0)
      getch();
*/     
    switch(seen_what)
    {
    case TUT_SEEN_FIRST_OBJECT:
//                                     1   5   10   5   20    5   30    5   40    5   50    5   60    5   70    5   80 
        snprintf(info, INFO_SIZE, "Generally, items are shown by non-letter symbols like <w>%%?!\"=()[<magenta>. Once picked up,");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "you can drop it again with <w>d<magenta>. Depending on the current options, several types of");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "objects will be picked up automatically.");  
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_POTION:
        snprintf(info, INFO_SIZE, "You have picked up your first potion ('<w>!<magenta>'). Use <w>q<magenta> to quaff (drink) it.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_SCROLL:
        snprintf(info, INFO_SIZE, "You have picked up your first scroll ('<w>?<magenta>'). Type <w>r<magenta> to read it.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_WAND:
        snprintf(info, INFO_SIZE, "You have picked up your first wand ('<w>/<magenta>'). Type <w>z<magenta> to zap it.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_SPBOOK:
        snprintf(info, INFO_SIZE, "You have picked up a spellbook ('<w>+<magenta>' or '<w>:<magenta>'). You can read it by typing <w>r<magenta>,");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "memorise spells via <w>M<magenta> and cast a memorised spell with <w>Z<magenta>.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        if (you.skills[SK_SPELLCASTING])
        {
            snprintf(info, INFO_SIZE, "However, for now you will be unable to do so. First your mind has to become");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "accustomed to using magic by reading lots of scrolls.");
            mpr(info, MSGCH_TUTORIAL);
        }       
        break;
    case TUT_SEEN_WEAPON:
        snprintf(info, INFO_SIZE, "This is the first weapon ('<w>(<magenta>') you've picked up. Use <w>w<magenta> to wield it, but be");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "aware that this weapon might use a different skill from the one you've been");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "training with your current one. You can use <w>v<magenta> (followed by the weapon's letter)");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "to find out about that and other extras the weapon might have. To take a look at");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "your skills, press <w>m<magenta>.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_MISSILES:
        snprintf(info, INFO_SIZE, "This is the first stack of missiles ('<w>)<magenta>') you've picked up. Darts can be thrown");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "by hand, but other missile types like arrows and needles require a launcher (and");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "training in using it) to be really effective. <w>v<magenta> gives more information about");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "both missiles and launcher.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        if (Options.tutorial_type == TUT_RANGER_CHAR)
        {
            snprintf(info, INFO_SIZE, "As you're already trained in Bows you should stick with arrows and collect more of them in the dungeon.");
        }
        else if (Options.tutorial_type == TUT_MAGIC_CHAR)
        {
            snprintf(info2, INFO2_SIZE, "However, as a spellslinger you don't really need another type of ranged attack, unless there's another effect in addition to damage.");
        }        
        else
        {
            snprintf(info2, INFO2_SIZE, "For now you might be best off with sticking to darts or stones for ranged attacks.");
        }
        mpr(info2, MSGCH_TUTORIAL, 1);
        break;
    case TUT_SEEN_ARMOUR:
        snprintf(info, INFO_SIZE, "This is the first piece of armour ('<w>[<magenta>') you've picked up. Use <w>W<magenta> to wear it and");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "<w>T<magenta> to take it off again. You can also use <w>v<magenta> (followed by the armour's letter)");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "to get some information about it.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        if (you.species == SP_CENTAUR)
        {
            snprintf( info, INFO_SIZE, "Note that as a %s, you will be unable to wear boots.", species_name(you.species, 1) );
            mpr(info, MSGCH_TUTORIAL);
        }       
        else if (you.species == SP_MINOTAUR)
        {
            snprintf( info, INFO_SIZE, "Note that as a %s, you will be unable to wear helmets.", species_name(you.species, 1) );
            mpr(info, MSGCH_TUTORIAL);
        }   
        break;
    case TUT_SEEN_RANDART:
        snprintf(info, INFO_SIZE, "Weapons and armour that have descriptions like this are more likely to be of higher"
                 "enchantment or have special properties, good or bad.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_FOOD:
        snprintf(info, INFO_SIZE, "You have picked up some food ('<w>%%<magenta>'). You can eat it by typing <w>e<magenta>.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        break;
    case TUT_SEEN_CARRION:
        snprintf(info, INFO_SIZE, "You have picked up a corpse ('<w>%%<magenta>'). You can dissect it with <w>D<magenta> - it needs to be");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "on the ground for that to work, though - and <w>e<magenta>at the resulting chunks (though");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "they may not be healthy). During prayer you can offer corpses to your god by dissecting them, as well.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_JEWELLERY:
        snprintf(info, INFO_SIZE, "You have picked up a a piece of jewellery, either a ring ('<w>=<magenta>') or an amulet");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "an amulet ('<w>\"<magenta>'). Type <w>P<magenta> to put it on and <w>R<magenta> to remove it. You can also use <w>v<magenta>"); 
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "(followed by the appropriate letter) to get some information about it.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        break;
    case TUT_SEEN_MISC:
        snprintf(info, INFO_SIZE, "This is a strange object. You can play around with it to find out what it does");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "by <w>w<magenta>ielding and <w>E<magenta>voking it.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        break;
    case TUT_SEEN_MONSTER:
//          snprintf(info, INFO_SIZE, "The %s is a monster, usually depicted by letters, e.g. typical early monsters are", st.c_str());
        snprintf(info, INFO_SIZE, "are <brown>r<magenta>, <w>g<magenta>, <lightgray>b<magenta> or <brown>K<magenta>. You can gain some information about it by pressing <w>x<magenta>, moving");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "the cursor on the monster and then pressing <w>v<magenta>. To attack it with your current");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "weapon, simply move into it.");
        mpr(info, MSGCH_TUTORIAL);

        if (Options.tutorial_type == TUT_RANGER_CHAR)
        {
            snprintf(info, INFO_SIZE, "However, as a hunter you might want to deal with it using your bow. Do this");
            mpr(info, MSGCH_TUTORIAL);  
            snprintf(info, INFO_SIZE, "with the following keypresses: <w>wbf+.<magenta> Here, <w>wb<magenta> wields the bow, <w>f<magenta> fires");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
            snprintf(info, INFO_SIZE, "appropriate ammunition (your arrows) and enters targeting mode. A very simple");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "way to aim is by pressing <w>+<magenta> and <w>-<magenta> until you target the proper monster. Finally,");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
            snprintf(info, INFO_SIZE, "<w>.<magenta> or <w>Enter<magenta> will fire. If you miss, <w>ff<magenta> will fire at the previous target.");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        }       
        else if (Options.tutorial_type == TUT_MAGIC_CHAR)
        {
            snprintf(info, INFO_SIZE, "However, as a conjurer you might want to deal with it using magic. Do this");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "with the following key presses: <w>Za+.<magenta> Here, <w>Za<magenta> zaps the first spell you know,");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "%s, and enters targeting mode. A very simple way to aim is by ", spell_title(get_spell_by_letter('a')));
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "pressing <w>+<magenta> and <w>-<magenta> until you target the proper monster. Finally, fire by pressing");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "<w>.<magenta> or <w>Enter<magenta>. If you miss, <w>Zap<magenta> will fire at the previous target.");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        }
        break;
    case TUT_SEEN_STAIRS:
        snprintf(info, INFO_SIZE, "These are some downstairs. You can enter the next (deeper) level by following");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "them down (<w>><magenta>). To get back to this level again, press <w><<<magenta> while standing on the");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "upstairs.");
        mpr(info, MSGCH_TUTORIAL);
        break;    
    case TUT_SEEN_TRAPS:
        snprintf(info, INFO_SIZE, "Oops... you just entered a trap. An unwary adventurer will occasionally stumble");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "into one of these nasty constructions depicted by <w>^<magenta>. They can do physical");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "damage (with darts or needles, for example) or have other, more magical "
                 "effects, e.g. teleportation.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_ALTAR:
        snprintf(info, INFO_SIZE, "The <w>_<magenta> is an altar. You can get information about it or join this god by pressing");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "<w>p<magenta> while standing on the square. Don't worry: You'll be asked for confirmation!");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_SHOP:
        snprintf(info, INFO_SIZE, "The <w>%c<magenta> is a shop. You can enter it same as you would a new level, by typing <w><<<magenta>.", get_screen_glyph(x,y));
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_SEEN_DOOR:
        snprintf(info, INFO_SIZE, "The <w>%c<magenta> is a closed door. You can open it by walking into it.", get_screen_glyph(x,y));
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "Sometimes it is useful to close a door. Do so by pressing <w>c<magenta>, followed by the direction.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_KILLED_MONSTER:
        snprintf(info, INFO_SIZE, "Congratulations, your character just gained some experience by killing this "
                 "monster! By taking actions, certain skills will be trained and become better.");
        mpr(info, MSGCH_TUTORIAL);          
        if (/*Options.tutorial_type == TUT_MELEE_CHAR ||*/ Options.tutorial_type == TUT_BERSERK_CHAR)           
            snprintf(info, INFO_SIZE, "For example, killing monsters in melee battle will raise your Axes and Fighting skills.");
        else if (Options.tutorial_type == TUT_RANGER_CHAR)
            snprintf(info, INFO_SIZE, "For example, killing monsters using bow and arrows will raise your Bows and Ranged Combat skills.");
        else //if (Options.tutorial_type == TUT_MAGIC_CHAR)
            snprintf(info, INFO_SIZE, "For example, killing monsters with offensive magic will raise your Conjurations" "and Spellcasting skills.");
        mpr(info, MSGCH_TUTORIAL);
            
        if (you.religion != GOD_NO_GOD)
        {
            snprintf(info, INFO_SIZE, "To dedicate your kills to %s, pray (<w>p<magenta>) before battle. ", god_name(you.religion));
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "Not all gods will be equally pleased at doing this, nor will they accept every kill.");
            mpr(info, MSGCH_TUTORIAL);
        }       
        break;
    case TUT_NEW_LEVEL:
        snprintf(info2, INFO2_SIZE, "Well done! Reaching a new experience level is always a nice event: "
                 "you get more health and magic points, and occasionally increases to your attributes "
                 "(strength, dexterity, intelligence).");
        mpr(info2, MSGCH_TUTORIAL);
        if (Options.tutorial_type == TUT_MAGIC_CHAR)
        {
            snprintf(info, INFO_SIZE, "New experience levels will let you learn more spells (the Spellcasting skill");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "also does this). For now, you should try to memorise the second spell of your");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "starting book with <w>M<magenta>, the letter of the book, and <w>b<magenta>. Once learnt, you can zap");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "it with <w>Zb<magenta>.");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        }           
        break;
    case TUT_SKILL_RAISE:
        snprintf(info, INFO_SIZE, "One of your skills just got raised. Type <w>m<magenta> to take a look at your skills screen.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_YOU_ENCHANTED:                 
        snprintf(info, INFO_SIZE, "Enchantments of all types can befall you temporarily. Abbreviated signalisation"); 
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "appears at the lower end of the stats area. Press <w>@<magenta> for more details. A list of");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "all possible enchantments is given in the manual."); 
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_YOU_SICK:
        learned_something_new(TUT_YOU_ENCHANTED);   
        snprintf(info, INFO_SIZE, "Sometimes corpses become spoiled or inedible, resulting in sickness. Also, some");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "monsters' flesh is less palatable than others'. While sick, your hitpoints will");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "not regenerate, and sickness often takes a toll on your body. It wears off with");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "time (wait with <w>5<magenta>) or, if you've got the means, you could heal yourself.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;          
    case TUT_YOU_POISON:                
        learned_something_new(TUT_YOU_ENCHANTED);   
        snprintf(info, INFO_SIZE, "Poison will slowly reduce your hp. It wears off with time (wait with <w>5<magenta>) or, if");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "you've got the means, you could heal yourself.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_YOU_CURSED:
        snprintf(info2, INFO2_SIZE, "Curses are comparatively harmless but they do mean that you cannot remove cursed "
                 "equipment and will have to suffer the (possibly) bad effects until you find and read a scroll to remove the curse.");
        mpr(info2, MSGCH_TUTORIAL);
        break;          
    case TUT_YOU_HUNGRY:
        snprintf(info, INFO_SIZE, "There are two ways to overcome hunger: food rations you started with or found,");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "and selfmade food from corpses. To get the latter, all you need to do is chop");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "up a corpse, that is, <w>D<magenta>issect it while standing on the corpse in question. You");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "need to wield a sharp weapon to do that. Your starting weapon will do nicely.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_YOU_STARVING:
        snprintf(info, INFO_SIZE, "You are now suffering from terrible hunger. You'll need to <w>e<magenta>at something");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "quickly, or you'll starve. The safest way to deal with this is to simply eat something from");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "your inventory rather than wait for a monster to leave a corpse.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_MULTI_PICKUP:
        snprintf(info, INFO_SIZE, "There's a more comfortable way to pick up several items at the same time. If");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "you press <w>,<magenta> or <w>g<magenta> twice you can choose items from a menu. This takes");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "less keystrokes but has no influence on the number of turns needed. Multi-pickup will be");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "interrupted by a monster attacking you or similar potentially dangerous events.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_HEAVY_LOAD:
        if (you.burden_state != BS_UNENCUMBERED)
            snprintf(info, INFO_SIZE, "It is not usually a good idea to run around encumbered; it slows you down and increases your hunger.");
        else 
            snprintf(info, INFO_SIZE, "Sadly, your inventory is limited to 52 items, and it appears your knapsack is full.");   
        mpr(info, MSGCH_TUTORIAL);

        snprintf(info, INFO_SIZE, "However, this is easy enough to rectify: simply <w>d<magenta>rop some of the stuff you");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "don't need or that's too heavy to lug around permanently.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_ROTTEN_FOOD:
        snprintf(info, INFO_SIZE, "One or more of the chunks or corpses you carry has started to rot. Few races can digest these safely, so you might "
                 "just as well <w>d<magenta>rop them now.");
        mpr(info, MSGCH_TUTORIAL);
        if (you.religion == GOD_TROG || you.religion == GOD_MAKHLEB || you.religion == GOD_OKAWARU)
        {
            snprintf(info, INFO_SIZE, "Also, if it is a rotting corpse you carry now might be a good time to <w>d<magenta>rop");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "and <w>D<magenta>issect it during prayer (<w>p<magenta>) as an offer to %s.", god_name(you.religion));
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        }       
        break;
    case TUT_MAKE_CHUNKS:
        snprintf(info, INFO_SIZE, "How lucky! You just killed a monster which left a corpse. Standing over the");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "corpse, you can press <w>D<magenta> to dissect it. One or more chunks will appear, which");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "can be eaten with <w>e<magenta>.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "Be warned that not all chunks are edible, some never, others only sometimes. Only experience will "
                 "help you, here. But remember that you are what you eat.");
        mpr(info, MSGCH_TUTORIAL);
            
        if (you.duration[DUR_PRAYER] && 
            (you.religion == GOD_OKAWARU || you.religion == GOD_MAKHLEB || you.religion == GOD_TROG || you.religion == GOD_ELYVILON))
        {
            snprintf(info, INFO_SIZE, "Note that doing this while praying will instead sacrifice it to %s, ", god_name(you.religion));
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "which a god may or may not like. (Type <w>^<magenta> to get a hint about that.)");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        }           
        break;
    case TUT_SHIFT_RUN:
        snprintf(info, INFO_SIZE, "Walking around takes less keystrokes if you press <w>Shift<magenta> while moving. That will");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "let you run into the specified direction until a monster comes into sight or your character "
                 "sees something interesting. Shift-running does not reduce the number of turns needed to move.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_MAP_VIEW:
        snprintf(info, INFO_SIZE, "As you explore more and more of a level, orientation becomes rather difficult.");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "Here the map overview comes in handy: Press <w>X<magenta> to see a much larger portion of");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "the map. You can easily return to the main screen with a number of keys. If you");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "move your cursor while on the level map and then press <w>.<magenta> your character will");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "attempt to move there on his own. Travel will be interrupted by monsters");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "stepping into sight, but can be resumed by pressing <w>X.<magenta> once more. Also, <w><<<magenta> and <w>><magenta>");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "can be used as shortcuts for cycling through the known staircases.");
        mpr(info, MSGCH_TUTORIAL);
        break;
/*
  case TUT_AUTOEXPLORE:
  snprintf(info2, INFO2_SIZE, "Exploration of a level gets even easier by another shortcut command: <w>Ctrl-O<magenta> will automatically move towards the "
  "nearest unexplored part of the map, but stop whenever you find something interesting or a monster moves into sight.");
  break;
*/              
    case TUT_DONE_EXPLORE:
        snprintf(info, INFO_SIZE, "You have explored the dungeon on this level. Search for some downstairs, which");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "look like this: '<w>><magenta>'. In order to descend, press <w>><magenta> when standing on such a");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "staircase.");
        mpr(info, MSGCH_TUTORIAL);
        if (Options.tutorial_events[TUT_SEEN_STAIRS])
        {
            snprintf(info, INFO_SIZE, "In rare cases, you may have found no downstairs at all. Try searching for");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "secret doors in suspicious looking spots; use <w>s<magenta>, <w>.<magenta> or <w>5<magenta> to do so.");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        }       
        else
        {
            snprintf(info, INFO_SIZE, "Each level of Crawl has at least three up and three down stairs. If you've");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "encountered less than that, you've explored only part of this level. The rest");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "may be accessible via another level or through secret doors. To find the");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "latter, try searching the walls with <w>s<magenta>, <w>.<magenta> (search for one turn) or <w>5<magenta> (search");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "longer).");
            mpr(info, MSGCH_TUTORIAL);
        }
        break;
    case TUT_NEED_HEALING:
        snprintf(info, INFO_SIZE, "If you're low on hitpoints or magic and there's no urgent need to move, you can");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "rest for a bit. Press <w>5<magenta> or <w>shift-numpad-5<magenta> to do so.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_NEED_POISON_HEALING:
//                                     1   5   10   5   20    5   30    5   40    5   50    5   60    5   70    5   80 
        snprintf(info, INFO_SIZE, "Your poisoning could easily kill you, so now would be a good time to <w>q<magenta>uaff a");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);    
        snprintf(info, INFO_SIZE, "potion of heal wounds, or better yet, a potion of healing. If you haven't seen");
        mpr(info, MSGCH_TUTORIAL);        
        snprintf(info, INFO_SIZE, "one of these so far, try unknown ones in your inventory. Good luck!");
        mpr(info, MSGCH_TUTORIAL);        
        break;
    case TUT_POSTBERSERK:
        snprintf(info, INFO_SIZE, "Berserking is extremely exhausting! It burns a lot of nutrition, and afterwards");
        mpr(info, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "you are slowed down and occasionally even pass out.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_RUN_AWAY:
        snprintf(info2, INFO2_SIZE, "Sometimes, when you've got only few hitpoints left and you're in danger of dying, retreat might be "
                 "the best option. %sTry to shake off any monster following you by leading them through corridors, or once there's some space between you "
                 "and your pursuer, change levels.", (you.species == SP_CENTAUR ? "As a four-legged centaur you are particularly well equipped "
                                                      "for doing so. " : "") );
        mpr(info2, MSGCH_TUTORIAL);
        if (Options.tutorial_type == TUT_BERSERK_CHAR && !you.berserker)
        {
            snprintf(info, INFO_SIZE, "Also, with %s's support you can use your Berserk ability (<w>a<magenta>) to ", god_name(you.religion));
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "temporarily gain more hitpoints and greater strength. Be warned that going into");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "a frenzy like that also has its drawbacks.");
            mpr(info, MSGCH_TUTORIAL);
        }    
        break;
    case TUT_YOU_MUTATED:
        snprintf(info2, INFO_SIZE, "Mutations can be obtained from several sources, among them potions of "
                 "mutations, spell miscasts, and overuse of strong enchantments like Haste or "
                 "Invisibility. The only reliable way to get rid of mutations is with potions of "
                 "cure mutation. Almost all mutations occur in three degrees. There are about as "
                 "many harmful as beneficial mutations.");
        mpr(info2, MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "To have a look at your mutations press <w>A<magenta>.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        break;
    case TUT_NEW_ABILITY:
        snprintf(info, INFO_SIZE, "You just gained a new ability. Press <w>a<magenta> to take a look at your abilities or use");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        snprintf(info, INFO_SIZE, "one of them.");
        mpr(info, MSGCH_TUTORIAL);
        break;
    case TUT_WIELD_WEAPON:
        snprintf(info, INFO_SIZE, "You might want to <w>w<magenta>ield a more suitable implement when attacking monsters.");
        formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        if (item_cursed( you.inv[ you.equip[EQ_WEAPON] ] ))
        {
            snprintf(info, INFO_SIZE, "To get rid of the curse you need to read a scroll of remove curse or enchant weapon.");
            mpr(info, MSGCH_TUTORIAL);
        }       
        else if (Options.tutorial_type == TUT_RANGER_CHAR && you.inv[ you.equip[EQ_WEAPON] ].sub_type == WPN_BOW)
        {
//                                         1   5   10   5   20    5   30    5   40    5   50    5   60    5   70    5   80 
            snprintf(info, INFO_SIZE, "You can easily switch between your primary (a) and secondary (b) weapon by ");
            mpr(info, MSGCH_TUTORIAL);
            snprintf(info, INFO_SIZE, "pressing <w>'<magenta>.");
            formatted_mpr(formatted_string::parse_string(info), MSGCH_TUTORIAL);
        }       
        break; 
    default: 
//                                     1   5   10   5   20    5   30    5   40    5   50    5   60    5   70    5   80 
        snprintf(info, INFO_SIZE, "You've found something new (but I don't know what)!");
        mpr(info, MSGCH_TUTORIAL);
    }    
    more();
    
    Options.tut_just_triggered = true;
    
    Options.tutorial_events[seen_what] = 0;
    Options.tutorial_left--;
    
    // there are so many triggers out there, chances that all will be called are small
/*  if (Options.tutorial_left < 10)
    tutorial_finished(); */
}
