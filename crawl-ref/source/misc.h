/**
 * @file
 * @brief Misc functions.
**/

#ifndef MISC_H
#define MISC_H

#include "coord.h"
#include "target.h"

#include <algorithm>
#include <queue>

extern const struct coord_def Compass[9];

void trackers_init_new_level(bool transit);

string weird_glowing_colour();

string weird_writing();

string weird_smell();

string weird_sound();

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
void revive();

bool bad_attack(const monster *mon, string& adj, string& suffix,
                bool& would_cause_penance,
                coord_def attack_pos = coord_def(0, 0),
                bool check_landing_only = false);

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool autohit_first = false,
                        bool *prompted = nullptr,
                        coord_def attack_pos = coord_def(0, 0),
                        bool check_landing_only = false);

bool stop_attack_prompt(targetter &hitfunc, const char* verb,
                        bool (*affects)(const actor *victim) = 0,
                        bool *prompted = nullptr);

void swap_with_monster(monster *mon_to_swap);

void auto_id_inventory();

int apply_chunked_AC(int dam, int ac);

void entered_malign_portal(actor* act);

void handle_real_time(time_t t = time(0));
string part_stack_string(const int num, const int total);
unsigned int breakpoint_rank(int val, const int breakpoints[],
                             unsigned int num_breakpoints);

#define DISCONNECT_DIST (INT_MAX - 1000)

struct position_node
{
    position_node(const position_node & existing)
    {
        pos = existing.pos;
        last = existing.last;
        estimate = existing.estimate;
        path_distance = existing.path_distance;
        connect_level = existing.connect_level;
        string_distance = existing.string_distance;
        departure = existing.departure;
    }

    position_node()
    {
        pos.x=0;
        pos.y=0;
        last = nullptr;
        estimate = 0;
        path_distance = 0;
        connect_level = 0;
        string_distance = 0;
        departure = false;
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

    simple_connect()
    {
        for (unsigned i=0; i<8; i++)
            compass_idx[i] = i;
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

    simple_connect<cost_T, est_T> connect;
    connect.connect = connect_mode;
    connect.cost_function = connection_cost;
    connect.estimate_function = cost_estimate;

    search_astar(start, valid_target, connect, visited, candidates);
}

struct counted_monster_list
{
    typedef pair<const monster* ,int> counted_monster;
    typedef vector<counted_monster> counted_list;
    counted_list list;
    void add(const monster* mons);
    int count();
    bool empty() { return list.empty(); }
    string describe(description_level_type desc = DESC_THE,
                    bool force_article = false);
};

bool today_is_halloween();

/** Remove from a container all elements matching a predicate.
 *
 * @tparam C the container type. Must be reorderable (not a map or set!),
 *           and must provide an erase() method taking two iterators.
 * @tparam P the predicate type, typically but not necessarily
 *           function<bool (const C::value_type &)>
 * @param container The container to modify.
 * @param pred The predicate to test.
 *
 * @post The container contains only those elements for which pred(elt)
 *       returns false, in the same relative order.
 */
template<class C, class P>
void erase_if(C &container, P pred)
{
    container.erase(remove_if(begin(container), end(container), pred),
                    end(container));
}

/** Remove from a container all elements with a given value.
 *
 * @tparam C the container type. Must be reorderable (not a map or set!),
 *           must provide an erase() method taking two iterators, and
 *           must provide a value_type type member.
 * @param container The container to modify.
 * @param val The value to remove.
 *
 * @post The container contains only those elements not equal to val, in the
 *       in the same relative order.
 */
template<class C>
void erase_val(C &container, const typename C::value_type &val)
{
    container.erase(remove(begin(container), end(container), val),
                    end(container));
}
#endif
