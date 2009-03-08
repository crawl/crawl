/*
 *  File:       directn.h
 *  Summary:    Functions used when picking squares.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef DIRECT_H
#define DIRECT_H

#include "describe.h"
#include "externs.h"
#include "enum.h"
#include "libgui.h"
#include "ray.h"
#include "state.h"

class range_view_annotator : public crawl_exit_hook
{
public:
    range_view_annotator(int range);
    virtual ~range_view_annotator();
    virtual void restore_state();
private:
    bool do_anything;
    FixedArray<int, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> orig_colours;
    int orig_mon_colours[MAX_MONSTERS];
};

class crawl_view_buffer
{
public:
    crawl_view_buffer();
    ~crawl_view_buffer();
    void size(const coord_def &size);
    operator screen_buffer_t * () { return (buffer); }

    void draw();
private:
    screen_buffer_t *buffer;
};

struct crawl_view_geometry
{
public:
    coord_def termp;               // Left-top pos of terminal.
    coord_def termsz;              // Size of the terminal.
    coord_def viewp;               // Left-top pos of viewport.
    coord_def viewsz;              // Size of the viewport (play area).
    coord_def hudp;                // Left-top pos of status area.
    coord_def hudsz;               // Size of the status area.
    coord_def msgp;                // Left-top pos of the message pane.
    coord_def msgsz;               // Size of the message pane.
    coord_def mlistp;              // Left-top pos of the monster list.
    coord_def mlistsz;             // Size of the monster list.

    crawl_view_buffer vbuf;        // Buffer for drawing the main game map.

    coord_def vgrdc;               // What grid pos is at the centre of the view
                                   // usually you.pos().

    coord_def viewhalfsz;

    coord_def glos1, glos2;        // LOS limit grid coords (inclusive)
    coord_def vlos1, vlos2;        // LOS limit viewport coords (inclusive)

    coord_def mousep;              // Where the mouse is.

private:
    coord_def last_player_pos;

public:
    crawl_view_geometry();
    void init_geometry();

    void init_view();
    void set_player_at(const coord_def &c, bool force_centre = false);
    // Set new location, but preserve scrolling as if the player didn't move.
    void shift_player_to(const coord_def &c);

    coord_def view_centre() const
    {
        return viewp + viewhalfsz;
    }

    coord_def glosc() const
    {
        return (glos1 + glos2) / 2;
    }

    bool in_grid_los(const coord_def &c) const
    {
        return (c.x >= glos1.x && c.x <= glos2.x
                && c.y >= glos1.y && c.y <= glos2.y);
    }

    bool in_view_los(const coord_def &c) const
    {
        return (c.x >= vlos1.x && c.x <= vlos2.x
                && c.y >= vlos1.y && c.y <= vlos2.y);
    }

    bool in_view_viewport(const coord_def &c) const
    {
        return (c.x >= viewp.x && c.y >= viewp.y
                && c.x < viewp.x + viewsz.x
                && c.y < viewp.y + viewsz.y);
    }

    bool in_grid_viewport(const coord_def &c) const
    {
        return in_view_viewport(c - vgrdc + view_centre());
    }
};

extern crawl_view_geometry crawl_view;

// An object that modifies the behaviour of the direction prompt.
class targeting_behaviour
{
public:
    targeting_behaviour(bool just_looking = false);
    virtual ~targeting_behaviour();

    // Returns a keystroke for the prompt.
    virtual int get_key();
    virtual command_type get_command(int key = -1);
    virtual bool should_redraw();
    virtual void mark_ammo_nonchosen();

public:
    bool just_looking;
    bool compass;
};

// output from direction() function:
struct dist
{
    bool isValid;       // valid target chosen?
    bool isTarget;      // target (true), or direction (false)?
    bool isMe;          // selected self (convenience: target == you.pos())
    bool isEndpoint;    // Does the player want the attack to stop at target?
    bool isCancel;      // user cancelled (usually <ESC> key)
    bool choseRay;      // user wants a specific beam
    coord_def target;   // target x,y or logical extension of beam to map edge
    coord_def delta;    // delta x and y if direction - always -1,0,1
    ray_def ray;        // ray chosen if necessary

    // internal use - ignore
    int  prev_target;   // previous target
};

void direction( dist &moves, targeting_type restricts = DIR_NONE,
                targ_mode_type mode = TARG_ANY, int range = -1,
                bool just_looking = false, bool needs_path = true,
                bool may_target_monster = true, bool may_target_self = false,
                const char *prompt = NULL, targeting_behaviour *mod = NULL,
                bool cancel_at_self = false );

bool in_los_bounds(const coord_def& p);
bool in_viewport_bounds(int x, int y);
inline bool in_viewport_bounds(const coord_def& pos) {
  return in_viewport_bounds(pos.x, pos.y);
}
bool in_los(int x, int y);
bool in_los(const coord_def &pos);
bool in_vlos(int x, int y);
bool in_vlos(const coord_def &pos);

std::string thing_do_grammar(description_level_type dtype,
                             bool add_stop,
                             bool force_article,
                             std::string desc);

std::string get_terse_square_desc(const coord_def &gc);
void terse_describe_square(const coord_def &c, bool in_range = true);
void full_describe_square(const coord_def &c);
void get_square_desc(const coord_def &c, describe_info &inf,
                     bool examine_mons = false);

void describe_floor();
std::string get_monster_equipment_desc(const monsters *mon,
                                bool full_desc = true,
                                description_level_type mondtype = DESC_CAP_A,
                                bool print_attitude = false);

int dos_direction_unmunge(int doskey);

std::string feature_description(const coord_def& where, bool bloody = false,
                                description_level_type dtype = DESC_CAP_A,
                                bool add_stop = true, bool base_desc = false);
std::string raw_feature_description(dungeon_feature_type grid,
                                    trap_type tr = NUM_TRAPS,
                                    bool base_desc = false);
std::string feature_description(dungeon_feature_type grid,
                                trap_type trap = NUM_TRAPS, bool bloody = false,
                                description_level_type dtype = DESC_CAP_A,
                                bool add_stop = true, bool base_desc = false);

void set_feature_desc_short(dungeon_feature_type grid,
                            const std::string &desc);
void set_feature_desc_short(const std::string &base_name,
                            const std::string &desc);

void setup_feature_descs_short();

std::vector<dungeon_feature_type> features_by_desc(const base_pattern &pattern);

void full_describe_view(void);

inline int view2gridX(int vx)
{
    return (crawl_view.vgrdc.x + vx - crawl_view.view_centre().x);
}

inline int view2gridY(int vy)
{
    return (crawl_view.vgrdc.y + vy - crawl_view.view_centre().y);
}

inline coord_def view2grid(const coord_def &pos)
{
    return pos - crawl_view.view_centre() + crawl_view.vgrdc;
}

inline int grid2viewX(int gx)
{
    return (gx - crawl_view.vgrdc.x + crawl_view.view_centre().x);
}

inline int grid2viewY(int gy)
{
    return (gy - crawl_view.vgrdc.y + crawl_view.view_centre().y);
}

inline coord_def grid2view(const coord_def &pos)
{
    return (pos - crawl_view.vgrdc + crawl_view.view_centre());
}

inline coord_def view2show(const coord_def &pos)
{
    return (pos - crawl_view.vlos1 + coord_def(1, 1));
}

inline coord_def show2view(const coord_def &pos)
{
    return (pos + crawl_view.vlos1 - coord_def(1, 1));
}

inline coord_def grid2show(const coord_def &pos)
{
    return (view2show(grid2view(pos)));
}

inline coord_def show2grid(const coord_def &pos)
{
    return (view2grid(show2view(pos)));
}

extern const struct coord_def Compass[8];

#endif
