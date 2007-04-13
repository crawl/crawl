/*
 *  File:       cloud.cc
 *  Summary:    Functions related to clouds.
 *  Written by: Brent Ross
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Creating a cloud module so all the cloud stuff can be isolated.
 *
 *  Change History (most recent first):
 *
 *   <1>    Oct 1/2001   BWR    Created
 */

#include "AppHdr.h"
#include "externs.h"

#include "cloud.h"
#include "misc.h"
#include "stuff.h"

// Returns true if this cloud spreads out as it dissipates.
static bool cloud_spreads(const cloud_struct &cloud)
{
    switch (cloud.type)
    {
    case CLOUD_STEAM:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLACK_SMOKE:
        return (true);
    default:
        return (false);
    }
}

static void new_cloud( int cloud, int type, int x, int y, int decay,
                       kill_category whose )
{
    ASSERT( env.cloud[ cloud ].type == CLOUD_NONE );

    env.cloud[ cloud ].type = type;
    env.cloud[ cloud ].decay = decay;
    env.cloud[ cloud ].x = x;
    env.cloud[ cloud ].y = y;
    env.cloud[ cloud ].whose = whose;
    env.cgrid[ x ][ y ] = cloud;
    env.cloud_no++;
}

static void place_new_cloud(int cltype, int x, int y, int decay,
                            kill_category whose)
{
    if (env.cloud_no >= MAX_CLOUDS)
        return;

    // find slot for cloud
    for (int ci = 0; ci < MAX_CLOUDS; ci++)
    {
        if (env.cloud[ci].type == CLOUD_NONE)   // ie is empty
        {
            new_cloud( ci, cltype, x, y, decay, whose );
            break;
        }
    }
}

static int spread_cloud(const cloud_struct &cloud)
{
    const int spreadch = cloud.decay > 30? 80 :
                         cloud.decay > 20? 50 :
                                           30;
    int extra_decay = 0;

    for (int yi = -1; yi <= 1; ++yi)
    {
        for (int xi = -1; xi <= 1; ++xi)
        {
            if ((!xi && !yi) || random2(100) >= spreadch)
                continue;

            const int x = cloud.x + xi;
            const int y = cloud.y + yi;

            if (!in_bounds(x, y) 
                    || env.cgrid[x][y] != EMPTY_CLOUD
                    || grid_is_solid(grd[x][y]))
                continue;

            int newdecay = cloud.decay / 2 + 1;
            if (newdecay >= cloud.decay)
                newdecay = cloud.decay - 1;

            place_new_cloud( cloud.type, x, y, newdecay, cloud.whose );

            extra_decay += 8;
        }
    }

    return (extra_decay);
}

static void dissipate_cloud(int cc, cloud_struct &cloud, int dissipate)
{
    // apply calculated rate to the actual cloud:
    cloud.decay -= dissipate;

    if (cloud_spreads(cloud) && cloud.decay > 10 && one_chance_in(5))
        cloud.decay -= spread_cloud(cloud);

    // check for total dissipation and handle accordingly:
    if (cloud.decay < 1)
        delete_cloud( cc );
}

void manage_clouds(void)
{
    // amount which cloud dissipates - must be unsigned! {dlb}
    unsigned int dissipate = 0;

    for (unsigned char cc = 0; cc < MAX_CLOUDS; cc++)
    {
        if (env.cloud[cc].type == CLOUD_NONE)   // no cloud -> next iteration
            continue;

        dissipate = you.time_taken;

        // water -> flaming clouds:
        // lava -> freezing clouds:
        if (env.cloud[cc].type == CLOUD_FIRE
            && grd[env.cloud[cc].x][env.cloud[cc].y] == DNGN_DEEP_WATER)
        {
            dissipate *= 4;
        }
        else if (env.cloud[cc].type == CLOUD_COLD
                && grd[env.cloud[cc].x][env.cloud[cc].y] == DNGN_LAVA)
        {
            dissipate *= 4;
        }

        // double the amount when slowed - must be applied last(!):
        if (you.slow)
            dissipate *= 2;

        dissipate_cloud( cc, env.cloud[cc], dissipate );
    }

    return;
}                               // end manage_clouds()

void delete_cloud( int cloud )
{
    if (env.cloud[ cloud ].type != CLOUD_NONE)
    {
        const int cloud_x = env.cloud[ cloud ].x;
        const int cloud_y = env.cloud[ cloud ].y;

        env.cloud[ cloud ].type = CLOUD_NONE;
        env.cloud[ cloud ].decay = 0;
        env.cloud[ cloud ].x = 0;
        env.cloud[ cloud ].y = 0;
        env.cloud[ cloud ].whose = KC_OTHER;
        env.cgrid[ cloud_x ][ cloud_y ] = EMPTY_CLOUD;
        env.cloud_no--;
    }
}

// The current use of this function is for shifting in the abyss, so
// that clouds get moved along with the rest of the map. 
void move_cloud( int cloud, int new_x, int new_y )
{
    if (cloud != EMPTY_CLOUD)
    {
        const int old_x = env.cloud[ cloud ].x;
        const int old_y = env.cloud[ cloud ].y;

        env.cgrid[ new_x ][ new_y ] = cloud;
        env.cloud[ cloud ].x = new_x;
        env.cloud[ cloud ].y = new_y;
        env.cgrid[ old_x ][ old_y ] = EMPTY_CLOUD;
    }
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud( int cl_type, int x, int y, int lifetime,
                        kill_category whose )
{
    if (!in_bounds(x, y) || env.cgrid[x][y] != EMPTY_CLOUD)
        return;

    place_cloud( cl_type, x, y, lifetime, whose );
}

//   Places a cloud with the given stats. May delete old clouds to
//   make way if there are too many on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(int cl_type, int ctarget_x,
                 int ctarget_y, int cl_range,
                 kill_category whose)
{
    int cl_new = -1;

    // more compact {dlb}
    const int target_cgrid = env.cgrid[ctarget_x][ctarget_y];

    // that is, another cloud already there {dlb}
    if (target_cgrid != EMPTY_CLOUD)
    {
        if ((env.cloud[ target_cgrid ].type >= CLOUD_GREY_SMOKE
                && env.cloud[ target_cgrid ].type <= CLOUD_STEAM)
            || env.cloud[ target_cgrid ].type == CLOUD_STINK
            || env.cloud[ target_cgrid ].type == CLOUD_BLACK_SMOKE
            || env.cloud[ target_cgrid ].type == CLOUD_MIST
            || env.cloud[ target_cgrid ].decay <= 20)     //soon gone
        {
            cl_new = env.cgrid[ ctarget_x ][ ctarget_y ];
            delete_cloud( env.cgrid[ ctarget_x ][ ctarget_y ] );
        }
        else
        {
            return;
        }
    }

    // too many clouds
    if (env.cloud_no >= MAX_CLOUDS) 
    {
        // default to random in case there's no low quality clouds
        int cl_del = random2( MAX_CLOUDS );

        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            if ((env.cloud[ ci ].type >= CLOUD_GREY_SMOKE
                    && env.cloud[ ci ].type <= CLOUD_STEAM)
                || env.cloud[ ci ].type == CLOUD_STINK
                || env.cloud[ ci ].type == CLOUD_BLACK_SMOKE
                || env.cloud[ ci ].type == CLOUD_MIST
                || env.cloud[ ci ].decay <= 20)     //soon gone
            {
                cl_del = ci;
                break;
            }
        }

        delete_cloud( cl_del );
        cl_new = cl_del;
    }

    // create new cloud
    if (cl_new != -1)
        new_cloud( cl_new, cl_type, ctarget_x, ctarget_y, cl_range * 10,
                   whose );
    else
    {
        // find slot for cloud
        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            if (env.cloud[ci].type == CLOUD_NONE)   // ie is empty
            {
                new_cloud( ci, cl_type, ctarget_x, ctarget_y, cl_range * 10,
                           whose );
                break;
            }
        }
    }
}                               // end place_cloud();

bool is_opaque_cloud(unsigned char cloud_idx)
{
    if ( cloud_idx == EMPTY_CLOUD )
        return false;
    const int ctype = env.cloud[cloud_idx].type;
    return ( ctype == CLOUD_BLACK_SMOKE ||
             (ctype >= CLOUD_GREY_SMOKE && ctype <= CLOUD_STEAM) );
}
