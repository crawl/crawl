/**
 * @file
 * @brief Monster tentacle-related code.
**/

#include "AppHdr.h"

#include "mon-tentacle.h"

#include "act-iter.h"
#include "coordit.h"
#include "delay.h"
#include "env.h"
#include "fprop.h"
#include "libutil.h" // map_find
#include "losglobal.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-death.h"
#include "mon-place.h"
#include "terrain.h"
#include "view.h"

const int MAX_KRAKEN_TENTACLE_DIST = 12;
const int MAX_ACTIVE_KRAKEN_TENTACLES = 4;
const int MAX_ACTIVE_STARSPAWN_TENTACLES = 2;

static monster_type _head_child_segment[][3] =
{
    { MONS_KRAKEN, MONS_KRAKEN_TENTACLE,
        MONS_KRAKEN_TENTACLE_SEGMENT },
    { MONS_TENTACLED_STARSPAWN, MONS_STARSPAWN_TENTACLE,
        MONS_STARSPAWN_TENTACLE_SEGMENT },
};

static monster_type _solo_tentacle_to_segment[][2] =
{
    { MONS_ELDRITCH_TENTACLE, MONS_ELDRITCH_TENTACLE_SEGMENT },
    { MONS_SNAPLASHER_VINE,   MONS_SNAPLASHER_VINE_SEGMENT },
};

bool mons_is_tentacle_head(monster_type mc)
{
    for (const monster_type (&m)[3] : _head_child_segment)
        if (mc == m[0])
            return true;

    return false;
}

bool mons_is_child_tentacle(monster_type mc)
{
    for (const monster_type (&m)[3] : _head_child_segment)
        if (mc == m[1])
            return true;

    return false;
}

bool mons_is_child_tentacle_segment(monster_type mc)
{
    for (const monster_type (&m)[3] : _head_child_segment)
        if (mc == m[2])
            return true;

    return false;
}

bool mons_is_solo_tentacle(monster_type mc)
{
    for (const monster_type (&m)[2] : _solo_tentacle_to_segment)
        if (mc == m[0])
            return true;

    return false;
}

bool mons_is_tentacle(monster_type mc)
{
    return mons_is_child_tentacle(mc) || mons_is_solo_tentacle(mc);
}

bool mons_is_tentacle_segment(monster_type mc)
{
    for (const monster_type (&m)[2] : _solo_tentacle_to_segment)
        if (mc == m[1])
            return true;

    return mons_is_child_tentacle_segment(mc);
}

bool mons_is_tentacle_or_tentacle_segment(monster_type mc)
{
    return mons_is_tentacle(mc) || mons_is_tentacle_segment(mc);
}

monster_type mons_tentacle_parent_type(const monster* mons)
{
    const monster_type mc = mons_base_type(mons);

    for (const monster_type (&m)[3] : _head_child_segment)
        if (mc == m[1])
            return m[0];

    for (const monster_type (&m)[3] : _head_child_segment)
        if (mc == m[2])
            return m[1];

    for (const monster_type (&m)[2] : _solo_tentacle_to_segment)
        if (mc == m[1])
            return m[0];

    return MONS_PROGRAM_BUG;
}

monster_type mons_tentacle_child_type(const monster* mons)
{
    const monster_type mc = mons_base_type(mons);

    for (const monster_type (&m)[3] : _head_child_segment)
        if (mc == m[0])
            return m[1];

    for (const monster_type (&m)[3] : _head_child_segment)
        if (mc == m[1])
            return m[2];

    for (const monster_type (&m)[2] : _solo_tentacle_to_segment)
        if (mc == m[0])
            return m[1];

    return MONS_PROGRAM_BUG;
}

bool monster::is_child_tentacle() const
{
    return mons_is_child_tentacle(mons_base_type(this));
}

bool monster::is_child_tentacle_segment() const
{
    return mons_is_child_tentacle_segment(mons_base_type(this));
}

bool monster::is_child_monster() const
{
    return is_child_tentacle() || is_child_tentacle_segment();
}

bool monster::is_child_tentacle_of(const monster* mons) const
{
    return mons_base_type(mons) == mons_tentacle_parent_type(this)
           && tentacle_connect == mons->mid;
}

bool monster::is_parent_monster_of(const monster* mons) const
{
    return mons_base_type(this) == mons_tentacle_parent_type(mons)
           && mons->tentacle_connect == mid;
}

//Returns whether a given monster is a tentacle segment immediately attached
//to the parent monster
bool mons_tentacle_adjacent(const monster* parent, const monster* child)
{
    return mons_is_tentacle_head(mons_base_type(parent))
           && mons_is_tentacle_segment(child->type)
           && child->props.exists("inwards")
           && child->props["inwards"].get_int() == (int) parent->mid;
}

bool get_tentacle_head(const monster*& mon)
{
    // For tentacle segments, find the associated tentacle.
    if (mon->is_child_tentacle_segment())
    {
        monster* tentacle = monster_by_mid(mon->tentacle_connect);
        if (!tentacle)
            return false;

        mon = tentacle;
    }

    // For tentacles, find the associated head.
    if (mon->is_child_tentacle())
    {
        monster* head = monster_by_mid(mon->tentacle_connect);
        if (!head)
            return false;

        mon = head;
    }

    return true;
}

static void _establish_connection(monster* tentacle,
                                  monster* head,
                                  set<position_node>::iterator path,
                                  monster_type connector_type)
{
    const position_node * last = &(*path);
    const position_node * current = last->last;

    // Tentacle is adjacent to the end position, not much to do.
    if (!current)
    {
        // This is a little awkward now. Oh well. -cao
        if (tentacle != head)
            tentacle->props["inwards"].get_int() = head->mid;
        else
            tentacle->props["inwards"].get_int() = MID_NOBODY;

        return;
    }

    // No base monster case (demonic tentacles)
    if (!monster_at(last->pos))
    {
        if (monster *connect = create_monster(
            mgen_data(connector_type, SAME_ATTITUDE(head), head,
                      0, 0, last->pos, head->foe,
                      MG_FORCE_PLACE, head->god, MONS_NO_MONSTER, tentacle->mid,
                      head->colour, PROX_CLOSE_TO_PLAYER)))
        {
            connect->props["inwards"].get_int() = MID_NOBODY;
            connect->props["outwards"].get_int() = MID_NOBODY;

            if (head->holiness() == MH_UNDEAD)
                connect->flags |= MF_FAKE_UNDEAD;

            connect->max_hit_points = tentacle->max_hit_points;
            connect->hit_points = tentacle->hit_points;
        }
        else
        {
            // Big failure mode.
            return;
        }
    }

    while (current)
    {
        // Last monster we visited or placed
        monster* last_mon = monster_at(last->pos);
        if (!last_mon)
        {
            // Should be something there, what to do if there isn't?
            mpr("Error! failed to place monster in tentacle connect change");
            break;
        }

        // Monster at the current square, should be the end of the line if there
        monster* current_mons = monster_at(current->pos);
        if (current_mons)
        {
            // Todo verify current monster type
            current_mons->props["inwards"].get_int() = last_mon->mid;
            last_mon->props["outwards"].get_int() = current_mons->mid;
            break;
        }

         // place a connector
        if (monster *connect = create_monster(
            mgen_data(connector_type, SAME_ATTITUDE(head), head,
                      0, 0, current->pos, head->foe,
                      MG_FORCE_PLACE, head->god, MONS_NO_MONSTER, tentacle->mid,
                      head->colour, PROX_CLOSE_TO_PLAYER)))
        {
            connect->max_hit_points = tentacle->max_hit_points;
            connect->hit_points = tentacle->hit_points;

            connect->props["inwards"].get_int() = last_mon->mid;
            connect->props["outwards"].get_int() = MID_NOBODY;

            if (last_mon->type == connector_type)
                last_mon->props["outwards"].get_int() = connect->mid;

            if (head->holiness() == MH_UNDEAD)
                connect->flags |= MF_FAKE_UNDEAD;

            if (monster_can_submerge(connect, env.grid(connect->pos())))
                connect->add_ench(ENCH_SUBMERGED);
        }
        else
        {
            // connector placement failed, what to do?
            mprf("connector placement failed at %d %d", current->pos.x, current->pos.y);
        }

        last = current;
        current = current->last;
    }
}

struct tentacle_attack_constraints
{
    vector<coord_def> * target_positions;

    map<coord_def, set<int> > * connection_constraints;
    monster *base_monster;
    int max_string_distance;
    int connect_idx[8];

    tentacle_attack_constraints()
    {
        for (int i=0; i<8; i++)
            connect_idx[i] = i;
    }

    int min_dist(const coord_def & pos)
    {
        int min = INT_MAX;
        for (coord_def tpos : *target_positions)
            min = std::min(min, grid_distance(pos, tpos));
        return min;
    }

    void operator()(const position_node & node,
                    vector<position_node> & expansion)
    {
        shuffle_array(connect_idx);

//        mprf("expanding %d %d, string dist %d", node.pos.x, node.pos.y, node.string_distance);
        for (int idx : connect_idx)
        {
            position_node temp;

            temp.pos = node.pos + Compass[idx];
            temp.string_distance = node.string_distance;
            temp.departure = node.departure;
            temp.connect_level = node.connect_level;
            temp.path_distance = node.path_distance;
            temp.estimate = 0;

            if (!in_bounds(temp.pos) || is_sanctuary(temp.pos))
                continue;

            if (!base_monster->is_habitable(temp.pos))
                temp.path_distance = DISCONNECT_DIST;
            else
            {
                actor * act_at = actor_at(temp.pos);
                monster* mons_at = monster_at(temp.pos);

                if (!act_at)
                    temp.path_distance += 1;
                // Can still search through a firewood monster, just at a higher
                // path cost.
                else if (mons_at && mons_is_firewood(mons_at)
                    && !mons_aligned(base_monster, mons_at))
                {
                    temp.path_distance += 10;
                }
                // An actor we can't path through is there
                else
                    temp.path_distance = DISCONNECT_DIST;

            }

            int connect_level = temp.connect_level;
            int base_connect_level = connect_level;

            if (auto constraint = map_find(*connection_constraints, temp.pos))
            {
                int max_val = constraint->empty()
                            ? INT_MAX : *constraint->rbegin();

                if (max_val < connect_level)
                    temp.departure = true;

                // If we can still feasibly retract (haven't left connect range)
                if (!temp.departure)
                {
                    if (constraint->count(connect_level))
                    {
                        while (constraint->count(connect_level + 1))
                            connect_level++;
                    }

                    int delta = connect_level - base_connect_level;
                    temp.connect_level = connect_level;
                    if (delta)
                        temp.string_distance -= delta;
                }

                if (connect_level < max_val)
                   temp.path_distance = DISCONNECT_DIST;
            }
            else
            {
                // We stopped retracting
                temp.departure = true;
            }

            if (temp.departure)
                temp.string_distance++;

//            if (temp.string_distance > MAX_KRAKEN_TENTACLE_DIST)
            if (temp.string_distance > max_string_distance)
                temp.path_distance = DISCONNECT_DIST;

            if (temp.path_distance != DISCONNECT_DIST)
                temp.estimate = min_dist(temp.pos);

            expansion.push_back(temp);
        }
    }
};

struct tentacle_connect_constraints
{
    map<coord_def, set<int> > * connection_constraints;

    monster* base_monster;

    tentacle_connect_constraints()
    {
        for (int i=0; i<8; i++)
            connect_idx[i] = i;
    }

    int connect_idx[8];
    void operator()(const position_node & node,
                    vector<position_node> & expansion)
    {
        shuffle_array(connect_idx);

        for (int idx : connect_idx)
        {
            position_node temp;

            temp.pos = node.pos + Compass[idx];

            if (!in_bounds(temp.pos))
                continue;

            auto constraint = map_find(*connection_constraints, temp.pos);

            if (!constraint || !constraint->count(node.connect_level))
                continue;

            if (!base_monster->is_habitable(temp.pos) || actor_at(temp.pos))
                temp.path_distance = DISCONNECT_DIST;
            else
                temp.path_distance = 1 + node.path_distance;

            //temp.estimate = grid_distance(temp.pos, kraken->pos());
            // Don't bother with an estimate, the search is highly constrained
            // so it's not really going to help
            temp.estimate = 0;
            int test_level = node.connect_level;

            while (constraint->count(test_level + 1))
                test_level++;

            int max = constraint->empty() ? INT_MAX : *constraint->rbegin();

            if (test_level < max)
                continue;

            temp.connect_level = test_level;

            expansion.push_back(temp);
        }
    }
};

struct target_position
{
    coord_def target;
    bool operator() (const coord_def & pos)
    {
        return pos == target;
    }
};

/*struct target_monster
{
    int target_mindex;

    bool operator() (const coord_def & pos)
    {
        monster* temp = monster_at(pos);
        if (!temp || temp->mindex() != target_mindex)
            return false;
        return true;

    }
};*/

struct multi_target
{
    vector<coord_def> * targets;

    bool operator() (const coord_def & pos)
    {
        return find(begin(*targets), end(*targets), pos) != end(*targets);
    }
};

// returns pathfinding success/failure
static bool _tentacle_pathfind(monster* tentacle,
                       tentacle_attack_constraints & attack_constraints,
                       coord_def & new_position,
                       vector<coord_def> & target_positions,
                       int total_length)
{
    multi_target foe_check { &target_positions };

    vector<set<position_node>::iterator > tentacle_path;

    set<position_node> visited;
    visited.clear();

    position_node temp;
    temp.pos = tentacle->pos();

    auto constraint = map_find(*attack_constraints.connection_constraints,
                               temp.pos);
    ASSERT(constraint);
    temp.connect_level = 0;
    while (constraint->count(temp.connect_level + 1))
        temp.connect_level++;

    temp.departure = false;
    temp.string_distance = total_length;

    search_astar(temp,
                 foe_check, attack_constraints,
                 visited, tentacle_path);

    bool path_found = false;
    // Did we find a path?
    if (!tentacle_path.empty())
    {
        // The end position is the enemy or target square, we need
        // to rewind the found path to find the next move

        const position_node * current = &(*tentacle_path[0]);
        const position_node * last;

        // The last position in the chain is the base position,
        // so we want to stop at the one before the last.
        while (current && current->last)
        {
            last = current;
            current = current->last;
            new_position = last->pos;
            path_found = true;
        }
    }

    return path_found;
}

static bool _try_tentacle_connect(const coord_def & new_pos,
                                  const coord_def & base_position,
                                  monster* tentacle,
                                  monster* head,
                                  tentacle_connect_constraints & connect_costs,
                                  monster_type connect_type)
{
    // Nothing to do here.
    // Except fix the tentacle end's pointer, idiot.
    if (base_position == new_pos)
    {
        if (tentacle == head)
            tentacle->props["inwards"].get_int() = MID_NOBODY;
        else
            tentacle->props["inwards"].get_int() = head->mid;
        return true;
    }

    int start_level = 0;
    // This condition should never miss
    if (auto constraint = map_find(*connect_costs.connection_constraints,
                          new_pos))
    {
        while (constraint->count(start_level + 1))
            start_level++;
    }

    // Find the tentacle -> head path
    target_position current_target;
    current_target.target = base_position;
/*  target_monster current_target;
    current_target.target_mindex = headnum;
*/

    set<position_node> visited;
    vector<set<position_node>::iterator> candidates;

    position_node temp;
    temp.pos = new_pos;
    temp.connect_level = start_level;

    search_astar(temp,
                 current_target, connect_costs,
                 visited, candidates);

    if (candidates.empty())
        return false;

    _establish_connection(tentacle, head, candidates[0], connect_type);

    return true;
}

static void _collect_tentacles(monster* mons,
                               vector<monster_iterator> & tentacles)
{
    // TODO: reorder tentacles based on distance to head or something.
    for (monster_iterator mi; mi; ++mi)
        if (mons->is_parent_monster_of(*mi))
            tentacles.push_back(mi);
}

static void _purge_connectors(monster* tentacle)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_child_tentacle_of(tentacle))
        {
            int hp = mi->hit_points;
            if (hp > 0 && hp < tentacle->hit_points)
                tentacle->hit_points = hp;

            monster_die(*mi, KILL_MISC, NON_MONSTER, true);
        }
    }
    ASSERT(tentacle->alive());
}

struct complicated_sight_check
{
    coord_def base_position;
    bool operator()(monster* mons, actor * test)
    {
        return test->visible_to(mons)
               && cell_see_cell(base_position, test->pos(), LOS_SOLID_SEE);
    }
};

static bool _basic_sight_check(monster* mons, actor * test)
{
    return mons->can_see(test);
}

template<typename T>
static void _collect_foe_positions(monster* mons,
                                   vector<coord_def> & foe_positions,
                                   T & sight_check)
{
    coord_def foe_pos(-1, -1);
    actor * foe = mons->get_foe();
    if (foe && sight_check(mons, foe))
    {
        foe_positions.push_back(mons->get_foe()->pos());
        foe_pos = foe_positions.back();
    }

    for (monster_iterator mi; mi; ++mi)
    {
        monster* test = *mi;
        if (!mons_is_firewood(test)
            && !mons_aligned(test, mons)
            && test->pos() != foe_pos
            && sight_check(mons, test))
        {
            foe_positions.push_back(test->pos());
        }
    }
}

// Return value is a retract position for the tentacle or -1, -1 if no
// retract position exists.
//
// move_kraken_tentacle should check retract pos, it could potentially
// give the kraken head's position as a retract pos.
static int _collect_connection_data(monster* start_monster,
               map<coord_def, set<int> > & connection_data,
               coord_def & retract_pos)
{
    int current_count = 0;
    monster* current_mon = start_monster;
    retract_pos.x = -1;
    retract_pos.y = -1;
    bool retract_found = false;

    while (current_mon)
    {
        for (adjacent_iterator adj_it(current_mon->pos(), false);
             adj_it; ++adj_it)
        {
            connection_data[*adj_it].insert(current_count);
        }

        bool basis = current_mon->props.exists("inwards");
        monster* next = basis ? monster_by_mid(current_mon->props["inwards"].get_int()) : nullptr;

        if (next && next->is_child_tentacle_of(start_monster))
        {
            current_mon = next;
            if (current_mon->tentacle_connect != start_monster->mid)
                mpr("link information corruption!!! tentacle in chain doesn't match mindex");
            if (!retract_found)
            {
                retract_pos = current_mon->pos();
                retract_found = true;
            }
        }
        else
        {
            current_mon = nullptr;
//            mprf("null at count %d", current_count);
        }
        current_count++;
    }

//    mprf("returned count %d", current_count);
    return current_count;
}

void move_solo_tentacle(monster* tentacle)
{
    if (!tentacle || !mons_is_solo_tentacle(mons_base_type(tentacle)))
        return;

    vector<coord_def> foe_positions;

    bool attack_foe = false;
    bool severed = tentacle->has_ench(ENCH_SEVERED);

    coord_def base_position;
    if (!tentacle->props.exists("base_position"))
        tentacle->props["base_position"].get_coord() = tentacle->pos();

    base_position = tentacle->props["base_position"].get_coord();

    if (!severed)
    {
        complicated_sight_check base_sight;
        base_sight.base_position = base_position;
        _collect_foe_positions(tentacle, foe_positions, base_sight);
        attack_foe = !foe_positions.empty();
    }

    coord_def retract_pos;
    map<coord_def, set<int> > connection_data;

    int visited_count = _collect_connection_data(tentacle,
                                                 connection_data,
                                                 retract_pos);

    bool retract_found = retract_pos.x != -1 && retract_pos.y != -1;

    _purge_connectors(tentacle);

    if (severed)
    {
        for (fair_adjacent_iterator ai(base_position); ai; ++ai)
        {
            if (!actor_at(*ai) && tentacle->is_habitable(*ai))
            {
                tentacle->props["base_position"].get_coord() = *ai;
                base_position = *ai;
                break;
            }
        }
    }

    coord_def new_pos = tentacle->pos();
    coord_def old_pos = tentacle->pos();

    int demonic_max_dist = (tentacle->type == MONS_ELDRITCH_TENTACLE ? 5 : 8);
    tentacle_attack_constraints attack_constraints;
    attack_constraints.base_monster = tentacle;
    attack_constraints.max_string_distance = demonic_max_dist;
    attack_constraints.connection_constraints = &connection_data;
    attack_constraints.target_positions = &foe_positions;

    bool path_found = false;
    if (attack_foe)
    {
        path_found = _tentacle_pathfind(tentacle, attack_constraints,
                                        new_pos, foe_positions, visited_count);
    }

    //If this tentacle is constricting a creature, attempt to pull it back
    //towards the head.
    bool pull_constrictee = false;
    bool shift_constrictee = false;
    coord_def shift_pos;
    actor* constrictee = nullptr;
    if (tentacle->is_constricting())
    {
        constrictee = actor_by_mid(tentacle->constricting->begin()->first);

        // Don't drag things that cannot move
        if (!constrictee->is_stationary())
        {
            if (retract_found)
            {
                if (constrictee->is_habitable(old_pos))
                {
                    pull_constrictee = true;
                    shift_pos = old_pos;
                }
            }
            else if (tentacle->type == MONS_SNAPLASHER_VINE)
            {
                // Don't shift our victim if they're already next to a tree
                // (To avoid shaking players back and forth constantly)
                bool near_tree = false;
                for (adjacent_iterator ai(constrictee->pos()); ai; ++ai)
                {
                    if (feat_is_tree(grd(*ai)))
                    {
                        near_tree = true;
                        break;
                    }
                }

                if (!near_tree)
                {
                    for (adjacent_iterator ai(tentacle->pos()); ai; ++ai)
                    {
                        if (adjacent(*ai, constrictee->pos())
                            && constrictee->is_habitable(*ai)
                            && !actor_at(*ai))
                        {
                            for (adjacent_iterator ai2(*ai); ai2; ++ai2)
                            {
                                if (feat_is_tree(grd(*ai2)))
                                {
                                    pull_constrictee = true;
                                    shift_constrictee = true;
                                    shift_pos = *ai;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!attack_foe || !path_found)
    {
        // todo: set a random position?

        dprf("pathing failed, target %d %d", new_pos.x, new_pos.y);
        for (fair_adjacent_iterator ai(old_pos); ai; ++ai)
        {
            if (!in_bounds(*ai) || is_sanctuary(*ai) || actor_at(*ai))
                continue;

            int escalated = 0;
            auto constraint = map_find(connection_data, *ai);
            ASSERT(constraint);

            while (constraint->count(escalated + 1))
                escalated++;

            if (!severed
                && tentacle->is_habitable(*ai)
                && escalated == *constraint->rbegin()
                && (visited_count < demonic_max_dist
                    || constraint->size() > 1))
            {
                new_pos = *ai;
                break;
            }
            else if (tentacle->is_habitable(*ai)
                     && visited_count > 1
                     && escalated == *constraint->rbegin()
                     && constraint->size() > 1)
            {
                new_pos = *ai;
                break;
            }
        }
    }
    else if (pull_constrictee && !shift_constrictee)
        new_pos = retract_pos;

    if (new_pos != old_pos)
    {
        // Did we path into a target?
        if (actor* blocking_actor = actor_at(new_pos))
        {
            tentacle->target = new_pos;
            tentacle->foe = blocking_actor->mindex();
            new_pos = old_pos;
        }
    }

    // Why do I have to do this move? I don't get it.
    // specifically, if tentacle isn't registered at its new position on mgrd
    // the search fails (sometimes), Don't know why. -cao
    tentacle->move_to_pos(new_pos);

    if (pull_constrictee)
    {
        if (you.can_see(tentacle))
        {
            mprf("The vine drags %s backwards!",
                    constrictee->name(DESC_THE).c_str());
        }

        if (constrictee->as_player())
            move_player_to_grid(shift_pos, false);
        else
            constrictee->move_to_pos(shift_pos);

        // Interrupt stair travel and passwall.
        if (constrictee->is_player())
            stop_delay(true);
    }
    tentacle->clear_far_constrictions();

    tentacle_connect_constraints connect_costs;
    connect_costs.connection_constraints = &connection_data;
    connect_costs.base_monster = tentacle;

    bool connected = _try_tentacle_connect(new_pos, base_position,
                                           tentacle, tentacle,
                                           connect_costs,
                                           mons_tentacle_child_type(tentacle));

    if (!connected)
    {
        // This should really never fail for demonic tentacles (they don't
        // have the whole shifting base problem). -cao
        mprf("tentacle connect failed! What the heck!  severed status %d",
             tentacle->has_ench(ENCH_SEVERED));
        mprf("pathed to %d %d from %d %d mid %d count %d", new_pos.x, new_pos.y,
             old_pos.x, old_pos.y, tentacle->mid, visited_count);

        // Is it ok to purge the tentacle here?
        monster_die(tentacle, KILL_MISC, NON_MONSTER, true);
        return;
    }

//  mprf("mindex %d vsisted %d", tentacle_idx, visited_count);
    tentacle->check_redraw(old_pos);
    tentacle->apply_location_effects(old_pos);
}

void move_child_tentacles(monster* mons)
{
    if (!mons_is_tentacle_head(mons_base_type(mons))
        || mons->asleep())
    {
        return;
    }

    bool no_foe = false;

    vector<coord_def> foe_positions;
    _collect_foe_positions(mons, foe_positions, _basic_sight_check);

    //if (!kraken->near_foe())
    if (foe_positions.empty()
        || mons->behaviour == BEH_FLEE
        || mons->behaviour == BEH_WANDER)
    {
        no_foe = true;
    }
    vector<monster_iterator> tentacles;
    _collect_tentacles(mons, tentacles);

    // Move each tentacle in turn
    for (monster_iterator &tent_it : tentacles)
    {
        // XXX: Why not just use *tent_it ?
        monster* tentacle = monster_at(tent_it->pos());

        if (!tentacle)
        {
            dprf("Missing tentacle in path.");
            continue;
        }

        tentacle_connect_constraints connect_costs;
        map<coord_def, set<int> > connection_data;

        monster* current_mon = tentacle;
        int current_count = 0;
        bool retract_found = false;
        coord_def retract_pos(-1, -1);

        while (current_mon)
        {
            for (adjacent_iterator adj_it(current_mon->pos(), false);
                 adj_it; ++adj_it)
            {
                connection_data[*adj_it].insert(current_count);
            }

            bool basis = current_mon->props.exists("inwards");
            monster* inward = basis ? monster_by_mid(current_mon->props["inwards"].get_int()) : nullptr;

            if (inward
                && (inward->is_child_tentacle_of(tentacle)
                    || inward->is_parent_monster_of(tentacle)))
            {
                current_mon = inward;
                if (!retract_found
                    && current_mon->is_child_tentacle_of(tentacle))
                {
                    retract_pos = current_mon->pos();
                    retract_found = true;
                }
            }
            else
                current_mon = nullptr;
            current_count++;
        }

        _purge_connectors(tentacle);

        if (no_foe
            && grid_distance(tentacle->pos(), mons->pos()) == 1)
        {
            // Drop the tentacle if no enemies are in sight and it is
            // adjacent to the main body. This is to prevent players from
            // just sniping tentacles while outside the kraken's fov.
            monster_die(tentacle, KILL_MISC, NON_MONSTER, true);
            continue;
        }

        coord_def new_pos = tentacle->pos();
        coord_def old_pos = tentacle->pos();
        bool path_found = false;

        tentacle_attack_constraints attack_constraints;
        attack_constraints.base_monster = tentacle;
        attack_constraints.max_string_distance = MAX_KRAKEN_TENTACLE_DIST;
        attack_constraints.connection_constraints = &connection_data;
        attack_constraints.target_positions = &foe_positions;

        //If this tentacle is constricting a creature, attempt to pull it back
        //towards the head.
        bool pull_constrictee = false;
        actor* constrictee = nullptr;
        if (tentacle->is_constricting() && retract_found)
        {
            constrictee = actor_by_mid(tentacle->constricting->begin()->first);
            if (feat_has_solid_floor(grd(old_pos))
                && constrictee->is_habitable(old_pos))
            {
                pull_constrictee = true;
            }
        }

        if (!no_foe && !pull_constrictee)
        {
            path_found = _tentacle_pathfind(
                    tentacle,
                    attack_constraints,
                    new_pos,
                    foe_positions,
                    current_count);
        }

        if (no_foe || !path_found || pull_constrictee)
        {
            if (retract_found)
                new_pos = retract_pos;
            else
            {
                // What happened here? Usually retract found should be true
                // or we should get pruned (due to being adjacent to the
                // head), in any case just stay here.
            }
        }

        // Did we path into a target?
        if (actor* blocking_actor = actor_at(new_pos))
        {
            tentacle->target = new_pos;
            tentacle->foe = blocking_actor->mindex();
            new_pos = old_pos;
        }

        // Why do I have to do this move? I don't get it.
        // specifically, if tentacle isn't registered at its new position on
        // mgrd the search fails (sometimes), Don't know why. -cao
        tentacle->move_to_pos(new_pos);

        if (pull_constrictee)
        {
            if (you.can_see(tentacle))
            {
                mprf("The tentacle pulls %s backwards!",
                     constrictee->name(DESC_THE).c_str());
            }

            if (constrictee->as_player())
                move_player_to_grid(old_pos, false);
            else
                constrictee->move_to_pos(old_pos);

            // Interrupt stair travel and passwall.
            if (constrictee->is_player())
                stop_delay(true);
        }
        tentacle->clear_far_constrictions();

        connect_costs.connection_constraints = &connection_data;
        connect_costs.base_monster = tentacle;
        bool connected = _try_tentacle_connect(new_pos, mons->pos(),
                                tentacle, mons,
                                connect_costs,
                                mons_tentacle_child_type(tentacle));

        // Can't connect, usually the head moved and invalidated our position
        // in some way. Should look into this more at some point -cao
        if (!connected)
        {
            mgrd(tentacle->pos()) = tentacle->mindex();
            monster_die(tentacle, KILL_MISC, NON_MONSTER, true);

            continue;
        }

        tentacle->check_redraw(old_pos);
        tentacle->apply_location_effects(old_pos);
    }
}

static monster* _mons_get_parent_monster(monster* mons)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_parent_monster_of(mons))
            return *mi;
    }

    return nullptr;
}

// When given either a tentacle end or segment, kills the end and all segments
// of that tentacle.
int destroy_tentacle(monster* mons)
{
    int seen = 0;

    monster* head = mons_is_tentacle_segment(mons->type)
            ? _mons_get_parent_monster(mons) : mons;

    //If we tried to find the head, but failed (probably because it is already
    //dead), cancel trying to kill this tentacle
    if (head == nullptr)
        return 0;

    // Some issue with using monster_die leading to DEAD_MONSTER
    // or w/e. Using hurt seems to cause more problems though.
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_child_tentacle_of(head))
        {
            if (mons_near(*mi))
                seen++;
            //mi->hurt(*mi, INSTANT_DEATH);
            monster_die(*mi, KILL_MISC, NON_MONSTER, true);
        }
    }

    if (mons != head)
    {
        if (mons_near(head))
                seen++;

        monster_die(head, KILL_MISC, NON_MONSTER, true);
    }

    return seen;
}

int destroy_tentacles(monster* head)
{
    int seen = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_child_tentacle_of(head))
        {
            if (destroy_tentacle(*mi))
                seen++;
            if (!mi->is_child_tentacle_segment())
            {
                monster_die(mi->as_monster(), KILL_MISC, NON_MONSTER, true);
                seen++;
            }
        }
    }
    return seen;
}

static int _max_tentacles(const monster* mon)
{
    if (mons_base_type(mon) == MONS_KRAKEN)
        return MAX_ACTIVE_KRAKEN_TENTACLES;
    else if (mon->type == MONS_TENTACLED_STARSPAWN)
        return MAX_ACTIVE_STARSPAWN_TENTACLES;
    else
        return 0;
}

int mons_available_tentacles(monster* head)
{
    int tentacle_count = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->is_child_tentacle_of(head))
            tentacle_count++;
    }

    return _max_tentacles(head) - tentacle_count;
}

void mons_create_tentacles(monster* head)
{
    int possible_count = mons_available_tentacles(head);

    if (possible_count <= 0)
        return;

    monster_type tent_type = mons_tentacle_child_type(head);

    vector<coord_def> adj_squares;

    // Collect open adjacent squares. Candidate squares must be
    // unoccupied.
    for (adjacent_iterator adj_it(head->pos()); adj_it; ++adj_it)
    {
        if (monster_habitable_grid(tent_type, grd(*adj_it))
            && !actor_at(*adj_it))
        {
            adj_squares.push_back(*adj_it);
        }
    }

    if (unsigned(possible_count) > adj_squares.size())
        possible_count = adj_squares.size();
    else if (adj_squares.size() > unsigned(possible_count))
        shuffle_array(adj_squares);

    int visible_count = 0;

    for (int i = 0 ; i < possible_count; ++i)
    {
        if (monster *tentacle = create_monster(
            mgen_data(tent_type, SAME_ATTITUDE(head), head,
                        0, 0, adj_squares[i], head->foe,
                        MG_FORCE_PLACE, head->god, MONS_NO_MONSTER,
                        head->mid, head->colour,
                        PROX_CLOSE_TO_PLAYER)))
        {
            if (you.can_see(tentacle))
                visible_count++;

            tentacle->props["inwards"].get_int() = head->mid;

            if (head->holiness() == MH_UNDEAD)
                tentacle->flags |= MF_FAKE_UNDEAD;
        }
    }

    if (mons_base_type(head) == MONS_KRAKEN)
    {
        if (visible_count == 1)
            mpr("A tentacle rises from the water!");
        else if (visible_count > 1)
            mpr("Tentacles burst out of the water!");
    }
    else if (head->type == MONS_TENTACLED_STARSPAWN)
    {
        if (visible_count == 1)
            mpr("A tentacle flies out from the starspawn's body!");
        else if (visible_count > 1)
            mpr("Tentacles burst from the starspawn's body!");
    }
    return;
}
