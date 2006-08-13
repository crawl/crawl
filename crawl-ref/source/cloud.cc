/*
 *  File:       cloud.cc
 *  Summary:    Functions related to clouds.
 *  Written by: Brent Ross
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
#include "stuff.h"

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
        env.cgrid[ cloud_x ][ cloud_y ] = EMPTY_CLOUD;
        env.cloud_no--;
    }
}

static void new_cloud( int cloud, int type, int x, int y, int decay )
{
    ASSERT( env.cloud[ cloud ].type == CLOUD_NONE );

    env.cloud[ cloud ].type = type;
    env.cloud[ cloud ].decay = decay;
    env.cloud[ cloud ].x = x;
    env.cloud[ cloud ].y = y;
    env.cgrid[ x ][ y ] = cloud;
    env.cloud_no++;
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

//   Places a cloud with the given stats. May delete old clouds to make way
//   if there are too many (MAX_CLOUDS == 30) on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(unsigned char cl_type, unsigned char ctarget_x,
                 unsigned char ctarget_y, unsigned char cl_range)
{
    int cl_new = -1;

    // more compact {dlb}
    const unsigned char target_cgrid = env.cgrid[ctarget_x][ctarget_y];

    // that is, another cloud already there {dlb}
    if (target_cgrid != EMPTY_CLOUD)
    {
        if ((env.cloud[ target_cgrid ].type >= CLOUD_GREY_SMOKE
                && env.cloud[ target_cgrid ].type <= CLOUD_STEAM)
            || env.cloud[ target_cgrid ].type == CLOUD_STINK
            || env.cloud[ target_cgrid ].type == CLOUD_BLACK_SMOKE
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
        new_cloud( cl_new, cl_type, ctarget_x, ctarget_y, cl_range * 10 );
    else
    {
        // find slot for cloud
        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            if (env.cloud[ci].type == CLOUD_NONE)   // ie is empty
            {
                new_cloud( ci, cl_type, ctarget_x, ctarget_y, cl_range * 10 );
                break;
            }
        }
    }
}                               // end place_cloud();
