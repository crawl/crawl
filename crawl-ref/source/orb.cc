/*
 * File:      orb.cc
 * Summary:   Orb-related functionality.
 */

#include "AppHdr.h"

#include "areas.h"
#include "message.h"
#include "orb.h"
#include "shout.h"
#include "view.h"

// Return true if it was an "actual" noise, flavour-wise.
bool orb_noise (const coord_def& where, int loudness)
{
    fake_noisy(loudness, where);

    if (silenced(where))
    {
        flash_view_delay(MAGENTA, 100);
        flash_view_delay(LIGHTMAGENTA, 100);
        flash_view_delay(MAGENTA, 100);
        flash_view_delay(LIGHTMAGENTA, 100);

        return (false);
    }

    return (true);
}

void orb_pickup_noise (const coord_def& where, int loudness, const char* msg, const char* msg2)
{
    if (orb_noise(where, loudness))
    {
        if (msg)
            mpr(msg, MSGCH_ORB);
        else
            mpr("The orb lets out a hideous shriek!", MSGCH_ORB);
    }
    else
    {
        if (msg2)
            mpr(msg2, MSGCH_ORB);
        else
            mpr("The orb lets out a furious burst of light!", MSGCH_ORB);
    }
}
