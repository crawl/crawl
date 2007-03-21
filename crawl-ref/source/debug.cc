/*
 *  File:       debug.cc
 *  Summary:    Debug and wizard related functions.
 *  Written by: Linley Henzell and Jesse Jones
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <4>     14/12/99       LRH             Added cast_spec_spell_name()
 *               <3>     5/06/99        JDJ     Added TRACE.
 *               <2>     -/--/--        JDJ     Added a bunch od debugging macros.
 *                                                   Old code is now #if WIZARD.
 *               <1>     -/--/--        LRH     Created
 */

#include "AppHdr.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#ifdef UNIX
#include <errno.h>
#endif

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "branch.h"
#include "direct.h"
#include "describe.h"
#include "dungeon.h"
#include "fight.h"
#include "files.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "travel.h"
#include "version.h"
#include "view.h"

// ========================================================================
//      Internal Functions
// ========================================================================

//---------------------------------------------------------------
//
// BreakStrToDebugger
//
//---------------------------------------------------------------
#if DEBUG
static void BreakStrToDebugger(const char *mesg)
{

#if OSX || defined(__MINGW32__)
    fprintf(stderr, mesg);
// raise(SIGINT);               // this is what DebugStr() does on OS X according to Tech Note 2030
    int* p = NULL;              // but this gives us a stack crawl...
    *p = 0;

#else
    fprintf(stderr, "%s\n", mesg);
    abort();
#endif
}
#endif

// ========================================================================
//      Global Functions
// ========================================================================

//---------------------------------------------------------------
//
// AssertFailed
//
//---------------------------------------------------------------
#if DEBUG
void AssertFailed(const char *expr, const char *file, int line)
{
    char mesg[512];

    const char *fileName = file + strlen(file); // strip off path

    while (fileName > file && fileName[-1] != '\\')
        --fileName;

    sprintf(mesg, "ASSERT(%s) in '%s' at line %d failed.", expr, fileName,
            line);

    BreakStrToDebugger(mesg);
}
#endif


//---------------------------------------------------------------
//
// DEBUGSTR
//
//---------------------------------------------------------------
#if DEBUG
void DEBUGSTR(const char *format, ...)
{
    char mesg[2048];

    va_list args;

    va_start(args, format);
    vsprintf(mesg, format, args);
    va_end(args);

    BreakStrToDebugger(mesg);
}
#endif

#ifdef WIZARD

static int debug_prompt_for_monster( void )
{
    char  specs[80];

    mpr( "(Hint: 'generated' names, eg 'orc zombie', won't work)", MSGCH_PROMPT );
    mpr( "Which monster by name? ", MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return (-1);

    return (get_monster_by_name(specs));
}
#endif

//---------------------------------------------------------------
//
// debug_prompt_for_skill
//
//---------------------------------------------------------------
#ifdef WIZARD
static int debug_prompt_for_skill( const char *prompt )
{
    char specs[80];

    mpr( prompt, MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return (-1);

    int skill = -1;

    for (int i = 0; i < NUM_SKILLS; i++)
    {
        // avoid the bad values:
        if (i == SK_UNUSED_1 || (i > SK_UNARMED_COMBAT && i < SK_SPELLCASTING))
            continue;

        char sk_name[80];
        strncpy( sk_name, skill_name(i), sizeof( sk_name ) );

        char *ptr = strstr( strlwr(sk_name), strlwr(specs) );
        if (ptr != NULL)
        {
            if (ptr == sk_name && strlen(specs) > 0)
            {
                // we prefer prefixes over partial matches
                skill = i;
                break;
            }
            else
                skill = i;
        }
    }

    return (skill);
}
#endif

//---------------------------------------------------------------
//
// debug_change_species
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_change_species( void ) 
{
    char specs[80];
    int i;

    mpr( "What species would you like to be now? " , MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return;

    int sp = -1;

    for (i = SP_HUMAN; i < NUM_SPECIES; i++)
    {
        char sp_name[80];
        strncpy( sp_name, species_name(i, you.experience_level), sizeof( sp_name ) );

        char *ptr = strstr( strlwr(sp_name), strlwr(specs) );
        if (ptr != NULL)
        {
            if (ptr == sp_name && strlen(specs) > 0)
            {
                // we prefer prefixes over partial matches
                sp = i;
                break;
            }
            else
                sp = i;
        }
    }

    if (sp == -1)
        mpr( "That species isn't available." );
    else
    {
        for (i = 0; i < NUM_SKILLS; i++)
        {
            you.skill_points[i] *= species_skills( i, sp );
            you.skill_points[i] /= species_skills( i, you.species );
        }

        you.species = sp;         
        you.is_undead = ((you.species == SP_MUMMY) ? US_UNDEAD :
                         (you.species == SP_GHOUL) ? US_HUNGRY_DEAD : US_ALIVE);

        redraw_screen();
    }
} 
#endif 
//---------------------------------------------------------------
//
// debug_prompt_for_int 
//
// If nonneg, then it returns a non-negative number or -1 on fail
// If !nonneg, then it returns an integer, and 0 on fail
//
//---------------------------------------------------------------
#ifdef WIZARD
static int debug_prompt_for_int( const char *prompt, bool nonneg )
{
    char specs[80];

    mpr( prompt, MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return (nonneg ? -1 : 0);

    char *end;
    int   ret = strtol( specs, &end, 10 );

    if ((ret < 0 && nonneg) || (ret == 0 && end == specs))
        ret = (nonneg ? -1 : 0);

    return (ret);
}
#endif

/*
   Some debugging functions, accessable through keys like %, $, &, ) etc when
   a section of code in input() in acr.cc is uncommented.
 */

//---------------------------------------------------------------
//
// cast_spec_spell
//
//---------------------------------------------------------------
#ifdef WIZARD
void cast_spec_spell(void)
{
    int spell = debug_prompt_for_int( "Cast which spell by number? ", true );

    if (spell == -1)
        canned_msg( MSG_OK );
    else
        your_spells( spell, 0, false );
}
#endif


//---------------------------------------------------------------
//
// cast_spec_spell_name
//
//---------------------------------------------------------------
#ifdef WIZARD
void cast_spec_spell_name(void)
{
    int i = 0;
    char specs[80];
    char spname[80];

    mpr( "Cast which spell by name? ", MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    for (i = 0; i < NUM_SPELLS; i++)
    {
        strncpy( spname, spell_title(i), sizeof( spname ) );

        if (strstr( strlwr(spname), strlwr(specs) ) != NULL)
        {
            your_spells(i, 0, false);
            return;
        }
    }

    mpr((one_chance_in(20)) ? "Maybe you should go back to WIZARD school."
                            : "I couldn't find that spell.");
}
#endif


//---------------------------------------------------------------
//
// create_spec_monster
//
//---------------------------------------------------------------
#ifdef WIZARD
void create_spec_monster(void)
{
    int mon = debug_prompt_for_int( "Create which monster by number? ", true );
    
    if (mon == -1)
        canned_msg( MSG_OK );
    else
        create_monster( mon, 0, BEH_SLEEP, 
                you.x_pos, you.y_pos, MHITNOT, 250, true );
}                               // end create_spec_monster()
#endif


//---------------------------------------------------------------
//
// create_spec_monster_name
//
//---------------------------------------------------------------
#ifdef WIZARD
void create_spec_monster_name(int x, int y)
{
    int mon = debug_prompt_for_monster();

    if (mon == -1 || mon == MONS_PROGRAM_BUG)
    {
        mpr("I couldn't find that monster.");

        if (one_chance_in(20))
            mpr("Maybe it's hiding.");
    }
    else
    {
        const bool force_place = x != -1 && y != -1;
        if (x == -1)
            x = you.x_pos;
        if (y == -1)
            y = you.y_pos;
        
        create_monster(mon, 0, BEH_SLEEP, x, y,
                       MHITNOT, 250, false, force_place);
    }
}                               // end create_spec_monster_name()
#endif


//---------------------------------------------------------------
//
// level_travel
//
//---------------------------------------------------------------
#ifdef WIZARD
void level_travel( int delta )
{
    int   old_level = you.your_level;
    int   new_level = you.your_level + delta;

    if (delta == 0)
    {
        new_level = debug_prompt_for_int( "Travel to which level? ", true ) - 1;
    }

    if (new_level < 0 || new_level >= 50)
    {
        mpr( "That level is out of bounds." );
        return;
    }

    you.your_level = new_level - 1;
    grd[you.x_pos][you.y_pos] = DNGN_STONE_STAIRS_DOWN_I;
    down_stairs(true, old_level);
    untag_followers();
}                               // end level_travel()

static void wizard_go_to_level(const level_pos &pos)
{
    const int abs_depth = absdungeon_depth(pos.id.branch, pos.id.depth);
    const int stair_taken =
        abs_depth > you.your_level? DNGN_STONE_STAIRS_DOWN_I
                                  : DNGN_STONE_STAIRS_UP_I;

    const int old_level = you.your_level;
    const int old_where = you.where_are_you;
    const bool was_a_labyrinth = you.level_type == LEVEL_LABYRINTH;

    you.level_type    = LEVEL_DUNGEON;
    you.where_are_you = pos.id.branch;
    you.your_level    = abs_depth;

    load(stair_taken, LOAD_ENTER_LEVEL, was_a_labyrinth, old_level, old_where);
    save_game_state();
    new_level();
    viewwindow(1, true);

    // Tell stash-tracker and travel that we've changed levels.
    trackers_init_new_level(true);
}

void wizard_interlevel_travel()
{
    const level_pos pos =
        prompt_translevel_target(TPF_ALLOW_UPDOWN | TPF_SHOW_ALL_BRANCHES);
    
    if (pos.id.depth < 1 || pos.id.depth > branches[pos.id.branch].depth)
    {
        canned_msg(MSG_OK);
        return;
    }

    wizard_go_to_level(pos);
}

#endif

#ifdef WIZARD
//---------------------------------------------------------------
//
// create_spec_object
//
//---------------------------------------------------------------
void create_spec_object()
{
    static int max_subtype[] = 
    {
        NUM_WEAPONS,
        NUM_MISSILES,
        NUM_ARMOURS,
        NUM_WANDS,
        NUM_FOODS,
        0,              // unknown I
        NUM_SCROLLS,
        NUM_JEWELLERY,
        NUM_POTIONS,
        0,              // unknown II
        NUM_BOOKS,
        NUM_STAVES,
        0,              // Orbs         -- only one, handled specially
        NUM_MISCELLANY,
        0,              // corpses      -- handled specially
        0,              // gold         -- handled specially
        0,              // "gemstones"  -- no items of type
    };

    char           specs[80];
    char           obj_name[ ITEMNAME_SIZE ];
    char           keyin;

    char *         ptr;
    int            best_index;
    int            mon;
    int            i;

    int            class_wanted   = OBJ_UNASSIGNED;
    int            type_wanted    = -1;
    int            special_wanted = 0;

    int            thing_created;

    while (class_wanted == OBJ_UNASSIGNED) 
    {
        mpr(") - weapons     ( - missiles  [ - armour  / - wands    ?  - scrolls",
             MSGCH_PROMPT);
        mpr("= - jewellery   ! - potions   : - books   | - staves   0  - The Orb",
             MSGCH_PROMPT);
        mpr("} - miscellany  X - corpses   % - food    $ - gold    ESC - exit", 
             MSGCH_PROMPT);

        mpr("What class of item? ", MSGCH_PROMPT);

        keyin = toupper( get_ch() );

        if (keyin == ')')
            class_wanted = OBJ_WEAPONS;
        else if (keyin == '(')
            class_wanted = OBJ_MISSILES;
        else if (keyin == '[' || keyin == ']')
            class_wanted = OBJ_ARMOUR;
        else if (keyin == '/' || keyin == '\\')
            class_wanted = OBJ_WANDS;
        else if (keyin == '?')
            class_wanted = OBJ_SCROLLS;
        else if (keyin == '=' || keyin == '"')
            class_wanted = OBJ_JEWELLERY;
        else if (keyin == '!')
            class_wanted = OBJ_POTIONS;
        else if (keyin == ':')
            class_wanted = OBJ_BOOKS;
        else if (keyin == '|')
            class_wanted = OBJ_STAVES;
        else if (keyin == '0' || keyin == 'O')
            class_wanted = OBJ_ORBS;
        else if (keyin == '}' || keyin == '{')
            class_wanted = OBJ_MISCELLANY;
        else if (keyin == 'X')
            class_wanted = OBJ_CORPSES;
        else if (keyin == '%')
            class_wanted = OBJ_FOOD;
        else if (keyin == '$')
            class_wanted = OBJ_GOLD;
        else if (keyin == ESCAPE || keyin == ' ' 
                || keyin == '\r' || keyin == '\n')
        {
            canned_msg( MSG_OK );
            return;
        }
    }

    // allocate an item to play with:
    thing_created = get_item_slot();
    if (thing_created == NON_ITEM)
    {
        mpr( "Could not allocate item." );
        return;
    }

    // turn item into appropriate kind:
    if (class_wanted == OBJ_ORBS)
    {
        mitm[thing_created].base_type = OBJ_ORBS;
        mitm[thing_created].sub_type  = ORB_ZOT;
        mitm[thing_created].quantity  = 1;
    }
    else if (class_wanted == OBJ_GOLD)
    {
        int amount = debug_prompt_for_int( "How much gold? ", true );
        if (amount <= 0)
        {
            canned_msg( MSG_OK );
            return;
        }

        mitm[thing_created].base_type = OBJ_GOLD;
        mitm[thing_created].sub_type = 0;
        mitm[thing_created].quantity = amount;
    }
    else if (class_wanted == OBJ_CORPSES)
    {
        mon = debug_prompt_for_monster();

        if (mon == -1 || mon == MONS_PROGRAM_BUG)
        {
            mpr( "No such monster." );
            return;
        }

        mitm[thing_created].base_type = OBJ_CORPSES;
        mitm[thing_created].sub_type  = CORPSE_BODY;
        mitm[thing_created].plus      = mon;
        mitm[thing_created].plus2     = 0;
        mitm[thing_created].special   = 210;
        mitm[thing_created].colour    = mons_class_colour(mon);;
        mitm[thing_created].quantity  = 1;
        mitm[thing_created].flags     = 0;
    }
    else
    {
        mpr( "What type of item? ", MSGCH_PROMPT );
        get_input_line( specs, sizeof( specs ) );

        if (specs[0] == '\0')
        {
            canned_msg( MSG_OK );
            return;
        }

        // In order to get the sub-type, we'll fill out the base type... 
        // then we're going to iterate over all possible subtype values 
        // and see if we get a winner. -- bwr
        mitm[thing_created].base_type = class_wanted;
        mitm[thing_created].sub_type  = 0;
        mitm[thing_created].plus      = 0;
        mitm[thing_created].plus2     = 0;
        mitm[thing_created].special   = 0;
        mitm[thing_created].flags     = 0;
        mitm[thing_created].quantity  = 1;
        set_ident_flags( mitm[thing_created], ISFLAG_IDENT_MASK );

        if (class_wanted == OBJ_ARMOUR) 
        {
            if (strstr( "naga barding", specs ))
            {
                mitm[thing_created].sub_type = ARM_NAGA_BARDING;
            }
            else if (strstr( "centaur barding", specs ))
            {
                mitm[thing_created].sub_type = ARM_CENTAUR_BARDING;
            }
            else if (strstr( "wizard's hat", specs ))
            {
                mitm[thing_created].sub_type = ARM_HELMET;
                mitm[thing_created].plus2 = THELM_WIZARD_HAT;
            }
            else if (strstr( "cap", specs ))
            {
                mitm[thing_created].sub_type = ARM_HELMET;
                mitm[thing_created].plus2 = THELM_CAP;
            }
            else if (strstr( "helm", specs ))
            {
                mitm[thing_created].sub_type = ARM_HELMET;
                mitm[thing_created].plus2 = THELM_HELM;
            }
        }

        if (!mitm[thing_created].sub_type)
        {
            type_wanted = -1;
            best_index  = 10000;

            for (i = 0; i < max_subtype[ mitm[thing_created].base_type ]; i++)
            {
                mitm[thing_created].sub_type = i;     
                item_name( mitm[thing_created], DESC_PLAIN, obj_name );

                ptr = strstr( strlwr(obj_name), strlwr(specs) );
                if (ptr != NULL)
                {
                    // earliest match is the winner
                    if (ptr - obj_name < best_index)
                    {
                        mpr( obj_name );
                        type_wanted = i;    
                        best_index = ptr - obj_name;
                    }
                }
            }

            if (type_wanted == -1)
            {
                // ds -- if specs is a valid int, try using that.
                //       Since zero is atoi's copout, the wizard
                //       must enter (subtype + 1).
                if (!(type_wanted = atoi(specs)))
                {
                    mpr( "No such item." );
                    return;
                }
                type_wanted--;
            }
            
            mitm[thing_created].sub_type = type_wanted;
        }
        
        switch (mitm[thing_created].base_type)
        {
        case OBJ_MISSILES:
            mitm[thing_created].quantity = 30;
            // intentional fall-through
        case OBJ_WEAPONS:
        case OBJ_ARMOUR:
            mpr( "What ego type? ", MSGCH_PROMPT );
            get_input_line( specs, sizeof( specs ) );
            
            if (specs[0] != '\0')
            {
                special_wanted = 0;
                best_index = 10000;

                for (i = 1; i < 25; i++)
                {
                    mitm[thing_created].special = i;
                    item_name( mitm[thing_created], DESC_PLAIN, obj_name );

                    ptr = strstr( strlwr(obj_name), strlwr(specs) );
                    if (ptr != NULL)
                    {
                        // earliest match is the winner
                        if (ptr - obj_name < best_index)
                        {
                            mpr( obj_name );
                            special_wanted = i;    
                            best_index = ptr - obj_name;
                        }
                    }
                }

                mitm[thing_created].special = special_wanted;
            }
            break;

        case OBJ_BOOKS:
            if (mitm[thing_created].sub_type == BOOK_MANUAL)
            {
                special_wanted = debug_prompt_for_skill( "A manual for which skill? " );
                if (special_wanted != -1)
                    mitm[thing_created].plus = special_wanted;
                else
                    mpr( "Sorry, no books on that skill today." );
            }
            break;
        
        case OBJ_WANDS:
            mitm[thing_created].plus = 24;
            break;

        case OBJ_STAVES:
            if (item_is_rod( mitm[thing_created] ))
            {
                mitm[thing_created].plus = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
                mitm[thing_created].plus2 = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
            }
            break;

        case OBJ_MISCELLANY:
            // Runes to "demonic", decks have 50 cards, ignored elsewhere?
            mitm[thing_created].plus = 50;
            break;

        case OBJ_FOOD:
        case OBJ_SCROLLS:
        case OBJ_POTIONS:
            mitm[thing_created].quantity = 12;
            break;

        default:
            break;
        }
    }

    item_colour( mitm[thing_created] );

    move_item_to_grid( &thing_created, you.x_pos, you.y_pos );

    if (thing_created != NON_ITEM)
    {
        origin_acquired( mitm[thing_created], AQ_WIZMODE );
        canned_msg( MSG_SOMETHING_APPEARS );
    }
}
#endif


//---------------------------------------------------------------
//
// create_spec_object
//
//---------------------------------------------------------------
#ifdef WIZARD
void tweak_object(void)
{
    char specs[50];
    char keyin;

    int item = prompt_invent_item("Tweak which item? ", MT_INVSELECT, -1);
    if (item == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (item == you.equip[EQ_WEAPON])
        you.wield_change = true;

    for (;;)
    {
        void *field_ptr = NULL;

        for (;;) 
        {
            item_name( you.inv[item], DESC_INVENTORY_EQUIP, info );
            mpr( info );

            mpr( "a - plus  b - plus2  c - special  d - quantity  e - flags  ESC - exit",
                 MSGCH_PROMPT );
            mpr( "Which field? ", MSGCH_PROMPT );

            keyin = tolower( get_ch() );

            if (keyin == 'a')
                field_ptr = &(you.inv[item].plus);
            else if (keyin == 'b')
                field_ptr = &(you.inv[item].plus2);
            else if (keyin == 'c')
                field_ptr = &(you.inv[item].special);
            else if (keyin == 'd')
                field_ptr = &(you.inv[item].quantity);
            else if (keyin == 'e')
                field_ptr = &(you.inv[item].flags);
            else if (keyin == ESCAPE || keyin == ' ' 
                    || keyin == '\r' || keyin == '\n')
            {
                canned_msg( MSG_OK );
                return;
            }

            if (keyin >= 'a' && keyin <= 'e')
                break;
        }

        if (keyin != 'c' && keyin != 'e')
        {
            const short *const ptr = static_cast< short * >( field_ptr );
            snprintf( info, INFO_SIZE, "Old value: %d (0x%04x)", *ptr, *ptr );
        }
        else 
        {
            const long *const ptr = static_cast< long * >( field_ptr );
            snprintf( info, INFO_SIZE, "Old value: %ld (0x%08lx)", *ptr, *ptr );
        }

        mpr( info );

        mpr( "New value? ", MSGCH_PROMPT );
        get_input_line( specs, sizeof( specs ) );
        
        if (specs[0] == '\0')
            return;

        char *end;
        int   new_value = strtol( specs, &end, 10 );

        if (new_value == 0 && end == specs)
            return;

        if (keyin != 'c' && keyin != 'e')
        {
            short *ptr = static_cast< short * >( field_ptr );
            *ptr = new_value;
        }
        else
        {
            long *ptr = static_cast< long * >( field_ptr );
            *ptr = new_value;
        }
    }
}
#endif


//---------------------------------------------------------------
//
// stethoscope
//
//---------------------------------------------------------------
#if DEBUG_DIAGNOSTICS

void stethoscope(int mwh)
{
    struct dist stth;
    int steth_x, steth_y;
    int i;

    if (mwh != RANDOM_MONSTER)
        i = mwh;
    else
    {
        mpr( "Which monster?", MSGCH_PROMPT );

        direction( stth );

        if (!stth.isValid)
            return;

        if (stth.isTarget)
        {
            steth_x = stth.tx;
            steth_y = stth.ty;
        }
        else
        {
            steth_x = you.x_pos + stth.dx;
            steth_y = you.x_pos + stth.dy;
        }

        if (env.cgrid[steth_x][steth_y] != EMPTY_CLOUD)
        {
            snprintf( info, INFO_SIZE, "cloud type: %d delay: %d", 
                     env.cloud[ env.cgrid[steth_x][steth_y] ].type,
                     env.cloud[ env.cgrid[steth_x][steth_y] ].decay );

            mpr( info, MSGCH_DIAGNOSTICS );
        }

        if (mgrd[steth_x][steth_y] == NON_MONSTER)
        {
            snprintf( info, INFO_SIZE, "item grid = %d", igrd[steth_x][steth_y] );
            mpr( info, MSGCH_DIAGNOSTICS );
            return;
        }

        i = mgrd[steth_x][steth_y];
    }

    // print type of monster
    snprintf( info, INFO_SIZE, "%s (id #%d; type=%d loc=(%d,%d) align=%s)",
              monam( &menv[i], menv[i].number, menv[i].type, true,
                     DESC_CAP_THE ),
              i, menv[i].type,
              menv[i].x, menv[i].y,
              ((menv[i].attitude == ATT_FRIENDLY) ? "friendly" :
               (menv[i].attitude == ATT_HOSTILE)  ? "hostile" :
               (menv[i].attitude == ATT_NEUTRAL)  ? "neutral" 
                                                  : "unknown alignment") );

    mpr( info, MSGCH_DIAGNOSTICS );

    // print stats and other info
    snprintf( info, INFO_SIZE,"HD=%d HP=%d/%d AC=%d EV=%d MR=%d SP=%d energy=%d num=%d flags=%04lx",
             menv[i].hit_dice, 
             menv[i].hit_points, menv[i].max_hit_points, 
             menv[i].ac, menv[i].ev,
             mons_resist_magic( &menv[i] ),
             menv[i].speed, menv[i].speed_increment,
             menv[i].number, menv[i].flags );

    mpr( info, MSGCH_DIAGNOSTICS );

    // print behaviour information
    
    const int hab = monster_habitat( menv[i].type );

    snprintf( info, INFO_SIZE, "hab=%s beh=%s(%d) foe=%s(%d) mem=%d target=(%d,%d)", 
             ((hab == DNGN_DEEP_WATER)            ? "water" :
              (hab == DNGN_LAVA)                  ? "lava" 
                                                  : "floor"),

             ((menv[i].behaviour == BEH_SLEEP)    ? "sleep" :
              (menv[i].behaviour == BEH_WANDER)   ? "wander" :
              (menv[i].behaviour == BEH_SEEK)     ? "seek" :
              (menv[i].behaviour == BEH_FLEE)     ? "flee" :
              (menv[i].behaviour == BEH_CORNERED) ? "cornered" 
                                                  : "unknown"), 
             menv[i].behaviour,

             ((menv[i].foe == MHITYOU)            ? "you" :
              (menv[i].foe == MHITNOT)            ? "none" :
              (menv[menv[i].foe].type == -1)      ? "unassigned monster" 
              : monam( &menv[menv[i].foe],
                       menv[menv[i].foe].number, menv[menv[i].foe].type,
                       true, DESC_PLAIN )),
             menv[i].foe, 
             menv[i].foe_memory, 

             menv[i].target_x, menv[i].target_y );

    mpr( info, MSGCH_DIAGNOSTICS );

    // print resistances
    snprintf( info, INFO_SIZE, "resist: fire=%d cold=%d elec=%d pois=%d neg=%d",
              mons_res_fire( &menv[i] ),
              mons_res_cold( &menv[i] ),
              mons_res_elec( &menv[i] ),
              mons_res_poison( &menv[i] ),
              mons_res_negative_energy( &menv[i] ) );

    mpr( info, MSGCH_DIAGNOSTICS );

    mprf(MSGCH_DIAGNOSTICS, "ench: %s",
         comma_separated_line(menv[i].enchantments.begin(),
                              menv[i].enchantments.end(),
                              ", ").c_str());

    if (menv[i].type == MONS_PLAYER_GHOST 
        || menv[i].type == MONS_PANDEMONIUM_DEMON)
    {
        ASSERT(menv[i].ghost.get());
        const ghost_demon &ghost = *menv[i].ghost;
        mprf( MSGCH_DIAGNOSTICS,
              "Ghost damage: %d; brand: %d", 
              ghost.values[ GVAL_DAMAGE ], ghost.values[ GVAL_BRAND ] );
    }
}                               // end stethoscope()
#endif

#if DEBUG_ITEM_SCAN
//---------------------------------------------------------------
//
// dump_item
//
//---------------------------------------------------------------
static void dump_item( const char *name, int num, const item_def &item )
{
    mpr( name, MSGCH_WARN );

    snprintf( info, INFO_SIZE, "    item #%d:  base: %d; sub: %d; plus: %d; plus2: %d; special: %ld",
             num, item.base_type, item.sub_type, 
             item.plus, item.plus2, item.special );

    mpr( info );

    snprintf( info, INFO_SIZE, "    quant: %d; colour: %d; ident: 0x%08lx; ident_type: %d",
             item.quantity, item.colour, item.flags,
             get_ident_type( item.base_type, item.sub_type ) );

    mpr( info );

    snprintf( info, INFO_SIZE, "    x: %d; y: %d; link: %d",
             item.x, item.y, item.link );

    mpr( info );
}

//---------------------------------------------------------------
//
// debug_item_scan
//
//---------------------------------------------------------------
void debug_item_scan( void )
{
    int   i;
    char  name[256];

    // unset marks
    for (i = 0; i < MAX_ITEMS; i++)
        mitm[i].flags &= (~ISFLAG_DEBUG_MARK);

    // First we're going to check all the stacks on the level:
    for (int x = 0; x < GXM; x++)
    {
        for (int y = 0; y < GYM; y++)
        {
            // These are unlinked monster inventory items -- skip them:
            if (x == 0 && y == 0)
                continue;

            // Looking for infinite stacks (ie more links than tems allowed)
            // and for items which have bad coordinates (can't find their stack)
            for (int obj = igrd[x][y]; obj != NON_ITEM; obj = mitm[obj].link)
            {
                // Check for invalid (zero quantity) items that are linked in
                if (!is_valid_item( mitm[obj] ))
                {
                    snprintf( info, INFO_SIZE, "Linked invalid item at (%d,%d)!", x, y);
                    mpr( info, MSGCH_WARN );
                    item_name( mitm[obj], DESC_PLAIN, name );
                    dump_item( name, obj, mitm[obj] );
                }

                // Check that item knows what stack it's in
                if (mitm[obj].x != x || mitm[obj].y != y)
                {
                    snprintf( info, INFO_SIZE, "Item position incorrect at (%d,%d)!", x, y);
                    mpr( info, MSGCH_WARN );
                    item_name( mitm[obj], DESC_PLAIN, name );
                    dump_item( name, obj, mitm[obj] );
                }

                // If we run into a premarked item we're in real trouble,
                // this will also keep this from being an infinite loop.
                if (mitm[obj].flags & ISFLAG_DEBUG_MARK)
                {
                    snprintf( info, INFO_SIZE, "Potential INFINITE STACK at (%d, %d)", x, y);
                    mpr( info, MSGCH_WARN );
                    break;
                }

                mitm[obj].flags |= ISFLAG_DEBUG_MARK;
            }
        }
    }

    // Now scan all the items on the level:
    for (i = 0; i < MAX_ITEMS; i++)
    {
        if (!is_valid_item( mitm[i] ))
            continue;

        item_name( mitm[i], DESC_PLAIN, name );

        // Don't check (-1,-1) player items or (0,0) monster items
        if ((mitm[i].x > 0 || mitm[i].y > 0) 
            && !(mitm[i].flags & ISFLAG_DEBUG_MARK))
        {
            mpr( "Unlinked item:", MSGCH_WARN );
            dump_item( name, i, mitm[i] );
            
            snprintf( info, INFO_SIZE, "igrd(%d,%d) = %d", mitm[i].x, mitm[i].y, 
                     igrd[ mitm[i].x ][ mitm[i].y ] );
            mpr( info );

            // Let's check to see if it's an errant monster object:
            for (int j = 0; j < MAX_MONSTERS; j++)
            {
                for (int k = 0; k < NUM_MONSTER_SLOTS; k++)
                {
                    if (menv[j].inv[k] == i)
                    {
                        snprintf( info, INFO_SIZE, "Held by monster #%d: %s at (%d,%d)", 
                                 j, ptr_monam( &menv[j], DESC_CAP_A ),
                                 menv[j].x, menv[j].y );

                        mpr( info );
                    }
                }
            }
        }

        // Current bad items of interest:
        //   -- armour and weapons with large enchantments/illegal special vals
        //
        //   -- items described as questionable (the class 100 bug)
        //
        //   -- eggplant is an illegal throwing weapon
        //
        //   -- bola is an illegal fixed artefact
        //
        //   -- items described as buggy (typically adjectives out of range)
        //      (note: covers buggy, bugginess, buggily, whatever else)
        //
        if (strstr( name, "questionable" ) != NULL
            || strstr( name, "eggplant" ) != NULL
            || strstr( name, "bola" ) != NULL
            || strstr( name, "bugg" ) != NULL)
        {
            mpr( "Bad item:", MSGCH_WARN );
            dump_item( name, i, mitm[i] );
        }
        else if ((mitm[i].base_type == OBJ_WEAPONS 
                && (abs(mitm[i].plus) > 30 
                    || abs(mitm[i].plus2) > 30
                    || (!is_random_artefact( mitm[i] )
                        && (mitm[i].special >= 30 
                            && mitm[i].special < 181))))

            || (mitm[i].base_type == OBJ_MISSILES 
                && (abs(mitm[i].plus) > 25 
                    || (!is_random_artefact( mitm[i] ) 
                        && mitm[i].special >= 30)))

            || (mitm[i].base_type == OBJ_ARMOUR
                && (abs(mitm[i].plus) > 25 
                    || (!is_random_artefact( mitm[i] )
                        && mitm[i].sub_type != ARM_HELMET 
                        && mitm[i].special >= 30))))
        {
            mpr( "Bad plus or special value:", MSGCH_WARN );
            dump_item( name, i, mitm[i] );
        }
    }

    // Don't want debugging marks interfering with anything else.
    for (i = 0; i < MAX_ITEMS; i++)
        mitm[i].flags &= (~ISFLAG_DEBUG_MARK);

    // Quickly scan monsters for "program bug"s.
    for (i = 0; i < MAX_MONSTERS; i++)
    {
        const struct monsters *const monster = &menv[i];

        if (monster->type == -1)
            continue;

        moname( monster->type, true, DESC_PLAIN, name );

        if (strcmp( name, "program bug" ) == 0)
        {
            mpr( "Program bug detected!", MSGCH_WARN );

            snprintf( info, INFO_SIZE,
                      "Buggy monster detected: monster #%d; position (%d,%d)",
                      i, monster->x, monster->y );

            mpr( info, MSGCH_WARN );
        }   
    }
}
#endif

//---------------------------------------------------------------
//
// debug_add_skills
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_add_skills(void)
{
    int skill = debug_prompt_for_skill( "Which skill (by name)? " );

    if (skill == -1)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr("Exercising...");
        exercise(skill, 100);
    }
}                               // end debug_add_skills()
#endif

//---------------------------------------------------------------
//
// debug_set_skills
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_set_skills(void)
{
    int skill = debug_prompt_for_skill( "Which skill (by name)? " );

    if (skill == -1)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr( skill_name(skill) );
        int amount = debug_prompt_for_int( "To what level? ", true );

        if (amount == -1)
            canned_msg( MSG_OK );
        else
        {
            const int points = (skill_exp_needed( amount + 1 ) 
                                * species_skills( skill, you.species )) / 100;

            you.skill_points[skill] = points + 1;
            you.skills[skill] = amount;

            calc_total_skill_points();

            redraw_skill( you.your_name, player_title() );

            switch (skill)
            {
            case SK_FIGHTING:
                calc_hp();
                break;

            case SK_SPELLCASTING:
            case SK_INVOCATIONS:
            case SK_EVOCATIONS:
                calc_mp();
                break;

            case SK_DODGING:
                you.redraw_evasion = 1;
                break;

            case SK_ARMOUR:
                you.redraw_armour_class = 1;
                you.redraw_evasion = 1;
                break;

            default:
                break;
            }
        }
    }
}                               // end debug_add_skills()
#endif


//---------------------------------------------------------------
//
// debug_set_all_skills
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_set_all_skills(void)
{
    int i;
    int amount = debug_prompt_for_int( "Set all skills to what level? ", true );

    if (amount < 0)             // cancel returns -1 -- bwr
        canned_msg( MSG_OK );
    else
    {
        if (amount > 27)
            amount = 27;

        for (i = SK_FIGHTING; i < NUM_SKILLS; i++)
        {
            if (i == SK_UNUSED_1 
                || (i > SK_UNARMED_COMBAT && i < SK_SPELLCASTING))
            {
                continue;
            }

            const int points = (skill_exp_needed( amount + 1 ) 
                                * species_skills( i, you.species )) / 100;

            you.skill_points[i] = points + 1;
            you.skills[i] = amount;
        }

        redraw_skill( you.your_name, player_title() );

        calc_total_skill_points();

        calc_hp();
        calc_mp();

        you.redraw_armour_class = 1;
        you.redraw_evasion = 1;
    }
}                               // end debug_add_skills()
#endif


//---------------------------------------------------------------
//
// debug_add_mutation
//
//---------------------------------------------------------------
#ifdef WIZARD
bool debug_add_mutation(void)
{
    bool success = false;
    char specs[80];

    // Yeah, the gaining message isn't too good for this... but
    // there isn't an array of simple mutation names. -- bwr
    mpr( "Which mutation (by message when getting mutation)? ", MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return (false);

    int mutation = -1;

    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        char mut_name[80];
        strncpy( mut_name, mutation_name( i, 1 ), sizeof( mut_name ) );

        char *ptr = strstr( strlwr(mut_name), strlwr(specs) );
        if (ptr != NULL)
        {
            // we take the first mutation that matches
            mutation = i;
            break;
        }
    }

    if (mutation == -1)
        mpr("I can't warp you that way!");
    else
    {
        snprintf( info, INFO_SIZE, "Found: %s", mutation_name( mutation, 1 ) );
        mpr( info );

        int levels = debug_prompt_for_int( "How many levels? ", false );

        if (levels == 0)
        {
            canned_msg( MSG_OK );
            success = false;
        }
        else if (levels > 0)
        {
            for (int i = 0; i < levels; i++)
            {
                if (mutate( mutation ))
                    success = true;
            }
        }
        else 
        {
            for (int i = 0; i < -levels; i++)
            {
                if (delete_mutation( mutation ))
                    success = true;
            }
        }
    }

    return (success);
}                               // end debug_add_mutation()
#endif


//---------------------------------------------------------------
//
// debug_get_religion
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_get_religion(void)
{
    char specs[80];

    mpr( "Which god (by name)? ", MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return;

    int god = -1;

    for (int i = 1; i < NUM_GODS; i++)
    {
        char name[80];
        strncpy( name, god_name(i), sizeof( name ) );

        char *ptr = strstr( strlwr(name), strlwr(specs) );
        if (ptr != NULL)
        {
            god = i;
            break;
        }
    }

    if (god == -1)
        mpr( "That god doesn't seem to be taking followers today." );
    else
    {
        grd[you.x_pos][you.y_pos] = 179 + god;
        god_pitch(god);
    }
}                               // end debug_add_skills()
#endif


void error_message_to_player(void)
{
    mpr("Oh dear. There appears to be a bug in the program.");
    mpr("I suggest you leave this level then save as soon as possible.");

}                               // end error_message_to_player()

#ifdef WIZARD

static int create_fsim_monster(int mtype, int hp)
{
    const int mi = 
        create_monster( mtype, 0, BEH_HOSTILE, you.x_pos, you.y_pos, 
                        MHITNOT, 250 );

    if (mi == -1)
        return (mi);

    monsters *mon = &menv[mi];
    mon->hit_points = mon->max_hit_points = hp;
    return (mi);
}

static skill_type fsim_melee_skill(const item_def *item)
{
    skill_type sk = SK_UNARMED_COMBAT;
    if (item)
        sk = weapon_skill(*item);
    return (sk);
}

static void fsim_set_melee_skill(int skill, const item_def *item)
{
    you.skills[fsim_melee_skill(item)] = skill;
    you.skills[SK_FIGHTING]           = skill * 15 / 27;
}

static void fsim_set_ranged_skill(int skill, const item_def *item)
{
    you.skills[range_skill(*item)] = skill;
    you.skills[SK_RANGED_COMBAT]   = skill * 15 / 27;
}

static void fsim_item(FILE *out, 
                      bool melee,
                      const item_def *weap,
                      const char *wskill,
                      unsigned long damage,
                      long iterations, long hits,
                      int maxdam, unsigned long time)
{
    double hitdam = hits? double(damage) / hits : 0.0;
    int avspeed = (int) (time / iterations);
    fprintf(out,
            " %-5s|  %3ld%%    |  %5.2f |    %5.2f  |"
            "   %5.2f |   %3d   |   %2ld\n",
            wskill,
            100 * hits / iterations,
            double(damage) / iterations,
            hitdam,
            double(damage) * player_speed() / avspeed / iterations,
            maxdam,
            time / iterations);
}

static void fsim_defence_item(FILE *out, long cum, int hits, int max,
                              int speed, long iters)
{
    // AC | EV | Arm | Dod | Acc | Av.Dam | Av.HitDam | Eff.Dam | Max.Dam | Av.Time
    fprintf(out, "%2d   %2d    %2d   %2d   %3ld%%   %5.2f      %5.2f      %5.2f      %3d"
            "       %2d\n",
            player_AC(),
            player_evasion(),
            you.skills[SK_DODGING],
            you.skills[SK_ARMOUR],
            100 * hits / iters,
            double(cum) / iters,
            hits? double(cum) / hits : 0.0,
            double(cum) / iters * speed / 10,
            max,
            100 / speed);
}
                              

static bool fsim_ranged_combat(FILE *out, int wskill, int mi, 
                             const item_def *item, int missile_slot)
{
    monsters &mon = menv[mi];
    unsigned long cumulative_damage = 0L;
    unsigned long time_taken = 0L;
    long hits = 0L;
    int maxdam = 0;

    const int thrown = missile_slot == -1? get_fire_item_index() : missile_slot;
    if (thrown == ENDOFPACK || thrown == -1)
    {
        mprf("No suitable missiles for combat simulation.");
        return (false);
    }

    fsim_set_ranged_skill(wskill, item);

    no_messages mx;
    const long iter_limit = Options.fsim_rounds;
    const int hunger = you.hunger;
    for (long i = 0; i < iter_limit; ++i)
    {
        mon.hit_points = mon.max_hit_points;
        bolt beam;
        you.time_taken = player_speed();
        if (throw_it(beam, thrown, &mon))
            hits++;
        you.hunger = hunger;
        time_taken += you.time_taken;

        int damage = (mon.max_hit_points - mon.hit_points);
        cumulative_damage += damage;
        if (damage > maxdam)
            maxdam = damage;
    }
    fsim_item(out, false, item, make_stringf("%2d", wskill).c_str(),
              cumulative_damage, iter_limit, hits, maxdam, time_taken);

    return (true);
}

static bool fsim_mon_melee(FILE *out, int dodge, int armour, int mi)
{
    you.skills[SK_DODGING] = dodge;
    you.skills[SK_ARMOUR]  = armour;

    const int yhp  = you.hp;
    const int ymhp = you.hp_max;
    unsigned long cumulative_damage = 0L;
    long hits = 0L;
    int maxdam = 0;
    no_messages mx;
        
    for (long i = 0; i < Options.fsim_rounds; ++i)
    {
        you.hp = you.hp_max = 5000;
        monster_attack(mi);
        const int damage = you.hp_max - you.hp;
        if (damage)
            hits++;
        cumulative_damage += damage;
        if (damage > maxdam)
            maxdam = damage;
    }

    you.hp = yhp;
    you.hp_max = ymhp;

    fsim_defence_item(out, cumulative_damage, hits, maxdam, menv[mi].speed,
                      Options.fsim_rounds);
    return (true);
}

static bool fsim_melee_combat(FILE *out, int wskill, int mi, 
                             const item_def *item)
{
    monsters &mon = menv[mi];
    unsigned long cumulative_damage = 0L;
    unsigned long time_taken = 0L;
    long hits = 0L;
    int maxdam = 0;

    fsim_set_melee_skill(wskill, item);

    no_messages mx;
    const long iter_limit = Options.fsim_rounds;
    const int hunger = you.hunger;
    for (long i = 0; i < iter_limit; ++i)
    {
        mon.hit_points = mon.max_hit_points;
        you.time_taken = player_speed();
        if (you_attack(mi, true))
            hits++;

        you.hunger = hunger;
        time_taken += you.time_taken;

        int damage = (mon.max_hit_points - mon.hit_points);
        cumulative_damage += damage;
        if (damage > maxdam)
            maxdam = damage;
    }
    fsim_item(out, true, item, make_stringf("%2d", wskill).c_str(),
              cumulative_damage, iter_limit, hits, maxdam, time_taken);

    return (true);
}

static bool debug_fight_simulate(FILE *out, int wskill, int mi, int miss_slot)
{
    int weapon = you.equip[EQ_WEAPON];
    const item_def *iweap = weapon != -1? &you.inv[weapon] : NULL;

    if (iweap && iweap->base_type == OBJ_WEAPONS 
            && is_range_weapon(*iweap))
        return fsim_ranged_combat(out, wskill, mi, iweap, miss_slot);
    else
        return fsim_melee_combat(out, wskill, mi, iweap);
}

static const item_def *fsim_weap_item()
{
    const int weap = you.equip[EQ_WEAPON];
    if (weap == -1)
        return NULL;

    return &you.inv[weap];
}

static std::string fsim_wskill()
{
    const item_def *iweap = fsim_weap_item();
    return iweap && iweap->base_type == OBJ_WEAPONS 
             && is_range_weapon(*iweap)?
                        skill_name( range_skill(*iweap) ) :
            iweap?      skill_name( fsim_melee_skill(iweap) ) :
                        skill_name( SK_UNARMED_COMBAT );
}

static std::string fsim_weapon(int missile_slot)
{
    char item_buf[ITEMNAME_SIZE];
    if (you.equip[EQ_WEAPON] != -1)
    {
        const item_def &weapon = you.inv[ you.equip[EQ_WEAPON] ];
        item_name(weapon, DESC_PLAIN, item_buf, true);

        if (is_range_weapon(weapon))
        {
            const int missile = 
                missile_slot == -1? get_fire_item_index() :
                                    missile_slot;
            if (missile < ENDOFPACK)
            {
                std::string base = item_buf;
                base += " with ";
                in_name(missile, DESC_PLAIN, item_buf, true);
                return (base + item_buf);
            }
        }
    }
    else
    {
        strncpy(item_buf, "unarmed", sizeof item_buf);
    }
    return (item_buf);
}

static std::string fsim_time_string()
{
    time_t curr_time = time(NULL);
    struct tm *ltime = localtime(&curr_time);
    if (ltime)
    {
        char buf[100];
        snprintf(buf, sizeof buf, "%4d%02d%02d/%2d:%02d:%02d",
                ltime->tm_year + 1900,
                ltime->tm_mon  + 1,
                ltime->tm_mday,
                ltime->tm_hour,
                ltime->tm_min,
                ltime->tm_sec);
        return (buf);
    }
    return ("");
}

static void fsim_mon_stats(FILE *o, const monsters &mon)
{
    char buf[ITEMNAME_SIZE];
    fprintf(o, "Monster   : %s\n",
            moname(mon.type, true, DESC_PLAIN, buf));
    fprintf(o, "HD        : %d\n", mon.hit_dice);
    fprintf(o, "AC        : %d\n", mon.ac);
    fprintf(o, "EV        : %d\n", mon.ev);
}

static void fsim_title(FILE *o, int mon, int ms)
{
    char buf[ITEMNAME_SIZE];
    fprintf(o, CRAWL " version " VERSION "\n\n");
    fprintf(o, "Combat simulation: %s %s vs. %s (%ld rounds) (%s)\n",
            species_name(you.species, you.experience_level),
            you.class_name,
            moname(menv[mon].type, true, DESC_PLAIN, buf),
            Options.fsim_rounds,
            fsim_time_string().c_str());
    fprintf(o, "Experience: %d\n", you.experience_level);
    fprintf(o, "Strength  : %d\n", you.strength);
    fprintf(o, "Intel.    : %d\n", you.intel);
    fprintf(o, "Dexterity : %d\n", you.dex);
    fprintf(o, "Base speed: %d\n", player_speed());
    fprintf(o, "\n");
    fsim_mon_stats(o, menv[mon]);
    fprintf(o, "\n");
    fprintf(o, "Weapon    : %s\n", fsim_weapon(ms).c_str());
    fprintf(o, "Skill     : %s\n", fsim_wskill().c_str());
    fprintf(o, "\n");
    fprintf(o, "Skill | Accuracy | Av.Dam | Av.HitDam | Eff.Dam | Max.Dam | Av.Time\n");
}

static void fsim_defence_title(FILE *o, int mon)
{
    fprintf(o, CRAWL " version " VERSION "\n\n");
    fprintf(o, "Combat simulation: %s vs. %s %s (%ld rounds) (%s)\n",
            menv[mon].name(DESC_PLAIN).c_str(),
            species_name(you.species, you.experience_level),
            you.class_name,
            Options.fsim_rounds,
            fsim_time_string().c_str());
    fprintf(o, "Experience: %d\n", you.experience_level);
    fprintf(o, "Strength  : %d\n", you.strength);
    fprintf(o, "Intel.    : %d\n", you.intel);
    fprintf(o, "Dexterity : %d\n", you.dex);
    fprintf(o, "Base speed: %d\n", player_speed());
    fprintf(o, "\n");
    fsim_mon_stats(o, menv[mon]);
    fprintf(o, "\n");
    fprintf(o, "AC | EV | Dod | Arm | Acc | Av.Dam | Av.HitDam | Eff.Dam | Max.Dam | Av.Time\n");
}

static int cap_stat(int stat)
{
    return (stat <  1 ? 1  :
            stat > 127 ? 127 :
                        stat);
}

static bool fsim_mon_hit_you(FILE *ostat, int mindex, int)
{
    fsim_defence_title(ostat, mindex);

    for (int sk = 0; sk <= 27; ++sk)
    {
        mesclr();
        mprf("Calculating average damage for %s at dodging %d",
             menv[mindex].name(DESC_PLAIN).c_str(),
             sk);

        if (!fsim_mon_melee(ostat, sk, 0, mindex))
            return (false);

        fflush(ostat);
        // Not checking in the combat loop itself; that would be more responsive
        // for the user, but slow down the sim with all the calls to kbhit().
        if (kbhit() && getch() == 27)
        {
            mprf("Canceling simulation\n");
            return (false);
        }
    }

    for (int sk = 0; sk <= 27; ++sk)
    {
        mesclr();
        mprf("Calculating average damage for %s at armour %d",
             menv[mindex].name(DESC_PLAIN).c_str(),
             sk);

        if (!fsim_mon_melee(ostat, 0, sk, mindex))
            return (false);

        fflush(ostat);
        // Not checking in the combat loop itself; that would be more responsive
        // for the user, but slow down the sim with all the calls to kbhit().
        if (kbhit() && getch() == 27)
        {
            mprf("Canceling simulation\n");
            return (false);
        }
    }

    mprf("Done defence simulation with %s",
         menv[mindex].name(DESC_PLAIN).c_str());
    
    return (true);
}

static bool fsim_you_hit_mon(FILE *ostat, int mindex, int missile_slot)
{
    fsim_title(ostat, mindex, missile_slot);
    for (int wskill = 0; wskill <= 27; ++wskill)
    {
        mesclr();
        mprf("Calculating average damage for %s at skill %d",
                fsim_weapon(missile_slot).c_str(), wskill);
        if (!debug_fight_simulate(ostat, wskill, mindex, missile_slot))
            return (false);
        
        fflush(ostat);
        // Not checking in the combat loop itself; that would be more responsive
        // for the user, but slow down the sim with all the calls to kbhit().
        if (kbhit() && getch() == 27)
        {
            mprf("Canceling simulation\n");
            return (false);
        }
    }
    mprf("Done fight simulation with %s", fsim_weapon(missile_slot).c_str());
    return (true);
}

static bool debug_fight_sim(int mindex, int missile_slot,
                            bool (*combat)(FILE *, int mind, int mslot))
{
    FILE *ostat = fopen("fight.stat", "a");
    if (!ostat)
    {
        // I'm not sure what header provides errno on djgpp,
        // and it's insufficiently important for a wizmode-only
        // feature.
#ifndef DOS
        mprf("Can't write fight.stat: %s", strerror(errno));
#endif
        return (false);
    }

    bool success = true;
    FixedVector<unsigned char, 50> skill_backup = you.skills;
    int ystr = you.strength,
        yint = you.intel,
        ydex = you.dex;
    int yxp  = you.experience_level;

    for (int i = SK_FIGHTING; i < NUM_SKILLS; ++i)
        you.skills[i] = 0;

    you.experience_level = Options.fsim_xl;
    if (you.experience_level < 1)
        you.experience_level = 1;
    if (you.experience_level > 27)
        you.experience_level = 27;

    you.strength = cap_stat(Options.fsim_str);
    you.intel    = cap_stat(Options.fsim_int);
    you.dex      = cap_stat(Options.fsim_dex);

    combat(ostat, mindex, missile_slot);

    you.skills = skill_backup;
    you.strength = ystr;
    you.intel    = yint;
    you.dex      = ydex;
    you.experience_level = yxp;

    fprintf(ostat, "-----------------------------------\n\n");
    fclose(ostat);

    return (success);
}

int fsim_kit_equip(const std::string &kit)
{
    int missile_slot = -1;
    char item_buf[ITEMNAME_SIZE];

    std::string::size_type ammo_div = kit.find("/");
    std::string weapon = kit;
    std::string missile;
    if (ammo_div != std::string::npos)
    {
        weapon = kit.substr(0, ammo_div);
        missile = kit.substr(ammo_div + 1);
        trim_string(weapon);
        trim_string(missile);
    }

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        in_name(i, DESC_PLAIN, item_buf, true);
        if (std::string(item_buf).find(weapon) != std::string::npos)
        {
            if (i != you.equip[EQ_WEAPON])
            {
                wield_weapon(true, i, false);
                if (i != you.equip[EQ_WEAPON])
                    return -100;
            }
            break;
        }
    }

    if (!missile.empty())
    {
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if (!is_valid_item(you.inv[i]))
                continue;
            
            in_name(i, DESC_PLAIN, item_buf, true);
            if (std::string(item_buf).find(missile) != std::string::npos)
            {
                missile_slot = i;
                break;
            }
        }
    }

    return (missile_slot);
}

// Writes statistics about a fight to fight.stat in the current directory.
// For fight purposes, a punching bag is summoned and given lots of hp, and the
// average damage the player does to the p. bag over 10000 hits is noted, 
// advancing the weapon skill from 0 to 27, and keeping fighting skill to 2/5
// of current weapon skill.
void debug_fight_statistics(bool use_defaults, bool defence)
{
    int punching_bag = get_monster_by_name(Options.fsim_mons);
    if (punching_bag == -1 || punching_bag == MONS_PROGRAM_BUG)
        punching_bag = MONS_WORM;

    int mindex = create_fsim_monster(punching_bag, 500);
    if (mindex == -1)
    {
        mprf("Failed to create punching bag");
        return;
    }

    you.exp_available = 0;
    
    if (!use_defaults || defence)
        debug_fight_sim(mindex, -1,
                        defence? fsim_mon_hit_you : fsim_you_hit_mon);
    else
    {
        for (int i = 0, size = Options.fsim_kit.size(); i < size; ++i)
        {
            int missile = fsim_kit_equip(Options.fsim_kit[i]);
            if (missile == -100)
            {
                mprf("Aborting sim on %s", Options.fsim_kit[i].c_str());
                break;
            }
            if (!debug_fight_sim(mindex, missile, fsim_you_hit_mon))
                break;
        }
    }
    monster_die(&menv[mindex], KILL_DISMISSED, 0);    
}

static int find_trap_slot()
{
    for (int i = 0; i < MAX_TRAPS; ++i)
    {
        if (env.trap[i].type == TRAP_UNASSIGNED)
            return (i);
    }
    return (-1);
}

void debug_make_trap()
{
    char requested_trap[80];
    int trap_slot = find_trap_slot();
    trap_type trap = TRAP_UNASSIGNED;
    int gridch = grd[you.x_pos][you.y_pos];

    if (trap_slot == -1)
    {
        mpr("Sorry, this level can't take any more traps.");
        return;
    }

    if (gridch != DNGN_FLOOR)
    {
        mpr("You need to be on a floor square to make a trap.");
        return;
    }

    mprf(MSGCH_PROMPT, "What kind of trap? ");
    get_input_line( requested_trap, sizeof( requested_trap ) );
    if (!*requested_trap)
        return;

    strlwr(requested_trap);
    for (int t = TRAP_DART; t < NUM_TRAPS; ++t)
    {
        if (strstr(requested_trap, 
                   trap_name(trap_type(t))))
        {
            trap = trap_type(t);
            break;
        }
    }

    if (trap == TRAP_UNASSIGNED)
    {
        mprf("I know no traps named \"%s\"", requested_trap);
        return;
    }

    place_specific_trap(you.x_pos, you.y_pos, trap);

    mprf("Created a %s trap, marked it undiscovered",
            trap_name(trap));
}

void debug_make_shop()
{
    char requested_shop[80];
    int gridch = grd[you.x_pos][you.y_pos];
    bool have_shop_slots = false;
    int new_shop_type = SHOP_UNASSIGNED;
    bool representative = false;

    if (gridch != DNGN_FLOOR)
    {
        mpr("Insufficient floor-space for new Wal-Mart.");
        return;
    }

    for (int i = 0; i < MAX_SHOPS; ++i)
    {
        if (env.shop[i].type == SHOP_UNASSIGNED)
        {
            have_shop_slots = true;
            break;
        }
    }

    if (!have_shop_slots)
    {
        mpr("There are too many shops on this level.");
        return;
    }

    mprf(MSGCH_PROMPT, "What kind of shop? ");
    get_input_line( requested_shop, sizeof( requested_shop ) );
    if (!*requested_shop)
        return;

    strlwr(requested_shop);
    std::string s = replace_all_of(requested_shop, "*", "");
    new_shop_type = str_to_shoptype(s);

    if (new_shop_type == SHOP_UNASSIGNED || new_shop_type == -1)
    {
        mprf("Bad shop type: \"%s\"", requested_shop);
        return;
    }

    representative = !!strchr(requested_shop, '*');

    place_spec_shop(you.your_level, you.x_pos, you.y_pos, 
                    new_shop_type, representative);
    link_items();
    mprf("Done.");
}

void debug_set_stats()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for Str, Int, Dex (space separated): ");
    if (cancelable_get_line(buf, sizeof buf))
        return;

    int sstr = you.strength,
        sdex = you.dex,
        sint = you.intel;
    sscanf(buf, "%d %d %d", &sstr, &sint, &sdex);

    you.max_strength = you.strength = cap_stat(sstr);
    you.max_dex      = you.dex      = cap_stat(sdex);
    you.max_intel    = you.intel    = cap_stat(sint);

    you.redraw_strength = true;
    you.redraw_dexterity = true;
    you.redraw_intelligence = true;
    you.redraw_evasion = true;
}

#endif
