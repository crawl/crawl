/**
 * @file
 * @brief What's near the player?
**/

#pragma once

#include "coord.h"
#include "random.h" // shuffle_array

#include <numeric> // iota
#include <queue>

extern const struct coord_def Compass[9];

bool mons_can_hurt_player(const monster* mon, const bool want_move = false);
bool mons_is_safe(const monster* mon, const bool want_move = false,
                  const bool consider_user_options = true,
                  const bool check_dist = true);

vector<monster* > get_nearby_monsters(bool want_move = false,
                                      bool just_check = false,
                                      bool dangerous_only = false,
                                      bool consider_user_options = true,
                                      bool require_visible = true,
                                      bool check_dist = true,
                                      int range = -1);

bool i_feel_safe(bool announce = false, bool want_move = false,
                 bool just_monsters = false, bool check_dist = true,
                 int range = -1);

bool there_are_monsters_nearby(bool dangerous_only = false,
                               bool require_visible = true,
                               bool consider_user_options = false);


bool player_in_a_dangerous_place(bool *invis = nullptr);
void bring_to_safety();
void revive(); // XXX: move elsewhere?

#define DISCONNECT_DIST (INT_MAX - 1000)

struct position_node
{
    position_node(const position_node & existing)
        : pos(existing.pos), last(existing.last), estimate(existing.estimate),
          path_distance(existing.path_distance),
          connect_level(existing.connect_level),
          string_distance(existing.string_distance),
          departure(existing.departure)
    {
    }

    position_node()
        : pos(), last(nullptr), estimate(0), path_distance(0),
          connect_level(0), string_distance(0), departure(false)
    {
    }

    coord_def pos;
    const position_node * last;

    int estimate;
    int path_distance;
    int connect_level;
    int string_distance;
    bool departure;

    bool operator < (const position_node & right) const
    {
        if (pos == right.pos)
            return string_distance < right.string_distance;

        return pos < right.pos;

  //      if (pos.x == right.pos.x)
//            return pos.y < right.pos.y;

//        return pos.x < right.pos.x;
    }

    int total_dist() const
    {
        return estimate + path_distance;
    }
};

struct path_less
{
    bool operator()(const set<position_node>::iterator & left,
                    const set<position_node>::iterator & right)
    {
        if (left->total_dist() == right->total_dist())
            return left->pos > right->pos;
        return left->total_dist() > right->total_dist();
    }
};

template<typename cost_T, typename est_T>
struct simple_connect
{
    cost_T cost_function;
    est_T estimate_function;

    int connect;
    int compass_idx[8];

    simple_connect(int cmode, cost_T &cf, est_T &ef)
        : cost_function(cf), estimate_function(ef), connect(cmode)
    {
        iota(begin(compass_idx), end(compass_idx), 0);
    }

    void operator()(const position_node & node,
                    vector<position_node> & expansion)
    {
        shuffle_array(compass_idx, connect);

        for (int i=0; i < connect; i++)
        {
            position_node temp;
            temp.pos = node.pos + Compass[compass_idx[i]];
            if (!in_bounds(temp.pos))
                continue;

            int cost = cost_function(temp.pos);
//            if (cost == DISCONNECT_DIST)
  //              continue;
            temp.path_distance = node.path_distance + cost;

            temp.estimate = estimate_function(temp.pos);
            expansion.push_back(temp);
            // leaving last undone for now, don't want to screw the pointer up.
        }
    }
};

struct coord_wrapper
{
    coord_wrapper( int (*input) (const coord_def & pos))
    {
        test = input;
    }
    int (*test) (const coord_def & pos);
    int  operator()(const coord_def & pos)
    {
        return test(pos);
    }

    coord_wrapper()
    {
    }
};

template<typename valid_T, typename expand_T>
void search_astar(position_node & start,
                  valid_T & valid_target,
                  expand_T & expand_node,
                  set<position_node> & visited,
                  vector<set<position_node>::iterator > & candidates)
{
    priority_queue<set<position_node>::iterator,
                        vector<set<position_node>::iterator>,
                        path_less  > fringe;

    set<position_node>::iterator current = visited.insert(start).first;
    fringe.push(current);

    bool done = false;
    while (!fringe.empty())
    {
        current = fringe.top();
        fringe.pop();

        vector<position_node> expansion;
        expand_node(*current, expansion);

        for (position_node &exp : expansion)
        {
            exp.last = &(*current);

            pair<set<position_node>::iterator, bool > res;
            res = visited.insert(exp);

            if (!res.second)
                continue;

            if (valid_target(res.first->pos))
            {
                candidates.push_back(res.first);
                done = true;
                break;
            }

            if (res.first->path_distance < DISCONNECT_DIST)
                fringe.push(res.first);
        }
        if (done)
            break;
    }
}

template<typename valid_T, typename expand_T>
void search_astar(const coord_def & start,
                  valid_T & valid_target,
                  expand_T & expand_node,
                  set<position_node> & visited,
                  vector<set<position_node>::iterator > & candidates)
{
    position_node temp_node;
    temp_node.pos = start;
    temp_node.last = nullptr;
    temp_node.path_distance = 0;

    search_astar(temp_node, valid_target, expand_node, visited, candidates);
}

template<typename valid_T, typename cost_T, typename est_T>
void search_astar(const coord_def & start,
                  valid_T & valid_target,
                  cost_T & connection_cost,
                  est_T & cost_estimate,
                  set<position_node> & visited,
                  vector<set<position_node>::iterator > & candidates,
                  int connect_mode = 8)
{
    if (connect_mode < 1 || connect_mode > 8)
        connect_mode = 8;

    simple_connect<cost_T, est_T> connect(connect_mode, connection_cost,
                                          cost_estimate);
    search_astar(start, valid_target, connect, visited, candidates);
}
