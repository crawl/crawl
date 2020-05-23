/**
 * @file
 * @brief Mini UI for choosing a compass direction around the player.
**/

#include "AppHdr.h"

#include "coord.h"
#include "directn.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "state.h"
#include "target-compass.h"
#ifdef USE_TILE_LOCAL
# include "tilesdl.h"
#endif
#ifdef USE_TILE_WEB
# include "tileweb.h"
#endif

static int _targeting_cmd_to_compass(command_type command)
{
    switch (command)
    {
    case CMD_TARGET_UP:         case CMD_TARGET_DIR_UP:
        return 0;
    case CMD_TARGET_UP_RIGHT:   case CMD_TARGET_DIR_UP_RIGHT:
        return 1;
    case CMD_TARGET_RIGHT:      case CMD_TARGET_DIR_RIGHT:
        return 2;
    case CMD_TARGET_DOWN_RIGHT: case CMD_TARGET_DIR_DOWN_RIGHT:
        return 3;
    case CMD_TARGET_DOWN:       case CMD_TARGET_DIR_DOWN:
        return 4;
    case CMD_TARGET_DOWN_LEFT:  case CMD_TARGET_DIR_DOWN_LEFT:
        return 5;
    case CMD_TARGET_LEFT:       case CMD_TARGET_DIR_LEFT:
        return 6;
    case CMD_TARGET_UP_LEFT:    case CMD_TARGET_DIR_UP_LEFT:
        return 7;
    default:
        return -1;
    }
}

static int targeting_behaviour_get_key()
{
    if (!crawl_state.is_replaying_keys())
        flush_input_buffer(FLUSH_BEFORE_COMMAND);

    flush_prev_message();
    msgwin_got_input();
    return unmangle_direction_keys(getchm(KMC_TARGETING), KMC_TARGETING,
                                   false);
}

coord_def prompt_compass_direction()
{
    mprf(MSGCH_PROMPT, "Which direction?");

    coord_def delta = {0, 0};
    bool cancel = false;

    mouse_control mc(MOUSE_MODE_TARGET_DIR);

    do
    {
        const auto key = targeting_behaviour_get_key();
        const auto cmd = key_to_command(key, KMC_TARGETING);

        if (crawl_state.seen_hups)
        {
            mprf(MSGCH_ERROR, "Targeting interrupted by HUP signal.");
            return {0, 0};
        }

#ifdef USE_TILE
        if (cmd == CMD_TARGET_MOUSE_MOVE)
            continue;
        else if (cmd == CMD_TARGET_MOUSE_SELECT)
        {
            const coord_def &gc = tiles.get_cursor();
            if (gc == NO_CURSOR)
                continue;

            if (!map_bounds(gc))
                continue;

            delta = gc - you.pos();
            if (delta.rdist() > 1)
            {
                tiles.place_cursor(CURSOR_MOUSE, gc);
                delta = tiles.get_cursor() - you.pos();
                ASSERT(delta.rdist() <= 1);
            }
            break;
        }
#endif

        if (cmd == CMD_TARGET_SELECT)
        {
            delta.reset();
            break;
        }

        const int i = _targeting_cmd_to_compass(cmd);
        if (i != -1)
            delta = Compass[i];
        else if (cmd == CMD_TARGET_CANCEL)
            cancel = true;
    }
    while (!cancel && delta.origin());

#ifdef USE_TILE
    tiles.place_cursor(CURSOR_MOUSE, NO_CURSOR);
#endif

    return cancel ? coord_def(0, 0) : delta;
}
