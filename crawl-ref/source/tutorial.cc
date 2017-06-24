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


void set_tutorial_hunger(int hunger)
{
    if (!crawl_state.game_is_tutorial())
        return;

    you.hunger = hunger;
}

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
    else if (strcmp(hintstr, "HINT_YOU_CURSED") == 0)
        hint = HINT_YOU_CURSED;
    else if (strcmp(hintstr, "HINT_REMOVED_CURSE") == 0)
        hint = HINT_REMOVED_CURSE;
    else if (strcmp(hintstr, "HINT_MULTI_PICKUP") == 0)
        hint = HINT_MULTI_PICKUP;

    if (hint != HINT_EVENTS_NUM)
        Hints.hints_events[hint] = true;
}

void tutorial_death_message()
{
    canned_msg(MSG_YOU_DIE);
    mpr_nojoin(MSGCH_TUTORIAL,
               "크롤의 세계에서는, 죽음은 슬픈 일이지만 흔한 일이기도 하다. "
               "기억해라. 대부분의 상황에서 당신은 살 수 있는 길을 찾을 수 있다. "
               "예를 들자면 물건을 사용하거나, 도망가거나, 휴식을 취하거나, "
               "아니면 완전히 전투를 피할 수도 있을 것이다. "
               "노력하고 또 노력해라. 그러면 언젠가는 승리를 쟁취할 수 있을 것이다!");
    more();
}
