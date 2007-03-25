/*
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include <vector>

#include "AppHdr.h"
#include "notes.h"

#include "branch.h"
#include "files.h"
#include "Kills.h"
#include "hiscores.h"
#include "message.h"
#include "misc.h"
#include "mon-pick.h"
#include "mutation.h"
#include "religion.h"
#include "skills2.h"
#include "spl-util.h"

#define NOTES_VERSION_NUMBER 1001

std::vector<Note> note_list;

// return the real number of the power (casting out nonexistent powers),
// starting from 0, or -1 if the power doesn't exist
static int real_god_power( int religion, int idx )
{
    if ( god_gain_power_messages[religion][idx][0] == 0 )
        return -1;
    int count = 0;
    for ( int j = 0; j < idx; ++j )
        if ( god_gain_power_messages[religion][j][0] )
            ++count;
    return count;
}

static bool is_noteworthy_skill_level( int level )
{
    unsigned i;
    for ( i = 0; i < Options.note_skill_levels.size(); ++i )
        if ( level == Options.note_skill_levels[i] )
            return true;
    return false;
}

static bool is_highest_skill( int skill )
{
    for ( int i = 0; i < NUM_SKILLS; ++i )
    {
        if ( i == skill )
            continue;
        if ( you.skills[i] >= you.skills[skill] )
            return false;
    }
    return true;
}

static bool is_noteworthy_hp( int hp, int maxhp )
{
    return (hp > 0 && Options.note_hp_percent &&
            hp <= (maxhp * Options.note_hp_percent) / 100);
}

static int dungeon_branch_depth( unsigned char branch )
{
    if ( branch >= NUM_BRANCHES )
        return -1;
    return branches[branch].depth;
}

static bool is_noteworthy_dlevel( unsigned short place )
{
    unsigned const char branch = (unsigned char) ((place >> 8) & 0xFF);
    const int lev = (place & 0xFF);

    /* Special levels (Abyss, etc.) are always interesting */
    if ( lev == 0xFF )
        return true;
    
    if ( lev == dungeon_branch_depth(branch) ||
         (branch == BRANCH_MAIN_DUNGEON && (lev % 5) == 0) ||
         (branch != BRANCH_MAIN_DUNGEON && lev == 1) )
        return true;

    return false;
}

/* Is a note worth taking?
   This function assumes that game state has not changed since
   the note was taken, e.g. you.* is valid.
 */
static bool is_noteworthy( const Note& note )
{
    /* always noteworthy */
    if ( note.type == NOTE_XP_LEVEL_CHANGE ||
         note.type == NOTE_GET_GOD ||
         note.type == NOTE_GOD_GIFT ||
         note.type == NOTE_GET_MUTATION ||
         note.type == NOTE_LOSE_MUTATION ||
         note.type == NOTE_SEEN_MONSTER ||
         note.type == NOTE_KILL_MONSTER ||
         note.type == NOTE_POLY_MONSTER ||
         note.type == NOTE_USER_NOTE ||
         note.type == NOTE_MESSAGE ||
         note.type == NOTE_LOSE_GOD ||
         note.type == NOTE_MOLLIFY_GOD )
        return true;
    
    /* never noteworthy, hooked up for fun or future use */
    if ( note.type == NOTE_GET_ITEM ||
         note.type == NOTE_MP_CHANGE ||
         note.type == NOTE_MAXHP_CHANGE ||
         note.type == NOTE_MAXMP_CHANGE )
        return false;

    /* god powers might be noteworthy if it's an actual power */
    if ( note.type == NOTE_GOD_POWER &&
         real_god_power(note.first, note.second) == -1 )
        return false;

    /* hp noteworthiness is handled in its own function */
    if ( note.type == NOTE_HP_CHANGE &&
         !is_noteworthy_hp(note.first, note.second) )
        return false;

    /* skills are noteworthy if in the skill value list or if
       it's a new maximal skill (depending on options) */
    if ( note.type == NOTE_GAIN_SKILL )
    {
        if ( is_noteworthy_skill_level(note.second) )
            return true;
        if ( Options.note_skill_max && is_highest_skill(note.first) )
            return true;
        return false;
    }

    if ( note.type == NOTE_DUNGEON_LEVEL_CHANGE )
    {
        if ( !is_noteworthy_dlevel(note.packed_place) )            
            return false;
        // labyrinths are always interesting
        if ( (note.packed_place & 0xFF) == 0xFF &&
             (note.packed_place >> 8) == LEVEL_LABYRINTH )
            return true;
    }
    
    /* Learning a spell is always noteworthy if note_all_spells is set */
    if ( note.type == NOTE_LEARN_SPELL && Options.note_all_spells )
        return true;

    for ( unsigned i = 0; i < note_list.size(); ++i )
    {
        if ( note_list[i].type != note.type )
            continue;
        const Note& rnote( note_list[i] );
        switch ( note.type )
        {
        case NOTE_DUNGEON_LEVEL_CHANGE:
            if ( rnote.packed_place == note.packed_place )
                return false;
            break;
        case NOTE_LEARN_SPELL:
            if (spell_difficulty(rnote.first) >= spell_difficulty(note.first))
                return false;
            break;
        case NOTE_GOD_POWER:
            if ( rnote.first == note.first && rnote.second == note.second )
                return false;
            break;
        case NOTE_ID_ITEM:
            /* re-id'ing an item, e.g. second copy of book, isn't
               noteworthy */
            if ( rnote.name == note.name )
                return false;
            break;
        case NOTE_HP_CHANGE:
            /* not if we have a recent warning */
            if ( (note.turn - rnote.turn < 5) &&
                 /* unless we've lost half our HP since then */
                 (note.first * 2 >= rnote.first) )
                return false;
            break;
        default:
          mpr("Buggy note passed: unknown note type");
          // Return now, rather than give a "Buggy note passed" message
          // for each note of the matching type in the note list.
          return true;
        }
    }
    return true;
}

const char* number_to_ordinal( int number )
{
    const char* ordinals[5] = { "first", "second", "third", "fourth",
                                "fifth" };
    if ( number < 1)
        return "[unknown ordinal (too small)]";
    if ( number > 5 )
        return "[unknown ordinal (too big)]";
    return ordinals[number-1];
}

std::string Note::describe( bool when, bool where, bool what ) const
{

    std::string result;

    if ( when )
    {
        char buf[20];
        snprintf(buf, sizeof buf, "| %5ld ", turn );
        result += buf;
    }
    if ( where )
    {
        result += "| ";
        std::string placename = short_place_name(packed_place);
        while ( placename.length() < 7 )
            placename += ' ';
        result += placename;
        result += " | ";
    }
    if ( what )
    {
        char buf[200];
        switch ( type )
        {
        case NOTE_HP_CHANGE:
            // [ds] Shortened HP change note from "Had X hitpoints" to
            // accommodate the cause for the loss of hitpoints.
            snprintf(buf, sizeof buf, "HP: %d/%d [%s]", 
                     first, second, name.c_str());
            break;
        case NOTE_MP_CHANGE:
            snprintf(buf, sizeof buf, "Mana: %d/%d", first, second);
            break;
        case NOTE_MAXHP_CHANGE:
            snprintf(buf, sizeof buf, "Reached %d max hit points", first);
            break;
        case NOTE_MAXMP_CHANGE:
            snprintf(buf, sizeof buf, "Reached %d max mana", first);
            break;
        case NOTE_XP_LEVEL_CHANGE:
            snprintf(buf, sizeof buf, "Reached XP level %d. %s", first, 
                     name.c_str());
            break;
        case NOTE_DUNGEON_LEVEL_CHANGE:
            snprintf(buf, sizeof buf, "Entered %s",
                     place_name(packed_place, true, true).c_str());
            break;
        case NOTE_LEARN_SPELL:
            snprintf(buf, sizeof buf, "Learned a level %d spell: %s",
                     spell_difficulty(first), spell_title(first));
            break;
        case NOTE_GET_GOD:
            snprintf(buf, sizeof buf, "Became a worshipper of %s",
                     god_name(first, true));
            break;
        case NOTE_LOSE_GOD:
            snprintf(buf, sizeof buf, "Fell from the grace of %s",
                     god_name(first));
            break;
        case NOTE_MOLLIFY_GOD:
            snprintf(buf, sizeof buf, "Was forgiven by %s",
                     god_name(first));
            break;
        case NOTE_GOD_GIFT:
            snprintf(buf, sizeof buf, "Received a gift from %s",
                     god_name(first));
            break;
        case NOTE_ID_ITEM:
            if (desc.length() > 0)
                snprintf(buf, sizeof buf, "Identified %s (%s)",
                         name.c_str(), desc.c_str());
            else
                snprintf(buf, sizeof buf, "Identified %s", name.c_str());
            break;
        case NOTE_GET_ITEM:
            snprintf(buf, sizeof buf, "Got %s", name.c_str());
            break;
        case NOTE_GAIN_SKILL:
            snprintf(buf, sizeof buf, "Reached skill %d in %s",
                     second, skill_name(first));
            break;
        case NOTE_SEEN_MONSTER:
            snprintf(buf, sizeof buf, "Noticed %s", name.c_str() );
            break;
        case NOTE_KILL_MONSTER:
            snprintf(buf, sizeof buf, "Defeated %s", name.c_str());
            break;
        case NOTE_POLY_MONSTER:
            snprintf(buf, sizeof buf, "%s changed form", name.c_str() );
            break;
        case NOTE_GOD_POWER:
            snprintf(buf, sizeof buf, "Acquired %s's %s power", 
                     god_name(first),
                     number_to_ordinal(real_god_power(first, second)+1));
            break;
        case NOTE_GET_MUTATION:
            snprintf(buf, sizeof buf, "Gained mutation: %s",
                     mutation_name(first, second == 0 ? 1 : second));
            break;
        case NOTE_LOSE_MUTATION:
            snprintf(buf, sizeof buf, "Lost mutation: %s",
                     mutation_name(first, second == 3 ? 3 : second+1));
            break;
        case NOTE_USER_NOTE:
            snprintf(buf, sizeof buf, "%s", name.c_str());
            break;
        case NOTE_MESSAGE:
            snprintf(buf, sizeof buf, "%s", name.c_str());
            break;
        default:
            snprintf(buf, sizeof buf,
                     "Buggy note description: unknown note type");
            break;
        }
        result += buf;
    }
    return result;
}

Note::Note()
{
    turn = you.num_turns;
    packed_place = get_packed_place();
}

Note::Note( NOTE_TYPES t, int f, int s, const char* n, const char* d ) :
    type(t), first(f), second(s)
{
    if (n)
        name = std::string(n);
    if (d)
        desc = std::string(d);
    turn = you.num_turns;
    packed_place = get_packed_place();
}

void Note::check_milestone() const
{
#ifdef DGL_MILESTONES
    if (type == NOTE_DUNGEON_LEVEL_CHANGE) {
        const int br = place_branch(packed_place),
            dep = place_depth(packed_place);

        if (br != -1)
        {
            std::string branch = place_name(packed_place, true, false).c_str();
            if (branch.find("The ") == 0)
                branch[0] = tolower(branch[0]);
            
            if (dep == 1)
                mark_milestone("enter", "entered " + branch + ".");
            else if (dep == dungeon_branch_depth(br))
            {
                char branch_finale[500];
                std::string level = place_name(packed_place, true, true);
                if (level.find("Level ") == 0)
                    level[0] = tolower(level[0]);
                
                snprintf(branch_finale, sizeof branch_finale,
                         "reached %s.", level.c_str());
                mark_milestone("branch-finale", branch_finale);
            }
        }
    }
#endif
}

void Note::save( FILE* fp ) const {
    writeLong( fp, type );
    writeLong( fp, turn );
    writeShort( fp, packed_place );
    writeLong( fp, first );
    writeLong( fp, second );
    writeString( fp, name );
    writeString( fp, desc );
}

void Note::load( FILE* fp )
{
    type = (NOTE_TYPES)(readLong( fp ));
    turn = readLong( fp );
    packed_place = readShort( fp );
    first = readLong( fp );
    second = readLong( fp );
    name = readString( fp );
    desc = readString( fp );
}

bool notes_active = false;

bool notes_are_active() 
{
    return notes_active;
}

void take_note( const Note& note, bool force )
{
    if ( notes_active && (force || is_noteworthy(note)) )
    {
        note_list.push_back( note );
        note.check_milestone();
    }
}

void activate_notes( bool active ) 
{
    notes_active = active;
}

void save_notes( FILE* fp ) 
{
    writeLong( fp, NOTES_VERSION_NUMBER );
    writeLong( fp, note_list.size() );
    for ( unsigned i = 0; i < note_list.size(); ++i )
        note_list[i].save(fp);
}

void load_notes( FILE* fp ) 
{
    if ( readLong(fp) != NOTES_VERSION_NUMBER )
        return;

    const long num_notes = readLong(fp);
    for ( long i = 0; i < num_notes; ++i )
    {
        Note new_note;
        new_note.load(fp);
        note_list.push_back(new_note);
    }
}

void make_user_note()
{
    mpr("Enter note: ", MSGCH_PROMPT);
    char buf[400];
    bool validline = !cancelable_get_line(buf, sizeof(buf));
    if ( !validline || (!*buf) )
        return;
    Note unote(NOTE_USER_NOTE);
    unote.name = std::string(buf);
    take_note(unote);
}
