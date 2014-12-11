/**
 * @file
 * @brief Notetaking stuff
**/

#ifndef NOTES_H
#define NOTES_H

#include <string>
#include <vector>
#include <cstdio>

#define MAX_NOTE_PLACE_LEN 8

class reader;
class writer;

enum NOTE_TYPES
{
    NOTE_HP_CHANGE = 0,         /* needs: new hp, max hp */
    NOTE_MAXHP_CHANGE,          /* needs: new maxhp */
    NOTE_MP_CHANGE,             /* needs: new mp, max mp */
    NOTE_MAXMP_CHANGE,          /* needs: new maxmp */
    NOTE_XP_LEVEL_CHANGE,       /* needs: new xplevel */
    NOTE_DUNGEON_LEVEL_CHANGE,  /* needs: branch, subdepth */
    NOTE_LEARN_SPELL,           /* needs: spell idx */
    NOTE_GET_GOD,               /* needs: god id */
    NOTE_GOD_GIFT,              /* needs: god id */
    NOTE_GOD_POWER,             /* needs: god id, idx */
    NOTE_GET_MUTATION,          /* needs: mutation idx, reason (string) */
    NOTE_LOSE_MUTATION,         /* needs: mutation idx, reason (string) */
    NOTE_ID_ITEM,               /* needs: item name (string) */
    NOTE_GET_ITEM,              /* needs: item name (string) */
    NOTE_GAIN_SKILL,            /* needs: skill id, level */
    NOTE_LOSE_SKILL,            /* needs: skill id, level */
    NOTE_SEEN_MONSTER,          /* needs: monster name (string) */
    NOTE_DEFEAT_MONSTER,        /* needs: monster name, defeat verb (strings) */
    NOTE_POLY_MONSTER,          /* needs: monster name (string) */
    NOTE_USER_NOTE,             /* needs: description string */
    NOTE_MESSAGE,               /* needs: message string */
    NOTE_LOSE_GOD,              /* needs: god id */
    NOTE_PENANCE,               /* needs: god id */
    NOTE_MOLLIFY_GOD,           /* needs: god id */
    NOTE_DEATH,                 /* needs: death cause */
    NOTE_BUY_ITEM,              /* needs: item name (string), price (int) */
    NOTE_DONATE_MONEY,          /* needs: amount of gold */
    NOTE_SEEN_FEAT,             /* needs: feature seen (string) */
    NOTE_XOM_EFFECT,            /* needs: description (name string) */
    NOTE_XOM_REVIVAL,           /* needs: death cause (string) */
    NOTE_PARALYSIS,             /* needs: paralysis source (string) */
    NOTE_NAMED_ALLY,            /* needs: ally name (string) */
    NOTE_ALLY_DEATH,            /* needs: ally name (string) */
    NOTE_FEAT_MIMIC,            /* needs: mimiced feature (string) */
    NOTE_OFFERED_SPELL,         /* needs: spell idx */
    NOTE_PERM_MUTATION,         /* needs: mutation idx, reason (string) */
    NOTE_NUM_TYPES
};

struct Note
{
    Note();
    Note(NOTE_TYPES t, int f = 0, int s = 0, const char* n = 0,
          const char* d = 0);
    void save(writer& outf) const;
    void load(reader& inf);
    string describe(bool when = true, bool where = true, bool what = true) const;
    void check_milestone() const;

    NOTE_TYPES type;
    int first, second;
    int turn;
    level_id place;

    string name;
    string desc;
};

extern vector<Note> note_list;
void activate_notes(bool active);
bool notes_are_active();
void take_note(const Note& note, bool force = false);
void save_notes(writer&);
void load_notes(reader&);
void make_user_note();

#endif
