/*
 *  File:       files.cc
 *  Summary:    Functions used to save and load levels/games.
 *  Written by: Linley Henzell and Alexey Guzeev
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <7>   19 June 2000  GDL    Change handle to FILE *
 *   <6>   11/14/99      cdl    Don't let player ghosts follow you up/down
 *   <5>    7/13/99      BWR    Monsters now regenerate hps off level &
 ghosts teleport
 *   <4>    6/13/99      BWR    Added tmp file pairs to save file.
 *   <3>    6/11/99      DML    Replaced temp file deletion code.
 *
 *   <2>    5/12/99      BWR    Multiuser system support,
 *                                        including appending UID to
 *                                        name, and compressed saves
 *                                        in the SAVE_DIR_PATH directory
 *
 *   <1>   --/--/--      LRH    Created
 */

#include "AppHdr.h"
#include "files.h"
#include "version.h"

#include <string.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#include <file.h>
#endif

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef USE_EMX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef OS9
#include <stat.h>
#else
#include <sys/stat.h>
#endif

#ifdef __MINGW32__
#include <io.h>
#endif

#include "externs.h"

#include "cloud.h"
#include "clua.h"
#include "debug.h"
#include "dungeon.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "message.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "notes.h"
#include "player.h"
#include "randart.h"
#include "skills2.h"
#include "stash.h"
#include "stuff.h"
#include "tags.h"
#include "travel.h"

#ifdef SHARED_FILES_CHMOD_PRIVATE
#define DO_CHMOD_PRIVATE(x) chmod( (x), SHARED_FILES_CHMOD_PRIVATE )
#else
#define DO_CHMOD_PRIVATE(x) // empty command
#endif

void save_level(int level_saved, bool was_a_labyrinth, char where_were_you);

// temp file pairs used for file level cleanup
extern FixedArray < bool, MAX_LEVELS, MAX_BRANCHES > tmp_file_pairs;

/*
   Order for looking for conjurations for the 1st & 2nd spell slots,
   when finding spells to be remembered by a player's ghost:
 */
unsigned char search_order_conj[] = {
/* 0 */
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_BOLT_OF_DRAINING,
    SPELL_AGONY,
    SPELL_DISINTEGRATE,
    SPELL_LIGHTNING_BOLT,
    SPELL_STICKY_FLAME,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_BOLT_OF_FIRE,
    SPELL_BOLT_OF_COLD,
    SPELL_FIREBALL,
    SPELL_DELAYED_FIREBALL,
/* 10 */
    SPELL_VENOM_BOLT,
    SPELL_BOLT_OF_IRON,
    SPELL_STONE_ARROW,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_PAIN,
    SPELL_STING,
    SPELL_MAGIC_DART,
    SPELL_NO_SPELL,                        // end search
};

/*
   Order for looking for summonings and self-enchants for the 3rd spell slot:
 */
unsigned char search_order_third[] = {
/* 0 */
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_SUMMON_GREATER_DEMON,
    SPELL_SUMMON_WRAITHS,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_SUMMON_DEMON,
    SPELL_DEMONIC_HORDE,
    SPELL_HASTE,
    SPELL_ANIMATE_DEAD,
    SPELL_INVISIBILITY,
    SPELL_CALL_IMP,
    SPELL_SUMMON_SMALL_MAMMAL,
/* 10 */
    SPELL_CONTROLLED_BLINK,
    SPELL_BLINK,
    SPELL_NO_SPELL,                        // end search
};

/*
   Order for looking for enchants for the 4th + 5th spell slot. If fails, will
   go through conjs.
   Note: Dig must be in misc2 (5th) position to work.
 */
unsigned char search_order_misc[] = {
/* 0 */
    SPELL_AGONY,
    SPELL_BANISHMENT,
    SPELL_PARALYZE,
    SPELL_CONFUSE,
    SPELL_SLOW,
    SPELL_POLYMORPH_OTHER,
    SPELL_TELEPORT_OTHER,
    SPELL_DIG,
    SPELL_NO_SPELL,                        // end search
};

/* Last slot (emergency) can only be teleport self or blink. */

static void redraw_all(void)
{
    you.redraw_hit_points = 1;
    you.redraw_magic_points = 1;
    you.redraw_strength = 1;
    you.redraw_intelligence = 1;
    you.redraw_dexterity = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion = 1;
    you.redraw_experience = 1;
    you.redraw_gold = 1;

    you.redraw_status_flags = REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK;
}

struct ghost_struct ghost;

unsigned char translate_spell(unsigned char spel);
unsigned char search_third_list(unsigned char ignore_spell);
unsigned char search_second_list(unsigned char ignore_spell);
unsigned char search_first_list(unsigned char ignore_spell);

void add_spells( struct ghost_struct &gs );
void generate_random_demon();

static bool determine_version( FILE *restoreFile, 
                               char &majorVersion, char &minorVersion );

static void restore_version( FILE *restoreFile, 
                             char majorVersion, char minorVersion );

static bool determine_level_version( FILE *levelFile, 
                                     char &majorVersion, char &minorVersion );

static void restore_level_version( FILE *levelFile, 
                                   char majorVersion, char minorVersion );

static bool determine_ghost_version( FILE *ghostFile, 
                                     char &majorVersion, char &minorVersion );

static void restore_ghost_version( FILE *ghostFile, 
                                   char majorVersion, char minorVersion );

static void restore_tagged_file( FILE *restoreFile, int fileType, 
                                 char minorVersion );

static void load_ghost();

static std::string uid_as_string()
{
#ifdef MULTIUSER
    char struid[20];
    snprintf( struid, sizeof struid, "%d", (int)getuid() );
    return std::string(struid);
#else
    return std::string();
#endif
}

std::string get_savedir_filename(const char *prefix, const char *suffix, 
                                 const char *extension, bool suppress_uid)
{
    std::string result;

#ifdef SAVE_DIR_PATH
    result = SAVE_DIR_PATH;
#endif

    // Shorten string as appropriate
    result += std::string(prefix).substr(0, kFileNameLen);

    // Technically we should shorten the string first.  But if
    // MULTIUSER is set we'll have long filenames anyway. Caveat
    // emptor.
    if ( !suppress_uid )
        result += uid_as_string();

    result += suffix;

    if ( *extension ) {
	result += '.';
	result += extension;
    }

#ifdef DOS
    /* yes, this is bad, but std::transform() has its own problems */
    for ( unsigned int i = 0; i < result.size(); ++i ) {
	result[i] = toupper(result[i]);
    }
#endif
    return result;
}

std::string get_prefs_filename()
{
    return get_savedir_filename("start", "ns", "prf");
}

std::string make_filename( const char *prefix, int level, int where,
                           bool isLabyrinth, bool isGhost )
{
    char suffix[4], lvl[5];
    strcpy(suffix, (level < 10) ? "0" : "");
    itoa(level, lvl, 10);
    strcat(suffix, lvl);
    suffix[2] = where + 97;
    suffix[3] = 0;
    return get_savedir_filename( prefix, "", isLabyrinth ? "lab" : suffix,
                                 isGhost );
}

static void write_tagged_file( FILE *dataFile, char majorVersion,
                               char minorVersion, int fileType )
{
    struct tagHeader th;

    // find all relevant tags
    char tags[NUM_TAGS];
    tag_set_expected(tags, fileType);

    // write version
    struct tagHeader versionTag;
    versionTag.offset = 0;
    versionTag.tagID = TAG_VERSION;
    marshallByte(versionTag, majorVersion);
    marshallByte(versionTag, minorVersion);
    tag_write(versionTag, dataFile);

    // all other tags
    for(int i=1; i<NUM_TAGS; i++)
    {
        if (tags[i] == 1)
        {
            tag_construct(th, i);
            tag_write(th, dataFile);
        }
    }
}

bool travel_load_map( char branch, int absdepth )
{
    // Try to open level savefile.
    FILE *levelFile = fopen(make_filename(you.your_name, absdepth, branch,
                                          false, false).c_str(), "rb");
    if (!levelFile)
        return false;

    // BEGIN -- must load the old level : pre-load tasks

    // LOAD various tags
    char majorVersion;
    char minorVersion;

    if (!determine_level_version( levelFile, majorVersion, minorVersion )
            || majorVersion != SAVE_MAJOR_VERSION)
    {
        fclose(levelFile);
        return false;
    }
    
    tag_read(levelFile, minorVersion);

    fclose( levelFile );
    
    return true;
}

void load( unsigned char stair_taken, int load_mode, bool was_a_labyrinth,
           char old_level, char where_were_you2 )
{
    int j = 0;
    int i = 0, count_x = 0, count_y = 0;

    int foll_class[8];
    int foll_hp[8];
    int foll_hp_max[8];
    unsigned char foll_HD[8];
    int foll_AC[8];
    char foll_ev[8];
    unsigned char foll_speed[8];
    unsigned char foll_speed_inc[8];

    unsigned char foll_targ_1_x[8];
    unsigned char foll_targ_1_y[8];
    unsigned char foll_beh[8];
    unsigned char foll_att[8];
    int foll_sec[8];
    unsigned char foll_hit[8];

    unsigned char foll_ench[8][NUM_MON_ENCHANTS];
    unsigned char foll_flags[8];

    item_def foll_item[8][8];

    int itmf = 0;
    int ic = 0;
    int imn = 0;
    int val;

    bool just_created_level = false;

#ifdef DOS_TERM
    window(1, 1, 80, 25);
#endif

    std::string cha_fil = make_filename( you.your_name, you.your_level,
                                         you.where_are_you,
                                         you.level_type != LEVEL_DUNGEON,
                                         false );

    if (you.level_type == LEVEL_DUNGEON)
    {
        if (tmp_file_pairs[you.your_level][you.where_are_you] == false)
        {
            // make sure old file is gone
            unlink(cha_fil.c_str());

            // save the information for later deletion -- DML 6/11/99
            tmp_file_pairs[you.your_level][you.where_are_you] = true;
        }
    }

    you.prev_targ = MHITNOT;

    int following = -1;
    int minvc = 0;

    // Don't delete clouds just because the player saved and restarted.
    if (load_mode != LOAD_RESTART_GAME)
    {
        for (int clouty = 0; clouty < MAX_CLOUDS; ++clouty)
            delete_cloud( clouty );

        ASSERT( env.cloud_no == 0 );
    }

    // This block is to grab followers and save the old level to disk.
    if (load_mode == LOAD_ENTER_LEVEL)
    {
        // grab followers
        for (count_x = you.x_pos - 1; count_x < you.x_pos + 2; count_x++)
        {
            for (count_y = you.y_pos - 1; count_y < you.y_pos + 2; count_y++)
            {
                if (count_x == you.x_pos && count_y == you.y_pos)
                    continue;

                following++;
                foll_class[following] = -1;

                if (mgrd[count_x][count_y] == NON_MONSTER)
                    continue;

                struct monsters *fmenv = &menv[mgrd[count_x][count_y]];

                if (fmenv->type == MONS_PLAYER_GHOST
                    && fmenv->hit_points < fmenv->max_hit_points / 2)
                {
                    mpr("The ghost fades into the shadows.");
                    monster_teleport(fmenv, true);
                    continue;
                }

                // monster has to be already tagged in order to follow:
                if (!testbits( fmenv->flags, MF_TAKING_STAIRS ))
                    continue;

#if DEBUG_DIAGNOSTICS
                snprintf( info, INFO_SIZE, "%s is following.", 
                          ptr_monam( fmenv, DESC_CAP_THE ) );
                mpr( info, MSGCH_DIAGNOSTICS );
#endif

                foll_class[following] = fmenv->type;
                foll_hp[following] = fmenv->hit_points;
                foll_hp_max[following] = fmenv->max_hit_points;
                foll_HD[following] = fmenv->hit_dice;
                foll_AC[following] = fmenv->armour_class;
                foll_ev[following] = fmenv->evasion;
                foll_speed[following] = fmenv->speed;
                foll_speed_inc[following] = fmenv->speed_increment;
                foll_targ_1_x[following] = fmenv->target_x;
                foll_targ_1_y[following] = fmenv->target_y;

                for (minvc = 0; minvc < NUM_MONSTER_SLOTS; ++minvc)
                {
                    const int item = fmenv->inv[minvc];
                    if (item == NON_ITEM)
                    {
                        foll_item[following][minvc].quantity = 0;
                        continue;
                    }

                    foll_item[following][minvc] = mitm[item];
                    destroy_item( item );
                }

                foll_beh[following] = fmenv->behaviour;
                foll_att[following] = fmenv->attitude;
                foll_sec[following] = fmenv->number;
                foll_hit[following] = fmenv->foe;

                for (j = 0; j < NUM_MON_ENCHANTS; j++)
                {
                    foll_ench[following][j] = fmenv->enchantment[j];
                    fmenv->enchantment[j] = ENCH_NONE;
                }

                foll_flags[following] = fmenv->flags;

                fmenv->flags = 0;
                fmenv->type = -1;
                fmenv->hit_points = 0;
                fmenv->max_hit_points = 0;
                fmenv->hit_dice = 0;
                fmenv->armour_class = 0;
                fmenv->evasion = 0;

                mgrd[count_x][count_y] = NON_MONSTER;
            }
        }                        // end of grabbing followers

        if (!was_a_labyrinth)
            save_level( old_level, false, where_were_you2 );

        was_a_labyrinth = false;
    }

    // clear out ghost/demon lord information:
    strcpy( ghost.name, "" );
    for (ic = 0; ic < NUM_GHOST_VALUES; ++ic)
        ghost.values[ic] = 0;

    // Try to open level savefile.
    FILE *levelFile = fopen(cha_fil.c_str(), "rb");

    // GENERATE new level when the file can't be opened:
    if (levelFile == NULL)
    {                           
        strcpy(ghost.name, "");

        for (imn = 0; imn < NUM_GHOST_VALUES; ++imn)
            ghost.values[imn] = 0;

        builder( you.your_level, you.level_type );
        just_created_level = true;

        if (you.level_type == LEVEL_PANDEMONIUM)
            generate_random_demon();

        if (you.your_level > 1 && one_chance_in(3))
            load_ghost();
    }
    else
    {
        // BEGIN -- must load the old level : pre-load tasks

        // LOAD various tags
        char majorVersion = 0;
        char minorVersion = 0;

        if (!determine_level_version( levelFile, majorVersion, minorVersion ))
        {
            perror("\nLevel file appears to be invalid.\n");
            end(-1);
        }

        restore_level_version( levelFile, majorVersion, minorVersion );

        // sanity check - EOF
        if (!feof( levelFile ))
        {
            snprintf( info, INFO_SIZE, "\nIncomplete read of \"%s\" - aborting.\n", cha_fil.c_str());
            perror(info);
            end(-1);
        }

        fclose( levelFile );

        // POST-LOAD tasks :
        link_items();
        redraw_all();
    }

    // closes all the gates if you're on the way out
    for (i = 0; i < GXM; i++)
    {
        for (j = 0; j < GYM; j++)
        {
            if (just_created_level)
                env.map[i][j] = 0;

            if (you.char_direction == DIR_ASCENDING
                && you.level_type != LEVEL_PANDEMONIUM)
            {
                if (grd[i][j] == DNGN_ENTER_HELL
                    || grd[i][j] == DNGN_ENTER_ABYSS
                    || grd[i][j] == DNGN_ENTER_PANDEMONIUM)
                {
                    grd[i][j] = DNGN_STONE_ARCH;
                }
            }

            if (load_mode != LOAD_RESTART_GAME)
                env.cgrid[i][j] = EMPTY_CLOUD;
        }
    }

    // This next block is for cases where we want to look for a stairs 
    // to place the player.
    if (load_mode != LOAD_RESTART_GAME && you.level_type != LEVEL_ABYSS)
    {
        bool find_first = true;

        // Order is important here:
        if (you.level_type == LEVEL_DUNGEON 
            && where_were_you2 == BRANCH_VESTIBULE_OF_HELL
            && stair_taken == DNGN_STONE_STAIRS_UP_I)
        {
            // leaving hell - look for entry potal first
            stair_taken = DNGN_ENTER_HELL;
            find_first = false;
        }
        else if (stair_taken == DNGN_EXIT_PANDEMONIUM)
        {
            stair_taken = DNGN_ENTER_PANDEMONIUM;
            find_first = false;
        }
        else if (stair_taken == DNGN_EXIT_ABYSS)
        {
            stair_taken = DNGN_ENTER_ABYSS;
            find_first = false;
        }
        else if (stair_taken == DNGN_ENTER_HELL 
            || stair_taken == DNGN_ENTER_LABYRINTH)
        {
            // the vestibule and labyrith always start from this stair
            stair_taken = DNGN_STONE_STAIRS_UP_I;
        }
        else if (stair_taken >= DNGN_STONE_STAIRS_DOWN_I 
            && stair_taken <= DNGN_ROCK_STAIRS_DOWN)
        {
            // look for coresponding up stair
            stair_taken += (DNGN_STONE_STAIRS_UP_I - DNGN_STONE_STAIRS_DOWN_I);
        }
        else if (stair_taken >= DNGN_STONE_STAIRS_UP_I 
            && stair_taken <= DNGN_ROCK_STAIRS_UP)
        {
            // look for coresponding down stair
            stair_taken += (DNGN_STONE_STAIRS_DOWN_I - DNGN_STONE_STAIRS_UP_I);
        }
        else if (stair_taken >= DNGN_RETURN_FROM_ORCISH_MINES 
            && stair_taken < 150) // 20 slots reserved
        {
            // find entry point to subdungeon when leaving
            stair_taken += (DNGN_ENTER_ORCISH_MINES - DNGN_RETURN_FROM_ORCISH_MINES);
        }
        else if (stair_taken >= DNGN_ENTER_ORCISH_MINES
             && stair_taken < DNGN_RETURN_FROM_ORCISH_MINES)
        {
            // find exit staircase from subdungeon when entering 
            stair_taken += (DNGN_RETURN_FROM_ORCISH_MINES - DNGN_ENTER_ORCISH_MINES);
        }
        else if (stair_taken >= DNGN_ENTER_DIS 
            && stair_taken <= DNGN_TRANSIT_PANDEMONIUM)
        {
            // when entering a hell or pandemonium
            stair_taken = DNGN_STONE_STAIRS_UP_I;
        }
        else // Note: stair_taken can equal things like DNGN_FLOOR
        {
            // just find a nice empty square
            stair_taken = DNGN_FLOOR;
            find_first = false;
        }

        int found = 0;
        int x_pos = 0, y_pos = 0;

        // Start by looking for the expected entry point:
        for (count_x = 0; count_x < GXM; count_x++)
        {
            for (count_y = 0; count_y < GYM; count_y++)
            {
                if (grd[count_x][count_y] == stair_taken)
                {
                    found++;
                    if (one_chance_in( found ))
                    {
                        x_pos = count_x;
                        y_pos = count_y;
                    }

                    if (find_first)
                        goto found_stair;  // double break
                }
            }
        }

found_stair:
        if (!found)
        {
            // See if we can find a stairway in the "right" direction:
            for (count_x = 0; count_x < GXM; count_x++)
            {
                for (count_y = 0; count_y < GYM; count_y++)
                {
                    if (stair_taken <= DNGN_ROCK_STAIRS_DOWN)
                    {
                        // looking for any down stairs
                        if (grd[count_x][count_y] >= DNGN_STONE_STAIRS_DOWN_I
                            && grd[count_x][count_y] <= DNGN_ROCK_STAIRS_DOWN)
                        {
                            found++;
                            if (one_chance_in( found ))
                            {
                                x_pos = count_x;
                                y_pos = count_y;
                            }
                        }
                    }
                    else 
                    {
                        // looking for any up stairs
                        if (grd[count_x][count_y] >= DNGN_STONE_STAIRS_UP_I
                            && grd[count_x][count_y] <= DNGN_ROCK_STAIRS_UP)
                        {
                            found++;
                            if (one_chance_in( found ))
                            {
                                x_pos = count_x;
                                y_pos = count_y;
                            }
                        }
                    }
                }
            }

            if (!found)
            {
                // Still not found? Look for any clear terrain:
                for (count_x = 0; count_x < GXM; count_x++)
                {
                    for (count_y = 0; count_y < GYM; count_y++)
                    {
                        if (grd[count_x][count_y] >= DNGN_FLOOR)
                        {
                            found++;
                            if (one_chance_in( found ))
                            {
                                x_pos = count_x;
                                y_pos = count_y;
                            }
                        }
                    }
                }
            }
        }

        // If still not found, the level is very buggy.
        ASSERT( found );

        you.x_pos = x_pos;
        you.y_pos = y_pos;
    }
    else if (load_mode != LOAD_RESTART_GAME && you.level_type == LEVEL_ABYSS)
    {
        you.x_pos = 45;
        you.y_pos = 35;
    }

    // This should fix the "monster occuring under the player" bug?
    if (mgrd[you.x_pos][you.y_pos] != NON_MONSTER)
        monster_teleport(&menv[mgrd[you.x_pos][you.y_pos]], true);
    /*
    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        grd[you.x_pos][you.y_pos] = DNGN_FLOOR;
    */
    following = 0;
    int fmenv = -1;

    // actually "move" the followers if applicable
    if ((you.level_type == LEVEL_DUNGEON
            || you.level_type == LEVEL_PANDEMONIUM)
        && load_mode == LOAD_ENTER_LEVEL)
    {
        for (ic = 0; ic < 2; ic++)
        {
            for (count_x = you.x_pos - 6; count_x < you.x_pos + 7;
                 count_x++)
            {
                for (count_y = you.y_pos - 6; count_y < you.y_pos + 7;
                     count_y++)
                {
                    if (ic == 0
                        && ((count_x < you.x_pos - 1)
                            || (count_x > you.x_pos + 1)
                            || (count_y < you.y_pos - 1)
                            || (count_y > you.y_pos + 1)))
                    {
                        continue;
                    }

                    if (count_x == you.x_pos && count_y == you.y_pos)
                        continue;

                    if (mgrd[count_x][count_y] != NON_MONSTER
                        || grd[count_x][count_y] < DNGN_FLOOR)
                    {
                        continue;
                    }

                    while (menv[following].type != -1)
                    {
                        following++;

                        if (following >= MAX_MONSTERS)
                            goto out_of_foll;
                    }

                    while (fmenv < 7)
                    {
                        fmenv++;

                        if (foll_class[fmenv] == -1)
                            continue;

                        menv[following].type = foll_class[fmenv];
                        menv[following].hit_points = foll_hp[fmenv];
                        menv[following].max_hit_points = foll_hp_max[fmenv];
                        menv[following].hit_dice = foll_HD[fmenv];
                        menv[following].armour_class = foll_AC[fmenv];
                        menv[following].evasion = foll_ev[fmenv];
                        menv[following].speed = foll_speed[fmenv];
                        menv[following].x = count_x;
                        menv[following].y = count_y;
                        menv[following].target_x = 0;
                        menv[following].target_y = 0;
                        menv[following].speed_increment = foll_speed_inc[fmenv];

                        for (minvc = 0; minvc < NUM_MONSTER_SLOTS; minvc++)
                        {

                            if (!is_valid_item(foll_item[fmenv][minvc]))
                            {
                                menv[following].inv[minvc] = NON_ITEM;
                                continue;
                            }

                            itmf = get_item_slot(0);
                            if (itmf == NON_ITEM)
                            {
                                menv[following].inv[minvc] = NON_ITEM;
                                continue;
                            }

                            mitm[itmf] = foll_item[fmenv][minvc];
                            mitm[itmf].x = 0;
                            mitm[itmf].y = 0;
                            mitm[itmf].link = NON_ITEM;

                            menv[following].inv[minvc] = itmf;
                        }

                        menv[following].behaviour = foll_beh[fmenv];
                        menv[following].attitude = foll_att[fmenv];
                        menv[following].number = foll_sec[fmenv];
                        menv[following].foe = foll_hit[fmenv];

                        for (j = 0; j < NUM_MON_ENCHANTS; j++)
                            menv[following].enchantment[j]=foll_ench[fmenv][j];

                        menv[following].flags = foll_flags[fmenv];
                        menv[following].flags |= MF_JUST_SUMMONED;

                        mgrd[count_x][count_y] = following;
                        break;
                    }
                }
            }
        }
    }                       // end of moving followers

  out_of_foll:
    redraw_all();

    // Sanity forcing of monster inventory items (required?)
    for (i = 0; i < MAX_MONSTERS; i++)
    {
        if (menv[i].type == -1)
            continue;

        for (j = 0; j < NUM_MONSTER_SLOTS; j++)
        {
            if (menv[i].inv[j] == NON_ITEM)
                continue;

            /* items carried by monsters shouldn't be linked */
            if (mitm[menv[i].inv[j]].link != NON_ITEM)
                mitm[menv[i].inv[j]].link = NON_ITEM;
        }
    }

    // Translate stairs for pandemonium levels:
    if (you.level_type == LEVEL_PANDEMONIUM)
    {
        for (count_x = 0; count_x < GXM; count_x++)
        {
            for (count_y = 0; count_y < GYM; count_y++)
            {
                if (grd[count_x][count_y] >= DNGN_STONE_STAIRS_UP_I
                    && grd[count_x][count_y] <= DNGN_ROCK_STAIRS_UP)
                {
                    if (one_chance_in( you.mutation[MUT_PANDEMONIUM] ? 5 : 50 ))
                        grd[count_x][count_y] = DNGN_EXIT_PANDEMONIUM;
                    else
                        grd[count_x][count_y] = DNGN_FLOOR;
                }

                if (grd[count_x][count_y] >= DNGN_ENTER_LABYRINTH
                    && grd[count_x][count_y] <= DNGN_ROCK_STAIRS_DOWN)
                {
                    grd[count_x][count_y] = DNGN_TRANSIT_PANDEMONIUM;
                }
            }
        }
    }

    // Things to update for player entering level
    if (load_mode == LOAD_ENTER_LEVEL)
    {
        // update corpses and fountains
        if (env.elapsed_time != 0.0)
            update_level( you.elapsed_time - env.elapsed_time );

        // Centaurs have difficulty with stairs
        val = ((you.species != SP_CENTAUR) ? player_movement_speed() : 15); 

        // new levels have less wary monsters: 
        if (just_created_level)
            val /= 2;

        val -= (stepdown_value( check_stealth(), 50, 50, 150, 150 ) / 10);

#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, "arrival time: %d", val );
        mpr( info, MSGCH_DIAGNOSTICS ); 
#endif

        if (val > 0)
        {
            you.time_taken = val;
            handle_monsters();
        }
    }

    // Save the created/updated level out to disk:
    save_level( you.your_level, (you.level_type != LEVEL_DUNGEON),
                you.where_are_you );
}                               // end load()

void save_level(int level_saved, bool was_a_labyrinth, char where_were_you)
{
    std::string cha_fil = make_filename( you.your_name, level_saved,
                                         where_were_you, was_a_labyrinth,
                                         false );

    you.prev_targ = MHITNOT;

    FILE *saveFile = fopen(cha_fil.c_str(), "wb");

    if (saveFile == NULL)
    {
        snprintf(info, INFO_SIZE, "Unable to open \"%s\" for writing!",
                 cha_fil.c_str());
        perror(info);
        end(-1);
    }

    // nail all items to the ground
    fix_item_coordinates();

    // 0.0 initial genesis of saved format
    // 0.1 added attitude tag
    // 0.2 replaced old 'enchantment1' and with 'flags' (bitfield)
    // 0.3 changes to make the item structure more sane  
    // 0.4 changes to the ghost save section
    // 0.5 spell and ability letter arrays
    // 0.6 inventory slots of items
    // 0.7 origin tracking for items
    // 0.8 widened env.map to 2 bytes
    // 0.9 inscriptions (hp)
    // 0.10 Monster colour and spells separated from mons->number.

    write_tagged_file( saveFile, SAVE_MAJOR_VERSION, 10, TAGTYPE_LEVEL );

    fclose(saveFile);

    DO_CHMOD_PRIVATE(cha_fil.c_str());
}                               // end save_level()


void save_game(bool leave_game)
{

#ifdef STASH_TRACKING
    /* Stashes */
    std::string stashFile = get_savedir_filename( you.your_name, "", "st" );
    FILE *stashf = fopen(stashFile.c_str(), "wb");
    if (stashf) {
        stashes.save(stashf);
        fclose(stashf);
	DO_CHMOD_PRIVATE(stashFile.c_str());
    }
#endif

#ifdef CLUA_BINDINGS
    /* lua */
    std::string luaFile = get_savedir_filename( you.your_name, "", "lua" );
    clua.save(luaFile.c_str());
    // note that luaFile may not exist
    DO_CHMOD_PRIVATE(luaFile.c_str());
#endif

    /* kills */
    std::string killFile = get_savedir_filename( you.your_name, "", "kil" );
    FILE *killf = fopen(killFile.c_str(), "wb");
    if (killf) {
        you.kills.save(killf);
        fclose(killf);
	DO_CHMOD_PRIVATE(killFile.c_str());
    }

    /* travel cache */
    std::string travelCacheFile = get_savedir_filename(you.your_name,"","tc");
    FILE *travelf = fopen(travelCacheFile.c_str(), "wb");
    if (travelf) {
        travel_cache.save(travelf);
        fclose(travelf);
	DO_CHMOD_PRIVATE(travelCacheFile.c_str());
    }
    
    /* notes */
    std::string notesFile = get_savedir_filename(you.your_name, "", "nts");
    FILE *notesf = fopen(notesFile.c_str(), "wb");
    if (notesf) {
	save_notes(notesf);
        fclose(notesf);
	DO_CHMOD_PRIVATE(notesFile.c_str());
    }

    std::string charFile = get_savedir_filename(you.your_name, "", "sav");
    FILE *charf = fopen(charFile.c_str(), "wb");
    if (!charf) {
	snprintf(info, INFO_SIZE, "Unable to open \"%s\" for writing!\n",
                 charFile.c_str());
	perror(info);
	end(-1);
    }

    // 0.0 initial genesis of saved format
    // 0.1 changes to make the item structure more sane  
    // 0.2 spell and ability tables
    // 0.3 added you.magic_contamination (05/03/05)
    // 0.4 added item origins
    // 0.5 added num_gifts
    // 0.6 inscriptions
    write_tagged_file( charf, SAVE_MAJOR_VERSION, 6, TAGTYPE_PLAYER );

    fclose(charf);
    DO_CHMOD_PRIVATE(charFile.c_str());

    // if just save, early out
    if (!leave_game)
        return;

    // must be exiting -- save level & goodbye!
    save_level(you.your_level, (you.level_type != LEVEL_DUNGEON),
               you.where_are_you);

#ifdef DOS_TERM
    window(1, 1, 80, 25);
#endif

    clrscr();

#ifdef SAVE_PACKAGE_CMD
    std::string basename = get_savedir_filename(you.your_name, "", "");
    char cmd_buff[1024];

    snprintf( cmd_buff, sizeof(cmd_buff), 
              SAVE_PACKAGE_CMD, basename.c_str(), basename.c_str() );


    if (system( cmd_buff ) != 0) {
        cprintf( EOL "Warning: Zip command (SAVE_PACKAGE_CMD) returned non-zero value!" EOL );
    }
    DO_CHMOD_PRIVATE ( (basename + PACKAGE_SUFFIX).c_str() );
#endif

    cprintf( "See you soon, %s!" EOL , you.your_name );
    end(0);
}                               // end save_game()

void load_ghost(void)
{
    char majorVersion;
    char minorVersion;
    int imn;
    int i;

    std::string cha_fil = make_filename("bones", you.your_level,
                                        you.where_are_you,
                                        (you.level_type != LEVEL_DUNGEON),
                                        true );

    FILE *gfile = fopen(cha_fil.c_str(), "rb");

    if (gfile == NULL)
        return;                 // no such ghost.

    if (!determine_ghost_version(gfile, majorVersion, minorVersion))
    {
        fclose(gfile);
#if DEBUG_DIAGNOSTICS
        snprintf( info, INFO_SIZE, "Ghost file \"%s\" seems to be invalid.",
                  cha_fil.c_str());
        mpr( info, MSGCH_DIAGNOSTICS );
        more();
#endif
        return;
    }

    restore_ghost_version(gfile, majorVersion, minorVersion);

    // sanity check - EOF
    if (!feof(gfile))
    {
        fclose(gfile);
#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Incomplete read of \"%s\".",
                  cha_fil.c_str() );
        more();
#endif
        return;
    }

    fclose(gfile);

#if DEBUG_DIAGNOSTICS
        mpr( "Loaded ghost.", MSGCH_DIAGNOSTICS );
#endif

    // remove bones file - ghosts are hardly permanent.
    unlink(cha_fil.c_str());

    // translate ghost to monster and place.
    for (imn = 0; imn < MAX_MONSTERS - 10; imn++)
    {
        if (menv[imn].type != -1)
            continue;

        menv[imn].type = MONS_PLAYER_GHOST;
        menv[imn].hit_dice = ghost.values[ GVAL_EXP_LEVEL ];
        menv[imn].hit_points = ghost.values[ GVAL_MAX_HP ];
        menv[imn].max_hit_points = ghost.values[ GVAL_MAX_HP ];
        menv[imn].armour_class = ghost.values[ GVAL_AC];
        menv[imn].evasion = ghost.values[ GVAL_EV ];
        menv[imn].speed = 10;
        menv[imn].speed_increment = 70;
        menv[imn].attitude = ATT_HOSTILE;
        menv[imn].behaviour = BEH_WANDER;
        menv[imn].flags = 0;
        menv[imn].foe = MHITNOT;
        menv[imn].foe_memory = 0;

        menv[imn].number = 250;

        for (i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
        {
            if (ghost.values[i] != MS_NO_SPELL)
            {
                menv[imn].number = MST_GHOST;
                break;
            }
        }

        for (i = 0; i < NUM_MONSTER_SLOTS; i++)
            menv[imn].inv[i] = NON_ITEM;

        for (i = 0; i < NUM_MON_ENCHANTS; i++)
            menv[imn].enchantment[i] = ENCH_NONE;

        do
        {
            menv[imn].x = random2(GXM - 20) + 10;
            menv[imn].y = random2(GYM - 20) + 10;
        }
        while ((grd[menv[imn].x][menv[imn].y] != DNGN_FLOOR)
               || (mgrd[menv[imn].x][menv[imn].y] != NON_MONSTER));

        mgrd[menv[imn].x][menv[imn].y] = imn;
        break;
    }
}


void restore_game(void)
{
    std::string charFile = get_savedir_filename(you.your_name, "", "sav");
    FILE *charf = fopen(charFile.c_str(), "rb");
    if (!charf )
    {
	snprintf(info, INFO_SIZE, "Unable to open %s for reading!\n",
		 charFile.c_str() );
        perror(info);
        end(-1);
    }

    char majorVersion = 0;
    char minorVersion = 0;

    if (!determine_version(charf, majorVersion, minorVersion))
    {
        perror("\nSavefile appears to be invalid.\n");
        end(-1);
    }

    restore_version(charf, majorVersion, minorVersion);

    // sanity check - EOF
    if (!feof(charf))
    {
        snprintf( info, INFO_SIZE, "\nIncomplete read of \"%s\" - aborting.\n",
		  charFile.c_str());
        perror(info);
        end(-1);
    }

    fclose(charf);

#ifdef STASH_TRACKING
    std::string stashFile = get_savedir_filename( you.your_name, "", "st" );
    FILE *stashf = fopen(stashFile.c_str(), "rb");
    if (stashf) {
        stashes.load(stashf);
        fclose(stashf);
    }
#endif

#ifdef CLUA_BINDINGS
    std::string luaFile = get_savedir_filename( you.your_name, "", "lua" );
    clua.execfile( luaFile.c_str() );
#endif

    std::string killFile = get_savedir_filename( you.your_name, "", "kil" );
    FILE *killf = fopen(killFile.c_str(), "rb");
    if (killf) {
        you.kills.load(killf);
        fclose(killf);
    }

    std::string travelCacheFile = get_savedir_filename(you.your_name,"","tc");
    FILE *travelf = fopen(travelCacheFile.c_str(), "rb");
    if (travelf) {
        travel_cache.load(travelf);
        fclose(travelf);
    }

    std::string notesFile = get_savedir_filename(you.your_name, "", "nts");
    FILE *notesf = fopen(notesFile.c_str(), "rb");
    if (notesf) {
	load_notes(notesf);
	fclose(notesf);
    }
}

static bool determine_version( FILE *restoreFile, 
                               char &majorVersion, char &minorVersion )
{
    // read first two bytes.
    char buf[2];
    if (read2(restoreFile, buf, 2) != 2)
        return false;               // empty file?

    // otherwise, read version and validate.
    majorVersion = buf[0];
    minorVersion = buf[1];

    if (majorVersion == SAVE_MAJOR_VERSION)
        return true;

    return false;   // if its not 0, no idea
}

static void restore_version( FILE *restoreFile, 
                             char majorVersion, char minorVersion )
{
    // assuming the following check can be removed once we can read all
    // savefile versions.
    if (majorVersion != SAVE_MAJOR_VERSION)
    {
        snprintf( info, INFO_SIZE, "\nSorry, this release cannot read a v%d.%d savefile.\n",
            majorVersion, minorVersion);
        perror(info);
        end(-1);
    }

    switch(majorVersion)
    {
        case SAVE_MAJOR_VERSION:
            restore_tagged_file(restoreFile, TAGTYPE_PLAYER, minorVersion);
            break;
        default:
            break;
    }
}

// generic v4 restore function
static void restore_tagged_file( FILE *restoreFile, int fileType, 
                                 char minorVersion )
{
    int i;

    char tags[NUM_TAGS];
    tag_set_expected(tags, fileType);

    while(1)
    {
        i = tag_read(restoreFile, minorVersion);
        if (i == 0)                 // no tag!
            break;
        tags[i] = 0;                // tag read
    }

    // go through and init missing tags
    for(i=0; i<NUM_TAGS; i++)
    {
        if (tags[i] == 1)           // expected but never read
            tag_missing(i, minorVersion);
    }
}

static bool determine_level_version( FILE *levelFile, 
                                     char &majorVersion, char &minorVersion )
{
    // read first two bytes.
    char buf[2];
    if (read2(levelFile, buf, 2) != 2)
        return false;               // empty file?

    // otherwise, read version and validate.
    majorVersion = buf[0];
    minorVersion = buf[1];

    if (majorVersion == SAVE_MAJOR_VERSION)
        return true;

    return false;   // if its not SAVE_MAJOR_VERSION, no idea
}

static void restore_level_version( FILE *levelFile, 
                                   char majorVersion, char minorVersion )
{
    // assuming the following check can be removed once we can read all
    // savefile versions.
    if (majorVersion != SAVE_MAJOR_VERSION)
    {
        snprintf( info, INFO_SIZE, "\nSorry, this release cannot read a v%d.%d level file.\n",
            majorVersion, minorVersion);
        perror(info);
        end(-1);
    }

    switch(majorVersion)
    {
        case SAVE_MAJOR_VERSION:
            restore_tagged_file(levelFile, TAGTYPE_LEVEL, minorVersion);
            break;
        default:
            break;
    }
}

static bool determine_ghost_version( FILE *ghostFile, 
                                     char &majorVersion, char &minorVersion )
{
    // read first two bytes.
    char buf[2];
    if (read2(ghostFile, buf, 2) != 2)
        return false;               // empty file?

    // check for pre-v4 -- simply started right in with ghost name.
    if (isprint(buf[0]) && buf[0] > 4)
    {
        majorVersion = 0;
        minorVersion = 0;
        rewind(ghostFile);
        return true;
    }

    // otherwise, read version and validate.
    majorVersion = buf[0];
    minorVersion = buf[1];

    if (majorVersion == SAVE_MAJOR_VERSION)
        return true;

    return false;   // if its not SAVE_MAJOR_VERSION, no idea!
}

static void restore_ghost_version( FILE *ghostFile, 
                                   char majorVersion, char minorVersion )
{
    switch(majorVersion)
    {
        case SAVE_MAJOR_VERSION:
            restore_tagged_file(ghostFile, TAGTYPE_GHOST, minorVersion);
            break;
        default:
            break;
    }
}

void save_ghost( bool force )
{
    const int wpn = you.equip[EQ_WEAPON];

    if (!force && (you.your_level < 2 || you.is_undead))
        return;

    std::string cha_fil = make_filename( "bones", you.your_level,
                                         you.where_are_you,
                                         (you.level_type != LEVEL_DUNGEON),
                                         true );

    FILE *gfile = fopen(cha_fil.c_str(), "rb");

    // don't overwrite existing bones!
    if (gfile != NULL)
    {
        fclose(gfile);
        return;
    }

    snprintf(ghost.name, sizeof ghost.name, "%s", you.your_name);

    ghost.values[ GVAL_MAX_HP ]    = ((you.hp_max >= 150) ? 150 : you.hp_max);
    ghost.values[ GVAL_EV ]        = player_evasion();
    ghost.values[ GVAL_AC ]        = player_AC();
    ghost.values[ GVAL_SEE_INVIS ] = player_see_invis();
    ghost.values[ GVAL_RES_FIRE ]  = player_res_fire();
    ghost.values[ GVAL_RES_COLD ]  = player_res_cold();
    ghost.values[ GVAL_RES_ELEC ]  = player_res_electricity();

    /* note - as ghosts, automatically get res poison + prot_life */

    int d = 4;
    int e = 0;

    if (wpn != -1)
    {
        if (you.inv[wpn].base_type == OBJ_WEAPONS
            || you.inv[wpn].base_type == OBJ_STAVES)
        {
            d = property( you.inv[wpn], PWPN_DAMAGE );

            d *= 25 + you.skills[weapon_skill( you.inv[wpn].base_type,
                                               you.inv[wpn].sub_type )];
            d /= 25;

            if (you.inv[wpn].base_type == OBJ_WEAPONS)
            {
                if (is_random_artefact( you.inv[wpn] ))
                    e = randart_wpn_property( you.inv[wpn], RAP_BRAND );
                else
                    e = you.inv[wpn].special;
            }
        }
    }
    else
    {
        /* Unarmed combat */
        if (you.species == SP_TROLL)
            d += you.experience_level;

        d += you.skills[SK_UNARMED_COMBAT];
    }

    d *= 30 + you.skills[SK_FIGHTING];
    d /= 30;

    d += you.strength / 4;

    if (d > 50)
        d = 50;

    ghost.values[ GVAL_DAMAGE ] = d;
    ghost.values[ GVAL_BRAND ]  = e;
    ghost.values[ GVAL_SPECIES ] = you.species;
    ghost.values[ GVAL_BEST_SKILL ] = best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99);
    ghost.values[ GVAL_SKILL_LEVEL ] = you.skills[best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99)];
    ghost.values[ GVAL_EXP_LEVEL ] = you.experience_level;
    ghost.values[ GVAL_CLASS ] = you.char_class;

    add_spells(ghost);

    gfile = fopen(cha_fil.c_str(), "wb");

    if (gfile == NULL)
    {
        snprintf(info, INFO_SIZE, "Error creating ghost file: %s",
                 cha_fil.c_str());
        mpr(info);
        more();
        return;
    }

    // 0.0-0.3  old tagged savefile (values as unsigned char)
    // 0.4      new tagged savefile (values as signed short)
    write_tagged_file( gfile, SAVE_MAJOR_VERSION, 4, TAGTYPE_GHOST );

    fclose(gfile);

#if DEBUG_DIAGNOSTICS
    mpr( "Saved ghost.", MSGCH_DIAGNOSTICS );
#endif

    DO_CHMOD_PRIVATE(cha_fil.c_str());
}                               // end save_ghost()

/*
   Used when creating ghosts: goes through and finds spells for the ghost to
   cast. Death is a traumatic experience, so ghosts only remember a few spells.
 */
void add_spells( struct ghost_struct &gs )
{
    int i = 0;

    for (i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
        gs.values[i] = SPELL_NO_SPELL;

    gs.values[ GVAL_SPELL_1 ] = search_first_list(SPELL_NO_SPELL);
    gs.values[ GVAL_SPELL_2 ] = search_first_list(gs.values[GVAL_SPELL_1]);
    gs.values[ GVAL_SPELL_3 ] = search_second_list(SPELL_NO_SPELL);
    gs.values[ GVAL_SPELL_4 ] = search_third_list(SPELL_NO_SPELL);

    if (gs.values[ GVAL_SPELL_4 ] == SPELL_NO_SPELL)
        gs.values[ GVAL_SPELL_4 ] = search_first_list(SPELL_NO_SPELL);

    gs.values[ GVAL_SPELL_5 ] = search_first_list(gs.values[GVAL_SPELL_4]);

    if (gs.values[ GVAL_SPELL_5 ] == SPELL_NO_SPELL)
        gs.values[ GVAL_SPELL_5 ] = search_first_list(gs.values[GVAL_SPELL_4]);

    if (player_has_spell( SPELL_DIG ))
        gs.values[ GVAL_SPELL_5 ] = SPELL_DIG;

    /* Looks for blink/tport for emergency slot */
    if (player_has_spell( SPELL_CONTROLLED_BLINK ) 
        || player_has_spell( SPELL_BLINK ))
    {
        gs.values[ GVAL_SPELL_6 ] = SPELL_CONTROLLED_BLINK;
    }

    if (player_has_spell( SPELL_TELEPORT_SELF ))
        gs.values[ GVAL_SPELL_6 ] = SPELL_TELEPORT_SELF;

    for (i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
        gs.values[i] = translate_spell( gs.values[i] );
}                               // end add_spells()

unsigned char search_first_list(unsigned char ignore_spell)
{
    for (int i = 0; i < 20; i++)
     {
        if (search_order_conj[i] == SPELL_NO_SPELL)
            return SPELL_NO_SPELL;

        if (search_order_conj[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_conj[i]))
            return search_order_conj[i];
    }

    return SPELL_NO_SPELL;
}                               // end search_first_list()

unsigned char search_second_list(unsigned char ignore_spell)
{
    for (int i = 0; i < 20; i++)
    {
        if (search_order_third[i] == SPELL_NO_SPELL)
            return SPELL_NO_SPELL;

        if (search_order_third[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_third[i]))
            return search_order_third[i];
    }

    return SPELL_NO_SPELL;
}                               // end search_second_list()

unsigned char search_third_list(unsigned char ignore_spell)
{
    for (int i = 0; i < 20; i++)
    {
        if (search_order_misc[i] == SPELL_NO_SPELL)
            return SPELL_NO_SPELL;

        if (search_order_misc[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_misc[i]))
            return search_order_misc[i];
    }

    return SPELL_NO_SPELL;
}                               // end search_third_list()


/*
   When passed the number for a player spell, returns the equivalent monster
   spell. Returns SPELL_NO_SPELL on failure (no equiv).
 */
unsigned char translate_spell(unsigned char spel)
{
    switch (spel)
    {
    case SPELL_TELEPORT_SELF:
        return (MS_TELEPORT);

    case SPELL_MAGIC_DART:
        return (MS_MMISSILE);
    case SPELL_FIREBALL:
    case SPELL_DELAYED_FIREBALL:
        return (MS_FIREBALL);
    case SPELL_DIG:
        return (MS_DIG);
    case SPELL_BOLT_OF_FIRE:
        return (MS_FIRE_BOLT);
    case SPELL_BOLT_OF_COLD:
        return (MS_COLD_BOLT);
    case SPELL_LIGHTNING_BOLT:
        return (MS_LIGHTNING_BOLT);
    case SPELL_POLYMORPH_OTHER:
        return (MS_MUTATION);
    case SPELL_SLOW:
        return (MS_SLOW);
    case SPELL_HASTE:
        return (MS_HASTE);
    case SPELL_PARALYZE:
        return (MS_PARALYSIS);
    case SPELL_CONFUSE:
        return (MS_CONFUSE);
    case SPELL_INVISIBILITY:
        return (MS_INVIS);
    case SPELL_THROW_FLAME:
        return (MS_FLAME);
    case SPELL_THROW_FROST:
        return (MS_FROST);
    case SPELL_CONTROLLED_BLINK:
        return (MS_BLINK);        /* approximate */
/*  case FREEZING_CLOUD: return ; no freezing/mephitic cloud yet
   case MEPHITIC_CLOUD: return ; */
    case SPELL_VENOM_BOLT:
        return (MS_VENOM_BOLT);
    case SPELL_POISON_ARROW:
        return (MS_POISON_ARROW);
    case SPELL_TELEPORT_OTHER:
        return (MS_TELEPORT_OTHER);
    case SPELL_SUMMON_SMALL_MAMMAL:
        return (MS_SUMMON_SMALL_MAMMALS);
    case SPELL_BOLT_OF_DRAINING:
        return (MS_NEGATIVE_BOLT);
    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:
        return (MS_CRYSTAL_SPEAR);
    case SPELL_BLINK:
        return (MS_BLINK);
    case SPELL_ISKENDERUNS_MYSTIC_BLAST:
        return (MS_ORB_ENERGY);
    case SPELL_SUMMON_HORRIBLE_THINGS:
        return (MS_LEVEL_SUMMON); /* approximate */
    case SPELL_SHADOW_CREATURES:
	return (MS_LEVEL_SUMMON); /* approximate */
    case SPELL_ANIMATE_DEAD:
        return (MS_ANIMATE_DEAD);
    case SPELL_PAIN:
        return (MS_PAIN);
    case SPELL_SUMMON_WRAITHS:
        return (MS_SUMMON_UNDEAD);        /* approximate */
    case SPELL_STICKY_FLAME:
        return (MS_STICKY_FLAME);
    case SPELL_CALL_IMP:
        return (MS_SUMMON_DEMON_LESSER);
    case SPELL_BANISHMENT:
        return (MS_BANISHMENT);
    case SPELL_STING:
        return (MS_STING);
    case SPELL_SUMMON_DEMON:
        return (MS_SUMMON_DEMON);
    case SPELL_DEMONIC_HORDE:
        return (MS_SUMMON_DEMON_LESSER);
    case SPELL_SUMMON_GREATER_DEMON:
        return (MS_SUMMON_DEMON_GREATER);
    case SPELL_BOLT_OF_IRON:
        return (MS_IRON_BOLT);
    case SPELL_STONE_ARROW:
        return (MS_STONE_ARROW);
    case SPELL_DISINTEGRATE:
        return (MS_DISINTEGRATE);
    case SPELL_AGONY:
        /* Too powerful to give ghosts Torment for Agony? Nah. */
        return (MS_TORMENT);
    case SPELL_SYMBOL_OF_TORMENT:
        return (MS_TORMENT);
    default:
        break;
    }

    return (MS_NO_SPELL);
}

void generate_random_demon(void)
{
    int rdem = 0;
    int i = 0;

    for (rdem = 0; rdem < MAX_MONSTERS + 1; rdem++)
    {
        if (rdem == MAX_MONSTERS)
            return;

        if (menv[rdem].type == MONS_PANDEMONIUM_DEMON)
            break;
    }

    char st_p[ITEMNAME_SIZE];

    make_name(random_int(), false, st_p);
    strcpy(ghost.name, st_p);

    // hp - could be defined below (as could ev, AC etc). Oh well, too late:
    ghost.values[ GVAL_MAX_HP ] = 100 + roll_dice( 3, 50 );

    ghost.values[ GVAL_EV ] = 5 + random2(20);
    ghost.values[ GVAL_AC ] = 5 + random2(20);

    ghost.values[ GVAL_SEE_INVIS ] = (one_chance_in(10) ? 0 : 1);

    if (!one_chance_in(3))
        ghost.values[ GVAL_RES_FIRE ] = (coinflip() ? 2 : 3);
    else
    {
        ghost.values[ GVAL_RES_FIRE ] = 0;    /* res_fire */

        if (one_chance_in(10))
            ghost.values[ GVAL_RES_FIRE ] = -1;
    }

    if (!one_chance_in(3))
        ghost.values[ GVAL_RES_COLD ] = 2;
    else
    {
        ghost.values[ GVAL_RES_COLD ] = 0;    /* res_cold */

        if (one_chance_in(10))
            ghost.values[ GVAL_RES_COLD ] = -1;
    }

    // demons, like ghosts, automatically get poison res. and life prot.

    // resist electricity:
    ghost.values[ GVAL_RES_ELEC ] = (!one_chance_in(3) ? 1 : 0);

    // HTH damage:
    ghost.values[ GVAL_DAMAGE ] = 20 + roll_dice( 2, 20 );

    // special attack type (uses weapon brand code):
    ghost.values[ GVAL_BRAND ] = SPWPN_NORMAL;

    if (!one_chance_in(3))
    {
        ghost.values[ GVAL_BRAND ] = random2(17);

        /* some brands inappropriate (eg holy wrath) */
        if (ghost.values[ GVAL_BRAND ] == SPWPN_HOLY_WRATH 
            || ghost.values[ GVAL_BRAND ] == SPWPN_ORC_SLAYING
            || ghost.values[ GVAL_BRAND ] == SPWPN_PROTECTION 
            || ghost.values[ GVAL_BRAND ] == SPWPN_FLAME 
            || ghost.values[ GVAL_BRAND ] == SPWPN_FROST 
            || ghost.values[ GVAL_BRAND ] == SPWPN_DISRUPTION)
        {
            ghost.values[ GVAL_BRAND ] = SPWPN_SPEED;
        }
    }

    // is demon a spellcaster?
    // upped from one_chance_in(3)... spellcasters are more interesting
    // and I expect named demons to typically have a trick or two -- bwr
    ghost.values[GVAL_DEMONLORD_SPELLCASTER] = (one_chance_in(10) ? 0 : 1);

    // does demon fly? (0 = no, 1 = fly, 2 = levitate)
    ghost.values[GVAL_DEMONLORD_FLY] = (one_chance_in(3) ? 0 : 
                                        one_chance_in(5) ? 2 : 1);

    // vacant <ghost best skill level>:
    ghost.values[GVAL_DEMONLORD_UNUSED] = 0;

    // hit dice:
    ghost.values[GVAL_DEMONLORD_HIT_DICE] = 10 + roll_dice(2, 10);

    // does demon cycle colours?
    ghost.values[GVAL_DEMONLORD_CYCLE_COLOUR] = (one_chance_in(10) ? 1 : 0);

    menv[rdem].hit_dice = ghost.values[ GVAL_DEMONLORD_HIT_DICE ];
    menv[rdem].hit_points = ghost.values[ GVAL_MAX_HP ];
    menv[rdem].max_hit_points = ghost.values[ GVAL_MAX_HP ];
    menv[rdem].armour_class = ghost.values[ GVAL_AC ];
    menv[rdem].evasion = ghost.values[ GVAL_EV ];
    menv[rdem].speed = (one_chance_in(3) ? 10 : 6 + roll_dice(2, 9));
    menv[rdem].speed_increment = 70;
    menv[rdem].number = random_colour();        // demon's colour

    for (i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
        ghost.values[i] = SPELL_NO_SPELL;

    /* This bit uses the list of player spells to find appropriate spells
       for the demon, then converts those spells to the monster spell indices.
       Some special monster-only spells are at the end. */
    if (ghost.values[ GVAL_DEMONLORD_SPELLCASTER ] == 1)
    {
        if (coinflip())
        {
            for (;;)
            {
                if (one_chance_in(3))
                    break;

                ghost.values[ GVAL_SPELL_1 ] = search_order_conj[i];
                i++;

                if (search_order_conj[i] == SPELL_NO_SPELL)
                    break;
            }
        } 

        if (coinflip())
        {
            for (;;)
            {
                if (one_chance_in(3))
                    break;

                ghost.values[ GVAL_SPELL_2 ] = search_order_conj[i];

                if (search_order_conj[i] == SPELL_NO_SPELL)
                    break;
            }
        }

        if (!one_chance_in(4))
        {
            for (;;)
            {
                if (one_chance_in(3))
                    break;

                ghost.values[ GVAL_SPELL_3 ] = search_order_third[i];
                i++;

                if (search_order_third[i] == SPELL_NO_SPELL)
                    break;
            }
        }

        if (coinflip())
        {
            for (;;)
            {
                if (one_chance_in(3))
                    break;

                ghost.values[ GVAL_SPELL_4 ] = search_order_misc[i];
                i++;

                if (search_order_misc[i] == SPELL_NO_SPELL)
                    break;
            }
        }

        if (coinflip())
        {
            for(;;)
            {
                if (one_chance_in(3))
                    break;

                ghost.values[ GVAL_SPELL_5 ] = search_order_misc[i];
                i++;

                if (search_order_misc[i] == SPELL_NO_SPELL)
                    break;
            }
        }

        if (coinflip())
            ghost.values[ GVAL_SPELL_6 ] = SPELL_BLINK;
        if (coinflip())
            ghost.values[ GVAL_SPELL_6 ] = SPELL_TELEPORT_SELF;

        /* Converts the player spell indices to monster spell ones */
        for (i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
            ghost.values[i] = translate_spell( ghost.values[i] );

        /* give demon a chance for some monster-only spells: */
        /* and demon-summoning should be fairly common: */
        if (one_chance_in(25))
            ghost.values[GVAL_SPELL_1] = MS_HELLFIRE_BURST;
        if (one_chance_in(25))
            ghost.values[GVAL_SPELL_1] = MS_METAL_SPLINTERS;
        if (one_chance_in(25))
            ghost.values[GVAL_SPELL_1] = MS_ENERGY_BOLT;  /* eye of devas */

        if (one_chance_in(25))
            ghost.values[GVAL_SPELL_2] = MS_STEAM_BALL;
        if (one_chance_in(25))
            ghost.values[GVAL_SPELL_2] = MS_PURPLE_BLAST;
        if (one_chance_in(25))
            ghost.values[GVAL_SPELL_2] = MS_HELLFIRE;

        if (one_chance_in(25))
            ghost.values[GVAL_SPELL_3] = MS_SMITE;
        if (one_chance_in(25))
            ghost.values[GVAL_SPELL_3] = MS_HELLFIRE_BURST;
        if (one_chance_in(12))
            ghost.values[GVAL_SPELL_3] = MS_SUMMON_DEMON_GREATER;
        if (one_chance_in(12))
            ghost.values[GVAL_SPELL_3] = MS_SUMMON_DEMON;

        if (one_chance_in(20))
            ghost.values[GVAL_SPELL_4] = MS_SUMMON_DEMON_GREATER;
        if (one_chance_in(20))
            ghost.values[GVAL_SPELL_4] = MS_SUMMON_DEMON;

        /* at least they can summon demons */
        if (ghost.values[17] == SPELL_NO_SPELL)
            ghost.values[GVAL_SPELL_4] = MS_SUMMON_DEMON;

        if (one_chance_in(15))
            ghost.values[GVAL_SPELL_5] = MS_DIG;
    }
}                               // end generate_random_demon()

// Largest string we'll save
#define STR_CAP 1000

void writeShort(FILE *file, short s)
{
    char data[2];
    // High byte first - network order
    data[0] = (char)((s >> 8) & 0xFF);
    data[1] = (char)(s & 0xFF);

    write2(file, data, sizeof(data));
}

short readShort(FILE *file)
{
    unsigned char data[2];
    read2(file, (char *) data, 2);

    // High byte first
    return (((short) data[0]) << 8) | (short) data[1];
}

void writeByte(FILE *file, unsigned char byte)
{
    write2(file, (char *) &byte, sizeof byte);
}

unsigned char readByte(FILE *file)
{
    unsigned char byte;
    read2(file, (char *) &byte, sizeof byte);
    return byte;
}

void writeString(FILE* file, const std::string &s)
{
    int length = s.length();
    if (length > STR_CAP) length = STR_CAP;
    writeShort(file, length);
    write2(file, s.c_str(), length);
}

std::string readString(FILE *file)
{
    char buf[STR_CAP + 1];
    short length = readShort(file);
    if (length)
        read2(file, buf, length);
    buf[length] = 0;
    return std::string(buf);
}

void writeLong(FILE* file, long num)
{
    // High word first, network order
    writeShort(file, (short) ((num >> 16) & 0xFFFFL));
    writeShort(file, (short) (num & 0xFFFFL));
}

long readLong(FILE *file)
{
    // We need the unsigned short cast even for the high word because we
    // might be on a system where long is more than 4 bytes, and we don't want
    // to sign extend the high short.
    return ((long) (unsigned short) readShort(file)) << 16 | 
        (long) (unsigned short) readShort(file);
}
