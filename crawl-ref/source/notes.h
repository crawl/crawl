/*
 *  File:       notes.cc
 *  Summary:    Notetaking stuff
 *  Written by: Haran Pilpel
 *
 *  Change History (most recent first):
 *
 *     <1>     -/--/--     PH     Created
 */

#ifndef NOTES_H
#define NOTES_H

#include <string>
#include <vector>
#include <stdio.h>

enum NOTE_TYPES {
    NOTE_HP_CHANGE = 0,		/* needs: new hp, max hp */
    NOTE_MAXHP_CHANGE,		/* needs: new maxhp */
    NOTE_MP_CHANGE,		/* needs: new mp, max mp */
    NOTE_MAXMP_CHANGE,		/* needs: new maxmp */
    NOTE_XP_LEVEL_CHANGE,	/* needs: new xplevel */
    NOTE_DUNGEON_LEVEL_CHANGE,	/* needs: branch, subdepth */
    NOTE_LEARN_SPELL,		/* needs: spell idx */
    NOTE_GET_GOD,		/* needs: god id */
    NOTE_GOD_GIFT,		/* needs: god id */
    NOTE_GOD_POWER,		/* needs: god id, idx */
    NOTE_GET_MUTATION,		/* needs: mutation idx */
    NOTE_LOSE_MUTATION,		/* needs: mutation idx */
    NOTE_ID_ITEM,		/* needs: item name (string) */
    NOTE_GET_ITEM,		/* needs: item name (string) NOT HOOKED */
    NOTE_GAIN_SKILL,		/* needs: skill id, level */
    NOTE_SEEN_MONSTER,		/* needs: monster name (string) */
    NOTE_KILL_MONSTER,		/* needs: monster name (string) */
    NOTE_POLY_MONSTER,		/* needs: monster name (string) */
    NOTE_USER_NOTE,		/* needs: description string */
    NOTE_MESSAGE,               /* needs: message string */
    NOTE_LOSE_GOD,		/* needs: god id */
    NOTE_MOLLIFY_GOD,           /* needs: god id */
    NOTE_NUM_TYPES
};

struct Note {
    Note();
    Note( NOTE_TYPES t, int f = 0, int s = 0, const char* n = 0,
		  const char* d = 0);
    NOTE_TYPES type;
    int first, second;
    long turn;
    unsigned short packed_place;
    std::string name;
    std::string desc;
    void load( FILE* fp );
    void save( FILE* fp ) const;
    std::string describe( bool when = true, bool where = true,
                          bool what = true ) const;
};

extern std::vector<Note> note_list;
void activate_notes( bool active );
bool notes_are_active();
void take_note( const Note& note );
void save_notes( FILE* fp );
void load_notes( FILE* fp );
void make_user_note();

#endif
