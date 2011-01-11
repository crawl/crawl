/*
 *  File:       tilereg_cmd.h
 *  Created by: jpeg on Sat, Dec 27 2010
 */

#ifdef USE_TILE
#ifndef TILEREG_CMD_H
#define TILEREG_CMD_H

#include "tilereg-grid.h"

static const command_type _common_commands[] =
{
    // action commands
    CMD_REST, CMD_EXPLORE, CMD_INTERLEVEL_TRAVEL,
#ifdef CLUA_BINDINGS
    CMD_AUTOFIGHT,
#endif
    CMD_USE_ABILITY, CMD_PRAY, CMD_SEARCH_STASHES,

    // informational commands
    CMD_REPLAY_MESSAGES, CMD_RESISTS_SCREEN, CMD_DISPLAY_OVERMAP,
    CMD_DISPLAY_RELIGION, CMD_DISPLAY_MUTATIONS, CMD_DISPLAY_SKILLS,
    CMD_DISPLAY_CHARACTER_STATUS, CMD_DISPLAY_KNOWN_OBJECTS,

    // meta commands
    CMD_SAVE_GAME_NOW, CMD_EDIT_PLAYER_TILE, CMD_DISPLAY_COMMANDS,
    CMD_CHARACTER_DUMP
};

static const int n_common_commands = ARRAYSZ(_common_commands);

class CommandRegion : public GridRegion
{
public:
    CommandRegion(const TileRegionInit &init);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_tab_tip_text(std::string &tip, bool active);
    virtual bool update_alt_text(std::string &alt);

    virtual const std::string name() const { return "Commands"; }

protected:
    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate();
};

#endif
#endif
