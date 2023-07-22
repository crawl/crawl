/**
 * @file
 * @brief Collection of tutorial related functions.
**/

#include "AppHdr.h"

#include "tutorial.h"

#include "hints.h"
#include "message.h"
#include "skills.h"
#include "state.h"


void set_tutorial_skill(const char *skill, int level)
{
    if (!crawl_state.game_is_tutorial())
        return;

    bool need_exercise_check = true;
    if (strcmp(skill, "spellcasting") == 0)
        you.skills[SK_SPELLCASTING] = level;
    else if (strcmp(skill, "conjurations") == 0)
        you.skills[SK_CONJURATIONS] = level;
    else if (strcmp(skill, "invocations") == 0)
        you.skills[SK_INVOCATIONS] = level;
    else
        need_exercise_check = false;

    if (need_exercise_check)
        reassess_starting_skills();
}

// FIXME: There's got to be a less hacky solution!
void tutorial_init_hint(const char* hintstr)
{
    hints_event_type hint = HINT_EVENTS_NUM;
    if (strcmp(hintstr, "HINT_NEW_LEVEL") == 0)
        hint = HINT_NEW_LEVEL;
    else if (strcmp(hintstr, "HINT_CHOOSE_STAT") == 0)
        hint = HINT_CHOOSE_STAT;
    else if (strcmp(hintstr, "HINT_MULTI_PICKUP") == 0)
        hint = HINT_MULTI_PICKUP;

    if (hint != HINT_EVENTS_NUM)
        Hints.hints_events[hint] = true;
}

// noreturn, but tutorial_msg doesn't guarantee it
void tutorial_death_message()
{
    canned_msg(MSG_YOU_DIE);
    more();

    tutorial_msg("tutorial death", true);
}
