/**
 * @file
 * @brief Orb-related functionality.
**/

#include "AppHdr.h"

#include "orb.h"

#include "areas.h"
#include "god-passive.h" // passive_t::slow_orb_run
#include "shout.h"
#include "view.h"
#include "religion.h"
#include "message.h"
#include "xom.h"


/**
 * Make a "noise" caused by the orb.
 *
 * Alerts monsters via fake_noisy. If the area is silenced, perform a "flashing
 * lights" visual effect.
 *
 * @param where      The location the "noise" comes from.
 * @param loudness   The loudness of the noise, determining distance travelled.
 * @return           True if the area was unsilenced and thus an actual noise
 *                   occurred, otherwise false, denoting that flashing lights
 *                   occurred.
**/
static bool _orb_noise(const coord_def& where, int loudness)
{
    // XXX: Fake noisy doesn't work. Oops.
    fake_noisy(loudness, where);

    if (silenced(where))
    {
        flash_view_delay(UA_MONSTER, MAGENTA, 100);
        flash_view_delay(UA_MONSTER, LIGHTMAGENTA, 100);
        flash_view_delay(UA_MONSTER, MAGENTA, 100);
        flash_view_delay(UA_MONSTER, LIGHTMAGENTA, 100);

        return false;
    }

    return true;
}

/**
 * Make a "noise" upon picking up the orb.
 *
 * Calls orb_noise(where, loudness), and, depending on the result of that
 * function, prints one of two messages.
 *
 * @param where       Passed to orb_noise.
 * @param loudness    Passed to orb_noise.
 * @param msg         The message to be printed if orb_noise returned true.
 * @param msg2        The message to be printed if orb_noise returned false.
**/
void orb_pickup_noise(const coord_def& where, int loudness, const char* msg, const char* msg2)
{
    if (_orb_noise(where, loudness))
    {
        if (msg)
            mprf(MSGCH_ORB, "%s", msg);
        else
            mprf(MSGCH_ORB, "오브가 끔찍한 괴성을 지른다!");
    }
    else
    {
        if (msg2)
            mprf(MSGCH_ORB, "%s", msg2);
        else
            mprf(MSGCH_ORB, "오브가 강력한 빛을 맹렬히 쏟아낸다!");
    }
}

/**
 * Is the Orb interfering with translocations?
 * @return True if the player is in Zot or is carrying the Orb.
 */
bool orb_limits_translocation()
{
    return player_in_branch(BRANCH_ZOT) || player_has_orb();
}

void start_orb_run(game_chapter chapter, const char* message)
{
    if (you.chapter != CHAPTER_ANGERED_PANDEMONIUM)
    {
        mprf(MSGCH_WARN, "판데모니움 군주들이 적잖이 화났다. 주의하라!");
        if (have_passive(passive_t::slow_orb_run))
            simple_god_message("는 그들에게 서두르지 말라고 지시했다.");
    }

    mprf(MSGCH_ORB, "%s", message);
    you.chapter = chapter;
    xom_is_stimulated(200, XM_INTRIGUED);
    invalidate_agrid(true);
}
