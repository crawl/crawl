#ifdef USE_TILE_LOCAL
#pragma once

#include <vector>

#include "command-type.h"
#include "tile-inventory-flags.h"
#include "tilereg-grid.h"

static const command_type ct_system_commands[] =
{
    // informational commands
    CMD_REPLAY_MESSAGES, CMD_RESISTS_SCREEN, CMD_DISPLAY_OVERMAP,
    CMD_DISPLAY_RELIGION, CMD_DISPLAY_MUTATIONS, CMD_DISPLAY_SKILLS,
    CMD_DISPLAY_CHARACTER_STATUS, CMD_DISPLAY_KNOWN_OBJECTS,

    // meta commands
    CMD_SAVE_GAME_NOW, CMD_EDIT_PLAYER_TILE, CMD_DISPLAY_COMMANDS,
};

static const command_type ct_map_commands[] =
{
    CMD_DISPLAY_MAP,
    CMD_MAP_GOTO_TARGET,

    CMD_MAP_NEXT_LEVEL,
    CMD_MAP_PREV_LEVEL,
    CMD_MAP_GOTO_LEVEL,

    CMD_MAP_EXCLUDE_AREA,
    CMD_MAP_FIND_EXCLUDED,
    CMD_MAP_CLEAR_EXCLUDES,

    CMD_MAP_ADD_WAYPOINT,
    CMD_MAP_FIND_WAYPOINT,

    CMD_MAP_FIND_UPSTAIR,
    CMD_MAP_FIND_DOWNSTAIR,
    CMD_MAP_FIND_YOU,
    CMD_MAP_FIND_PORTAL,
    CMD_MAP_FIND_TRAP,
    CMD_MAP_FIND_ALTAR,
//    CMD_MAP_FIND_F,  // no-one knows what this is for, so it's been taken out :P

    CMD_MAP_FIND_STASH,
};

static const command_type ct_action_commands[] =
{
    CMD_EXPLORE,
    CMD_REST, CMD_WAIT,
    CMD_BUTCHER,
    CMD_DISPLAY_INVENTORY, CMD_DROP,
    CMD_CAST_SPELL, CMD_USE_ABILITY,
    CMD_DISPLAY_SKILLS, CMD_MEMORISE_SPELL,
    CMD_INTERLEVEL_TRAVEL, CMD_SEARCH_STASHES,
    CMD_LOOKUP_HELP,
#ifdef TOUCH_UI
    CMD_SHOW_KEYBOARD,
#endif
};

class CommandRegion : public GridRegion
{
public:
    CommandRegion(const TileRegionInit &init, const command_type commands[],
                  const int n_commands, const string name="Commands",
                  const string help="Execute commands");
    int n_common_commands;

    virtual void update() override;
    virtual int handle_mouse(MouseEvent &event) override;
    virtual bool update_tip_text(string &tip) override;
    virtual bool update_tab_tip_text(string &tip, bool active) override;
    virtual bool update_alt_text(string &alt) override;

    virtual const string name() const override { return m_name; }

protected:
    virtual void pack_buffers() override;
    virtual void draw_tag() override;
    virtual void activate() override;

private:
    vector<command_type> _common_commands;
    string m_name;
    string m_help;
};

#endif
