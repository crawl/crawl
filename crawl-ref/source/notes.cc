/**
 * @file
 * @brief Notetaking stuff
**/

#include "AppHdr.h"

#include "notes.h"

#include <iomanip>
#include <sstream>
#include <vector>

#include "branch.h"
#include "english.h"
#include "hiscores.h"
#include "message.h"
#include "mutation.h"
#include "options.h"
#include "religion.h"
#include "skills.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "unicode.h"

#define NOTES_VERSION_NUMBER 1002

vector<Note> note_list;

static bool _is_highest_skill(int skill)
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (i == skill)
            continue;
        if (you.skills[i] >= you.skills[skill])
            return false;
    }
    return true;
}

static bool _is_noteworthy_hp(int hp, int maxhp)
{
    return hp > 0 && Options.note_hp_percent
           && hp <= (maxhp * Options.note_hp_percent) / 100;
}

static int _dungeon_branch_depth(uint8_t branch)
{
    if (branch >= NUM_BRANCHES)
        return -1;
    return brdepth[branch];
}

static bool _is_noteworthy_dlevel(level_id place)
{
    branch_type branch = place.branch;
    int lev = place.depth;

    // Entering the Abyss is noted a different way, since we care mostly about
    // the cause.
    if (branch == BRANCH_ABYSS)
        return lev == _dungeon_branch_depth(branch);

    // These get their note in the .des files.
    if (branch == BRANCH_WIZLAB)
        return false;

    // Other portal levels are always interesting.
    if (!is_connected_branch(branch))
        return true;

    return lev == _dungeon_branch_depth(branch)
           || branch == BRANCH_DUNGEON && (lev % 5) == 0
           || branch != BRANCH_DUNGEON && lev == 1;
}

// Is a note worth taking?
// This function assumes that game state has not changed since
// the note was taken, e.g. you.* is valid.
static bool _is_noteworthy(const Note& note)
{
    // Always noteworthy.
    if (note.type == NOTE_XP_LEVEL_CHANGE
        || note.type == NOTE_LEARN_SPELL
        || note.type == NOTE_GET_GOD
        || note.type == NOTE_GOD_GIFT
        || note.type == NOTE_GET_MUTATION
        || note.type == NOTE_LOSE_MUTATION
        || note.type == NOTE_PERM_MUTATION
        || note.type == NOTE_GET_ITEM
        || note.type == NOTE_ID_ITEM
        || note.type == NOTE_BUY_ITEM
        || note.type == NOTE_DONATE_MONEY
        || note.type == NOTE_SEEN_MONSTER
        || note.type == NOTE_DEFEAT_MONSTER
        || note.type == NOTE_POLY_MONSTER
        || note.type == NOTE_USER_NOTE
        || note.type == NOTE_MESSAGE
        || note.type == NOTE_LOSE_GOD
        || note.type == NOTE_PENANCE
        || note.type == NOTE_MOLLIFY_GOD
        || note.type == NOTE_DEATH
        || note.type == NOTE_XOM_REVIVAL
        || note.type == NOTE_SEEN_FEAT
        || note.type == NOTE_PARALYSIS
        || note.type == NOTE_NAMED_ALLY
        || note.type == NOTE_ALLY_DEATH
        || note.type == NOTE_FEAT_MIMIC
        || note.type == NOTE_OFFERED_SPELL
        || note.type == NOTE_ANCESTOR_TYPE
        || note.type == NOTE_FOUND_UNRAND)
    {
        return true;
    }

    // Never noteworthy, hooked up for fun or future use.
    if (note.type == NOTE_MP_CHANGE
        || note.type == NOTE_MAXHP_CHANGE
        || note.type == NOTE_MAXMP_CHANGE)
    {
        return false;
    }

    // Xom effects are only noteworthy if the option is true.
    if (note.type == NOTE_XOM_EFFECT)
        return Options.note_xom_effects;

    // HP noteworthiness is handled in its own function.
    if (note.type == NOTE_HP_CHANGE
        && !_is_noteworthy_hp(note.first, note.second))
    {
        return false;
    }

    // Skills are always noteworthy in order to construct the skill_gains table
    // in the chardump. The options to control display are used in
    // Note::hidden().
    if (note.type == NOTE_GAIN_SKILL || note.type == NOTE_LOSE_SKILL)
        return true;

    if (note.type == NOTE_DUNGEON_LEVEL_CHANGE)
        return _is_noteworthy_dlevel(note.place);

    for (const Note &oldnote : note_list)
    {
        if (oldnote.type != note.type)
            continue;

        const Note& rnote(oldnote);
        switch (note.type)
        {
        case NOTE_PIETY_RANK:
            if (rnote.first == note.first && rnote.second == note.second)
                return false;
            break;

        case NOTE_HP_CHANGE:
            // Not if we have a recent warning
            // unless we've lost half our HP since then.
            if (note.turn - rnote.turn < 5
                && note.first * 2 >= rnote.first)
            {
                return false;
            }
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

string Note::describe(bool when, bool where, bool what) const
{
    ostringstream result;

    if (when)
        result << setw(6) << turn << " ";

    if (where)
    {
        result << "| "
               << chop_string(place.describe(), MAX_NOTE_PLACE_LEN)
               << " | ";
    }

    if (what)
    {
        switch (type)
        {
        case NOTE_HP_CHANGE:
            // [ds] Shortened HP change note from "Had X hitpoints" to
            // accommodate the cause for the loss of hitpoints.
            result << "HP: " << first << "/" << second
                   << " [" << name << "]";
            break;
        case NOTE_XOM_REVIVAL:
            result << "Xom revived you";
            break;
        case NOTE_MP_CHANGE:
            result << "Magic: " << first << "/" << second;
            break;
        case NOTE_MAXHP_CHANGE:
            result << "Reached " << first << " max health";
            break;
        case NOTE_MAXMP_CHANGE:
            result << "Reached " << first << " max magic points";
            break;
        case NOTE_XP_LEVEL_CHANGE:
            result << "Reached XP level " << first << ". " << name;
            break;
        case NOTE_DUNGEON_LEVEL_CHANGE:
            if (!desc.empty())
                result << desc;
            else
                result << "Entered "
                       << place.describe(true, true);
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
            if (!name.empty())
                result << " (" << name << ")";
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
            result << "Reached skill level " << second
                   << " in " << skill_name(static_cast<skill_type>(first));
            break;
        case NOTE_LOSE_SKILL:
            result << "Reduced skill "
                   << skill_name(static_cast<skill_type>(first))
                   << " to level " << second;
            break;
        case NOTE_SEEN_MONSTER:
            result << "Noticed " << name;
            break;
        case NOTE_DEFEAT_MONSTER:
            if (second)
                result << name << " (ally) was " << desc;
            else
                result << uppercase_first(desc) << " " << name;
            break;
        case NOTE_POLY_MONSTER:
            result << name << " changed into " << desc;
            break;
        case NOTE_PIETY_RANK:
            result << "Reached "
                   << string(second, '*')
                   << " piety under "
                   << god_name(static_cast<god_type>(first));
            break;
        case NOTE_GET_MUTATION:
            result << "Gained mutation: "
                   << mutation_desc(static_cast<mutation_type>(first),
                                    second == 0 ? 1 : second);
            if (!name.empty())
                result << " [" << name << "]";
            break;
        case NOTE_LOSE_MUTATION:
            result << "Lost mutation: "
                   << mutation_desc(static_cast<mutation_type>(first),
                                    second == 3 ? 3 : second+1);
            if (!name.empty())
                result << " [" << name << "]";
            break;
        case NOTE_PERM_MUTATION:
            result << "Mutation became permanent: "
                   << mutation_desc(static_cast<mutation_type>(first),
                                    second == 0 ? 1 : second);
            if (!name.empty())
                result << " [" << name << "]";
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
        case NOTE_FEAT_MIMIC:
            result << name <<" was a mimic.";
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
        case NOTE_PARALYSIS:
            result << "Paralysed by " << name << " for " << first << " turns";
            break;
        case NOTE_NAMED_ALLY:
            result << "Gained " << name << " as an ally";
            break;
        case NOTE_ALLY_DEATH:
            result << "Your ally " << name << " died";
            break;
        case NOTE_OFFERED_SPELL:
            result << "Offered knowledge of "
                   << spell_title(static_cast<spell_type>(first))
                   << " by Vehumet.";
            break;
        case NOTE_ANCESTOR_TYPE:
            result << "Remembered your ancestor " << hepliaklqana_ally_name()
                   << " as " << name;
            break;
#if TAG_MAJOR_VERSION == 34
        case NOTE_ANCESTOR_SPECIALIZATION:
            result << "Remembered your ancestor " << hepliaklqana_ally_name()
                   << " " << name;
            break;
        case NOTE_ANCESTOR_DEATH:
            result << "Remembered your ancestor "
                   << apostrophise(hepliaklqana_ally_name())
                   << " " << name << " death";
            break;
#endif
          case NOTE_FOUND_UNRAND:
            result << "Found " << name;
            break;
        default:
            result << "Buggy note description: unknown note type";
            break;
        }
    }

    if (type == NOTE_SEEN_MONSTER || type == NOTE_DEFEAT_MONSTER)
    {
        if (what && first == MONS_PANDEMONIUM_LORD)
            result << " the pandemonium lord";
    }
    return result.str();
}

bool Note::hidden() const
{
    // Hide skill gains that are not enabled by options.
    if (type == NOTE_GAIN_SKILL || type == NOTE_LOSE_SKILL)
    {
        return !(Options.note_all_skill_levels
                 || second <= 27 && Options.note_skill_levels[second]
                 || Options.note_skill_max && _is_highest_skill(first));
    }
    return false;
}

void Note::check_milestone() const
{
    if (crawl_state.game_is_arena())
        return;

    if (type == NOTE_DUNGEON_LEVEL_CHANGE)
    {
        const int br = place.branch,
                 dep = place.depth;

        // Wizlabs report their milestones on their own.
        if (br != -1 && br != BRANCH_WIZLAB)
        {
            ASSERT_RANGE(br, 0, NUM_BRANCHES);
            string branch = place.describe(true, false);

            if (starts_with(branch, "The "))
                branch[0] = tolower(branch[0]);

            if (dep == 1)
            {
                mark_milestone(br == BRANCH_ZIGGURAT ? "zig.enter" : "br.enter",
                               "entered " + branch + ".", "parent");
            }
            else if (dep == _dungeon_branch_depth(br)
                     || br == BRANCH_ZIGGURAT)
            {
                string level = place.describe(true, true);
                if (starts_with(level, "Level "))
                    level[0] = tolower(level[0]);

                mark_milestone(br == BRANCH_ZIGGURAT ? "zig" : "br.end",
                               "reached " + level + ".");
            }
        }
    }
}

void Note::save(writer& outf) const
{
    marshallInt(outf, type);
    marshallInt(outf, turn);
    place.save(outf);
    marshallInt(outf, first);
    marshallInt(outf, second);
    marshallString4(outf, name);
    marshallString4(outf, desc);
}

void Note::load(reader& inf)
{
    type = static_cast<NOTE_TYPES>(unmarshallInt(inf));
    turn = unmarshallInt(inf);
#if TAG_MAJOR_VERSION == 34
    if (inf.getMinorVersion() < TAG_MINOR_PLACE_UNPACK)
        place = level_id::from_packed_place(unmarshallShort(inf));
    else
#endif
    place.load(inf);
    first  = unmarshallInt(inf);
    second = unmarshallInt(inf);
    unmarshallString4(inf, name);
    unmarshallString4(inf, desc);
}

static bool notes_active = false;

bool notes_are_active()
{
    return notes_active;
}

void take_note(const Note& note, bool force)
{
    if (notes_active && (force || _is_noteworthy(note)))
    {
        note_list.push_back(note);
        note.check_milestone();
    }
}

void activate_notes(bool active)
{
    notes_active = active;
}

void save_notes(writer& outf)
{
    marshallInt(outf, NOTES_VERSION_NUMBER);
    marshallInt(outf, note_list.size());
    for (const Note &note : note_list)
        note.save(outf);
}

void load_notes(reader& inf)
{
    if (unmarshallInt(inf) != NOTES_VERSION_NUMBER)
        return;

    const int num_notes = unmarshallInt(inf);
    for (int i = 0; i < num_notes; ++i)
    {
        Note new_note;
        new_note.load(inf);
        note_list.push_back(new_note);
    }
}

void make_user_note()
{
    char buf[400];
    bool validline = !msgwin_get_line("Enter note: ", buf, sizeof(buf));
    if (!validline || (!*buf))
        return;
    Note unote(NOTE_USER_NOTE);
    unote.name = buf;
    take_note(unote);
}
