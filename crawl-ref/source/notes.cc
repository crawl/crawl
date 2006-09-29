/*
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include <vector>

#include "AppHdr.h"
#include "notes.h"

#include "files.h"
#include "Kills.h"
#include "message.h"
#include "mon-pick.h"
#include "mutation.h"
#include "religion.h"
#include "skills2.h"
#include "spl-util.h"
#include "stash.h"

std::vector<Note> note_list;

/* I can't believe I'm writing code this bad */
static int real_god_power( int religion, int idx ) {
    switch ( religion ) {
    case GOD_NO_GOD:
    case GOD_XOM:
    case GOD_NEMELEX_XOBEH:
	return -1;
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_YREDELEMNUL:
    case GOD_MAKHLEB:	
    case GOD_ELYVILON:
	return idx;
    case GOD_KIKUBAAQUDGHA:
	if ( idx < 3 )
	    return idx;
	if ( idx == 3 )
	    return -1;
	return idx-1;	
    case GOD_VEHUMET:
	return ( idx > 3 ? -1 : idx );
    case GOD_OKAWARU:
	if ( idx < 2 )
	    return idx;
	if ( idx == 2 || idx == 3 )
	    return -1;
	return idx - 2;
    case GOD_SIF_MUNA:
	if ( idx == 2 || idx == 4 ) return -1;
	if ( idx < 2 ) return idx;
	if ( idx == 3 ) return 2;
    case GOD_TROG:
	if ( idx == 2 || idx == 4 ) return -1;
	if ( idx < 2 ) return idx;
	if ( idx == 3 ) return idx-1;
    default:
	return -1;
    }
}

static bool is_noteworthy_skill_level( int level ) {
    unsigned i;
    for ( i = 0; i < Options.note_skill_levels.size(); ++i )
	if ( level == Options.note_skill_levels[i] )
	    return true;
    return false;
}

static bool is_highest_skill( int skill ) {
    for ( int i = 0; i < NUM_SKILLS; ++i ) {
	if ( i == skill )
	    continue;
	if ( you.skills[i] >= you.skills[skill] )
	    return false;
    }
    return true;
}

static bool is_noteworthy_hp( int hp, int maxhp ) {
    return (hp > 0 && Options.note_hp_percent &&
	    hp <= (maxhp * Options.note_hp_percent) / 100);
}

static int dungeon_branch_depth( unsigned char branch ) {
    if ( branch > BRANCH_CAVERNS ) // last branch
	return -1;
    switch ( branch ) {
    case BRANCH_MAIN_DUNGEON:
	return 27;
    case BRANCH_DIS:
    case BRANCH_GEHENNA:
    case BRANCH_COCYTUS:
    case BRANCH_TARTARUS:
	return 7;
    case BRANCH_VESTIBULE_OF_HELL:
	return 1;
    case BRANCH_INFERNO:
    case BRANCH_THE_PIT:
	return -1;
    default:
	return branch_depth( branch - 10 );
    }
}

static bool is_noteworthy_dlevel( unsigned short place ) {
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
static bool is_noteworthy( const Note& note ) {

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
	 note.type == NOTE_LOSE_GOD )
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
    if ( note.type == NOTE_GAIN_SKILL ) {
	if ( is_noteworthy_skill_level(note.second) )
	    return true;
	if ( Options.note_skill_max && is_highest_skill(note.first) )
	    return true;
	return false;
    }
    
    if ( note.type == NOTE_DUNGEON_LEVEL_CHANGE &&
	 !is_noteworthy_dlevel(note.packed_place) )
	return false;
    
    /* Learning a spell is always noteworthy if note_all_spells is set */
    if ( note.type == NOTE_LEARN_SPELL && Options.note_all_spells )
	return true;


    unsigned i;
    for ( i = 0; i < note_list.size(); ++i ) {
	if ( note_list[i].type != note.type )
	    continue;
	const Note& rnote( note_list[i] );
	switch ( note.type ) {
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
    } // for ( i = 0; i < note_list.size(); ++i )
    return true;
}

const char* number_to_ordinal( int number ) {
    const char* ordinals[5] = { "first", "second", "third", "fourth",
				"fifth" };
    if ( number < 1)
	return "[unknown ordinal (too small)]";
    if ( number > 5 )
	return "[unknown ordinal (too big)]";
    return ordinals[number-1];
}

std::string describe_note( const Note& note ) {
    char buf[200], buf2[50];

    switch ( note.type ) {
    case NOTE_HP_CHANGE:
	sprintf(buf, "Had %d/%d hit points", note.first, note.second);
	break;
    case NOTE_MP_CHANGE:
	sprintf(buf, "Had %d/%d mana", note.first, note.second);
	break;
    case NOTE_MAXHP_CHANGE:
	sprintf(buf, "Reached %d max hit points", note.first);
	break;
    case NOTE_MAXMP_CHANGE:
	sprintf(buf, "Reached %d max mana", note.first);
	break;
    case NOTE_XP_LEVEL_CHANGE:
	sprintf(buf, "Reached XP level %d. %s", note.first, note.name.c_str());
	break;
    case NOTE_DUNGEON_LEVEL_CHANGE:
	sprintf(buf, "Entered %s",
		branch_level_name(note.packed_place).c_str());
	break;
    case NOTE_LEARN_SPELL:
	sprintf(buf, "Learned a level %d spell: %s",
		spell_difficulty(note.first), spell_title(note.first));
	break;
    case NOTE_GET_GOD:
	sprintf(buf, "Became a worshipper of %s",
		god_name(note.first, true));
	break;
    case NOTE_LOSE_GOD:
	sprintf(buf, "Fell from the grace of %s",
		god_name(note.first));
	break;
    case NOTE_GOD_GIFT:
	sprintf(buf, "Received a gift from %s",
		god_name(note.first));
	break;
    case NOTE_ID_ITEM:
        if (note.desc.length() > 0)
            sprintf(buf, "Identified %s (%s)", note.name.c_str(),
                    note.desc.c_str());
        else
	    sprintf(buf, "Identified %s", note.name.c_str());
	break;
    case NOTE_GET_ITEM:
	sprintf(buf, "Got %s", note.name.c_str());
	break;
    case NOTE_GAIN_SKILL:
	sprintf(buf, "Reached skill %d in %s",
		note.second, skill_name(note.first));
	break;
    case NOTE_SEEN_MONSTER:
        sprintf(buf, "Noticed %s", note.name.c_str() );
	break;
    case NOTE_KILL_MONSTER:
        sprintf(buf, "Defeated %s", note.name.c_str());
	break;
    case NOTE_POLY_MONSTER:
        sprintf(buf, "%s changed form", note.name.c_str() );
	break;
    case NOTE_GOD_POWER:
	sprintf(buf, "Acquired %s's %s power", god_name(note.first),
		number_to_ordinal(real_god_power(note.first, note.second)+1));
	break;
    case NOTE_GET_MUTATION:
	sprintf(buf, "Gained mutation: %s",
		mutation_name(note.first, note.second == 0 ? 1 : note.second));
	break;
    case NOTE_LOSE_MUTATION:
	sprintf(buf, "Lost mutation: %s",
		mutation_name(note.first,
			      note.second == 3 ? 3 : note.second+1));
	break;
    case NOTE_USER_NOTE:
	sprintf(buf, "%s", note.name.c_str());
	break;
    case NOTE_MESSAGE:
	sprintf(buf, "%s", note.name.c_str());
	break;
    default:
	sprintf(buf, "Buggy note description: unknown note type");
	break;
    }
    sprintf(buf2, "| %5ld | ", note.turn );
    std::string placename = short_place_name(note.packed_place);
    while ( placename.length() < 7 )
	placename += ' ';
    return std::string(buf2) + placename + std::string(" | ") +
	std::string(buf);
}

Note::Note() {
    turn = you.num_turns;
    packed_place = get_packed_place();
}

Note::Note( NOTE_TYPES t, int f, int s, const char* n, const char* d ) :
    type(t), first(f), second(s) {
    if (n)
	name = std::string(n);
    if (d)
        desc = std::string(d);
    turn = you.num_turns;
    packed_place = get_packed_place();
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

void Note::load( FILE* fp ) {
    type = (NOTE_TYPES)(readLong( fp ));
    turn = readLong( fp );
    packed_place = readShort( fp );
    first = readLong( fp );
    second = readLong( fp );
    name = readString( fp );
    desc = readString( fp );
}

bool notes_active = false;

bool notes_are_active() {
    return notes_active;
}

void take_note( const Note& note ) {
    if ( notes_active && is_noteworthy( note ) )
	note_list.push_back( note );
}

void activate_notes( bool active ) {
    notes_active = active;
}

void save_notes( FILE* fp ) {
    writeLong( fp, note_list.size() );
    for ( unsigned i = 0; i < note_list.size(); ++i )
	note_list[i].save(fp);
}

void load_notes( FILE* fp ) {
    long num_notes = readLong(fp);
    for ( long i = 0; i < num_notes; ++i ) {
	Note new_note;
	new_note.load(fp);
	note_list.push_back(new_note);
    }
}

void make_user_note() {
    mpr("Enter note: ", MSGCH_PROMPT);
    char buf[400];
    bool validline = cancelable_get_line(buf, sizeof(buf));
    if ( !validline || (!*buf) )
	return;
    Note unote(NOTE_USER_NOTE);
    unote.name = std::string(buf);
    take_note(unote);
}
