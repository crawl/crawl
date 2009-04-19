/*
 *  File:       notes.cc
 *  Summary:    Notetaking stuff
 *  Written by: Haran Pilpel
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <vector>
#include <sstream>
#include <iomanip>

#include "notes.h"

#include "branch.h"
#include "cio.h"
#include "describe.h"
#include "files.h"
#include "Kills.h"
#include "hiscores.h"
#include "message.h"
#include "mon-pick.h"
#include "mutation.h"
#include "place.h"
#include "religion.h"
#include "skills2.h"
#include "spl-util.h"
#include "tags.h"

#define NOTES_VERSION_NUMBER 1002

std::vector<Note> note_list;

// return the real number of the power (casting out nonexistent powers),
// starting from 0, or -1 if the power doesn't exist
static int _real_god_power( int religion, int idx )
{
    if (god_gain_power_messages[religion][idx][0] == 0)
        return -1;

    int count = 0;
    for (int j = 0; j < idx; ++j)
        if (god_gain_power_messages[religion][j][0])
            ++count;

    return count;
}

static bool _is_noteworthy_skill_level( int level )
{
    for (unsigned int i = 0; i < Options.note_skill_levels.size(); ++i)
        if (level == Options.note_skill_levels[i])
            return (true);

    return (false);
}

static bool _is_highest_skill( int skill )
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (i == skill)
            continue;
        if (you.skills[i] >= you.skills[skill])
            return (false);
    }
    return (true);
}

static bool _is_noteworthy_hp( int hp, int maxhp )
{
    return (hp > 0 && Options.note_hp_percent
            && hp <= (maxhp * Options.note_hp_percent) / 100);
}

static int _dungeon_branch_depth( unsigned char branch )
{
    if (branch >= NUM_BRANCHES)
        return -1;
    return branches[branch].depth;
}

static bool _is_noteworthy_dlevel( unsigned short place )
{
    const unsigned char branch =
        static_cast<unsigned char>((place >> 8) & 0xFF);
    const int lev = (place & 0xFF);

    // Special levels (Abyss, etc.) are always interesting.
    if (lev == 0xFF)
        return (true);

    if (lev == _dungeon_branch_depth(branch)
        || branch == BRANCH_MAIN_DUNGEON && (lev % 5) == 0
        || branch != BRANCH_MAIN_DUNGEON && lev == 1)
    {
        return (true);
    }

    return (false);
}

// Is a note worth taking?
// This function assumes that game state has not changed since
// the note was taken, e.g. you.* is valid.
static bool _is_noteworthy( const Note& note )
{
    // Always noteworthy.
    if (note.type == NOTE_XP_LEVEL_CHANGE
        || note.type == NOTE_GET_GOD
        || note.type == NOTE_GOD_GIFT
        || note.type == NOTE_GET_MUTATION
        || note.type == NOTE_LOSE_MUTATION
        || note.type == NOTE_GET_ITEM
        || note.type == NOTE_ID_ITEM
        || note.type == NOTE_BUY_ITEM
        || note.type == NOTE_DONATE_MONEY
        || note.type == NOTE_SEEN_MONSTER
        || note.type == NOTE_KILL_MONSTER
        || note.type == NOTE_POLY_MONSTER
        || note.type == NOTE_USER_NOTE
        || note.type == NOTE_MESSAGE
        || note.type == NOTE_LOSE_GOD
        || note.type == NOTE_PENANCE
        || note.type == NOTE_MOLLIFY_GOD
        || note.type == NOTE_DEATH
        || note.type == NOTE_SEEN_FEAT)
    {
        return (true);
    }

    // Never noteworthy, hooked up for fun or future use.
    if (note.type == NOTE_MP_CHANGE
        || note.type == NOTE_MAXHP_CHANGE
        || note.type == NOTE_MAXMP_CHANGE)
    {
        return (false);
    }

    // Xom effects are only noteworthy if the option is true.
    if (note.type == NOTE_XOM_EFFECT)
        return (Options.note_xom_effects);

    // God powers might be noteworthy if it's an actual power.
    if (note.type == NOTE_GOD_POWER
        && _real_god_power(note.first, note.second) == -1)
    {
        return (false);
    }

    // HP noteworthiness is handled in its own function.
    if (note.type == NOTE_HP_CHANGE
        && !_is_noteworthy_hp(note.first, note.second))
    {
        return (false);
    }

    // Skills are noteworthy if in the skill value list or if
    // it's a new maximal skill (depending on options).
    if (note.type == NOTE_GAIN_SKILL)
    {
        if (Options.note_all_skill_levels
            || _is_noteworthy_skill_level(note.second)
            || Options.note_skill_max && _is_highest_skill(note.first))
        {
            return (true);
        }
        return (false);
    }

    if (note.type == NOTE_DUNGEON_LEVEL_CHANGE)
    {
        if (!_is_noteworthy_dlevel(note.packed_place))
            return (false);

        // Labyrinths and portal vaults are always interesting.
        if ((note.packed_place & 0xFF) == 0xFF
            && ((note.packed_place >> 8) == LEVEL_LABYRINTH
                || (note.packed_place >> 8) == LEVEL_PORTAL_VAULT))
        {
            return (true);
        }
    }

    // Learning a spell is always noteworthy if note_all_spells is set.
    if (note.type == NOTE_LEARN_SPELL && Options.note_all_spells)
        return (true);

    for (unsigned i = 0; i < note_list.size(); ++i)
    {
        if (note_list[i].type != note.type)
            continue;

        const Note& rnote( note_list[i] );
        switch (note.type)
        {
        case NOTE_DUNGEON_LEVEL_CHANGE:
            if (rnote.packed_place == note.packed_place)
                return (false);
            break;

        case NOTE_LEARN_SPELL:
            if (spell_difficulty(static_cast<spell_type>(rnote.first))
                >= spell_difficulty(static_cast<spell_type>(note.first)))
            {
                return (false);
            }
            break;

        case NOTE_GOD_POWER:
            if (rnote.first == note.first && rnote.second == note.second)
                return (false);
            break;

        case NOTE_HP_CHANGE:
            // Not if we have a recent warning
            // unless we've lost half our HP since then.
            if (note.turn - rnote.turn < 5
                && note.first * 2 >= rnote.first)
            {
                return (false);
            }
            break;

        default:
            mpr("Buggy note passed: unknown note type");
            // Return now, rather than give a "Buggy note passed" message
            // for each note of the matching type in the note list.
            return (true);
        }
    }
    return (true);
}

static const char* _number_to_ordinal( int number )
{
    const char* ordinals[5] = { "first", "second", "third", "fourth", "fifth" };

    if (number < 1)
        return "[unknown ordinal (too small)]";
    if (number > 5)
        return "[unknown ordinal (too big)]";
    return ordinals[number-1];
}

std::string Note::describe( bool when, bool where, bool what ) const
{

    std::ostringstream result;

    if (when)
        result << std::setw(6) << turn << " ";

    if (where)
    {
        if (!place_abbrev.empty())
            result << "| " << std::setw(MAX_NOTE_PLACE_LEN) << std::left
                   << place_abbrev << " | ";
        else
            result << "| " << std::setw(MAX_NOTE_PLACE_LEN) << std::left
                   << short_place_name(packed_place) << " | ";
    }

    if (what)
    {
        switch ( type )
        {
        case NOTE_HP_CHANGE:
            // [ds] Shortened HP change note from "Had X hitpoints" to
            // accommodate the cause for the loss of hitpoints.
            result << "HP: " << first << "/" << second
                   << " [" << name << "]";
            break;
        case NOTE_MP_CHANGE:
            result << "Mana: " << first << "/" << second;
            break;
        case NOTE_MAXHP_CHANGE:
            result << "Reached " << first << " max hit points";
            break;
        case NOTE_MAXMP_CHANGE:
            result << "Reached " << first << " max mana";
            break;
        case NOTE_XP_LEVEL_CHANGE:
            result << "Reached XP level " << first << ". " << name;
            break;
        case NOTE_DUNGEON_LEVEL_CHANGE:
            if ( !desc.empty() )
                result << desc;
            else
                result << "Entered " << place_name(packed_place, true, true);
            break;
        case NOTE_LEARN_SPELL:
            result << "Learned a level "
                   << spell_difficulty(static_cast<spell_type>(first))
                   << " spell: "
                   << spell_title(static_cast<spell_type>(first));
            break;
        case NOTE_GET_GOD:
            result << "Became a worshipper of "
                   << god_name(static_cast<god_type>(first), true);
            break;
        case NOTE_LOSE_GOD:
            result << "Fell from the grace of "
                   << god_name(static_cast<god_type>(first));
            break;
        case NOTE_PENANCE:
            result << "Was placed under penance by "
                   << god_name(static_cast<god_type>(first));
            break;
        case NOTE_MOLLIFY_GOD:
            result << "Was forgiven by "
                   << god_name(static_cast<god_type>(first));
            break;
        case NOTE_GOD_GIFT:
            result << "Received a gift from "
                   << god_name(static_cast<god_type>(first));
            break;
        case NOTE_ID_ITEM:
            result << "Identified " << name;
            if (!desc.empty())
                result << " (" << desc << ")";
            break;
        case NOTE_GET_ITEM:
            result << "Got " << name;
            break;
        case NOTE_BUY_ITEM:
            result << "Bought " << name << " for " << first << " gold piece"
                   << (first == 1 ? "" : "s");
            break;
        case NOTE_DONATE_MONEY:
            result << "Donated " << first << " gold piece"
                   << (first == 1 ? "" : "s") << " to Zin";
            break;
        case NOTE_GAIN_SKILL:
            result << "Reached skill " << second
                   << " in " << skill_name(first);
            break;
        case NOTE_SEEN_MONSTER:
            result << "Noticed " << name;
            break;
        case NOTE_KILL_MONSTER:
            if (second)
                result << name << " (ally) was defeated";
            else
                result << "Defeated " << name;
            break;
        case NOTE_POLY_MONSTER:
            result << name << " changed into " << desc;
            break;
        case NOTE_GOD_POWER:
            result << "Acquired "
                   << god_name(static_cast<god_type>(first)) << "'s "
                   << _number_to_ordinal(_real_god_power(first, second)+1)
                   << " power";
            break;
        case NOTE_GET_MUTATION:
            result << "Gained mutation: "
                   << mutation_name(static_cast<mutation_type>(first),
                                    second == 0 ? 1 : second);
            break;
        case NOTE_LOSE_MUTATION:
            result << "Lost mutation: "
                   << mutation_name(static_cast<mutation_type>(first),
                                    second == 3 ? 3 : second+1);
            break;
        case NOTE_DEATH:
            result << name;
            break;
        case NOTE_USER_NOTE:
            result << Options.user_note_prefix << name;
            break;
        case NOTE_MESSAGE:
            result << name;
            break;
        case NOTE_SEEN_FEAT:
            result << "Found " << name;
            break;
        case NOTE_XOM_EFFECT:
            result << "XOM: " << name;
#if defined(DEBUG_XOM) || defined(NOTE_DEBUG_XOM)
            // If debugging, also take note of piety and tension.
            result << " (piety: " << first;
            if (second >= 0)
                result << ", tension: " << second;
            result << ")";
#endif
            break;
        default:
            result << "Buggy note description: unknown note type";
            break;
        }
    }

    if (type == NOTE_SEEN_MONSTER || type == NOTE_KILL_MONSTER)
    {
        if (what && first == MONS_PANDEMONIUM_DEMON)
            result << " the pandemonium lord";
    }
    return result.str();
}

Note::Note()
{
    turn         = you.num_turns;
    packed_place = get_packed_place();

    if (you.level_type == LEVEL_PORTAL_VAULT)
        place_abbrev = you.level_type_name_abbrev;
}

Note::Note( NOTE_TYPES t, int f, int s, const char* n, const char* d ) :
    type(t), first(f), second(s), place_abbrev("")
{
    if (n)
        name = std::string(n);
    if (d)
        desc = std::string(d);

    turn         = you.num_turns;
    packed_place = get_packed_place();

    if (you.level_type == LEVEL_PORTAL_VAULT)
        place_abbrev = you.level_type_name_abbrev;
}

void Note::check_milestone() const
{
#ifdef DGL_MILESTONES
    if (type == NOTE_DUNGEON_LEVEL_CHANGE)
    {
        const int br = place_branch(packed_place),
                 dep = place_depth(packed_place);

        if (br != -1)
        {
            std::string branch = place_name(packed_place, true, false).c_str();
            if (branch.find("The ") == 0)
                branch[0] = tolower(branch[0]);

            if (dep == 1)
                mark_milestone("enter", "entered " + branch + ".");
            else if (dep == _dungeon_branch_depth(br))
            {
                std::string level = place_name(packed_place, true, true);
                if (level.find("Level ") == 0)
                    level[0] = tolower(level[0]);

                std::ostringstream branch_finale;
                branch_finale << "reached " << level << ".";
                mark_milestone("branch-finale", branch_finale.str());
            }
        }
    }
#endif
}

void Note::save(writer& outf) const
{
    marshallLong( outf, type );
    marshallLong( outf, turn );
    marshallShort( outf, packed_place );
    marshallLong( outf, first );
    marshallLong( outf, second );
    marshallString4( outf, name );
    marshallString4( outf, place_abbrev );
    marshallString4( outf, desc );
}

void Note::load(reader& inf)
{
    type = static_cast<NOTE_TYPES>(unmarshallLong( inf ));
    turn = unmarshallLong( inf );
    packed_place = unmarshallShort( inf );
    first  = unmarshallLong( inf );
    second = unmarshallLong( inf );
    unmarshallString4( inf, name );
    unmarshallString4( inf, place_abbrev );
    unmarshallString4( inf, desc );
}

bool notes_active = false;

bool notes_are_active()
{
    return (notes_active);
}

void take_note( const Note& note, bool force )
{
    if (notes_active && (force || _is_noteworthy(note)))
    {
        note_list.push_back( note );
        note.check_milestone();
    }
}

void activate_notes( bool active )
{
    notes_active = active;
}

void save_notes(writer& outf)
{
    marshallLong( outf, NOTES_VERSION_NUMBER );
    marshallLong( outf, note_list.size() );
    for (unsigned i = 0; i < note_list.size(); ++i)
        note_list[i].save(outf);
}

void load_notes(reader& inf)
{
    if ( unmarshallLong(inf) != NOTES_VERSION_NUMBER )
        return;

    const long num_notes = unmarshallLong(inf);
    for ( long i = 0; i < num_notes; ++i )
    {
        Note new_note;
        new_note.load(inf);
        note_list.push_back(new_note);
    }
}

void make_user_note()
{
    mpr("Enter note: ", MSGCH_PROMPT);
    char buf[400];
    bool validline = !cancelable_get_line(buf, sizeof(buf));
    if (!validline || (!*buf))
        return;
    Note unote(NOTE_USER_NOTE);
    unote.name = buf;
    take_note(unote);
}
