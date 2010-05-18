/*
 *  File:       mon-stuff.h
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 */


#ifndef MONSTUFF_H
#define MONSTUFF_H

#define ORIG_MONSTER_KEY "orig_monster_key"

enum mon_dam_level_type
{
    MDAM_OKAY,
    MDAM_LIGHTLY_DAMAGED,
    MDAM_MODERATELY_DAMAGED,
    MDAM_HEAVILY_DAMAGED,
    MDAM_SEVERELY_DAMAGED,
    MDAM_ALMOST_DEAD,
    MDAM_DEAD
};

enum mon_desc_type   // things that cross categorical lines {dlb}
{
    MDSC_LEAVES_HIDE,                  //    0
    MDSC_REGENERATES,
    MDSC_NOMSG_WOUNDS
};

struct level_exit
{
    coord_def target;
    bool unreachable;

    level_exit(coord_def t = coord_def(-1, -1), bool u = true)
        : target(t), unreachable(u)
    {
    }
};

#define FRESHEST_CORPSE 210

#define YOU_KILL(x) ((x) == KILL_YOU || (x) == KILL_YOU_MISSILE \
                     || (x) == KILL_YOU_CONF)
#define MON_KILL(x) ((x) == KILL_MON || (x) == KILL_MON_MISSILE)

#define SAME_ATTITUDE(x) (x->friendly()       ? BEH_FRIENDLY : \
                          x->good_neutral()   ? BEH_GOOD_NEUTRAL : \
                          x->strict_neutral() ? BEH_STRICT_NEUTRAL : \
                          x->neutral()        ? BEH_NEUTRAL \
                                              : BEH_HOSTILE)

#define MONST_INTERESTING(x) (x->flags & MF_INTERESTING)


const item_def *give_mimic_item(monsters *mimic);
const item_def &get_mimic_item(const monsters *mimic);
int  get_mimic_colour( const monsters *mimic );

void alert_nearby_monsters(void);

beh_type attitude_creation_behavior(mon_attitude_type att);
beh_type actual_same_attitude(const monsters & base);

enum poly_power_type {
    PPT_LESS,
    PPT_MORE,
    PPT_SAME
};

bool monster_polymorph(monsters *monster, monster_type targetc,
                       poly_power_type power = PPT_SAME,
                       bool force_beh = false);

int monster_die(monsters *monster, killer_type killer,
                int killer_index, bool silent = false, bool wizard = false);

monster_type fill_out_corpse(const monsters* monster, item_def& corpse,
                             bool allow_weightless = false);

bool explode_corpse(item_def& corpse, const coord_def& where);

int place_monster_corpse(const monsters *monster, bool silent,
                         bool force = false);

void slimify_monster(monsters *monster, bool hostile = false);

bool mon_can_be_slimified(monsters *monster);

void corrode_monster(monsters *monster);

void mons_check_pool(monsters *monster, const coord_def &oldpos,
                     killer_type killer = KILL_NONE, int killnum = -1);

void monster_cleanup(monsters *monster);

int dismiss_monsters(std::string pattern);

bool curse_an_item(bool decay_potions, bool quiet = false);


void monster_drop_ething(monsters *monster, bool mark_item_origins = false,
                         int owner_id = NON_ITEM);

bool monster_blink(monsters *monster, bool quiet = false);

bool simple_monster_message(const monsters *monster, const char *event,
                            msg_channel_type channel = MSGCH_PLAIN,
                            int param = 0,
                            description_level_type descrip = DESC_CAP_THE);

bool choose_any_monster(const monsters* mon);
monsters *choose_random_nearby_monster(
    int weight,
    bool (*suitable)(const monsters* mon) =
        choose_any_monster,
    bool in_sight = true,
    bool prefer_named = false, bool prefer_priest = false);

monsters *choose_random_monster_on_level(
    int weight,
    bool (*suitable)(const monsters* mon) =
        choose_any_monster,
    bool in_sight = true, bool near_by = false,
    bool prefer_named = false, bool prefer_priest = false);

bool swap_places(monsters *monster);
bool swap_places(monsters *monster, const coord_def &loc);
bool swap_check(monsters *monster, coord_def &loc, bool quiet = false);


std::string get_wounds_description(const monsters *monster, bool colour=false);
std::string get_wounds_description_sentence(const monsters *monster);
void print_wounds(const monsters *monster);
bool monster_descriptor(int which_class, mon_desc_type which_descriptor);

// Return your target, if it still exists and is visible to you.
monsters *get_current_target();

void mons_get_damage_level(const monsters*, std::string& desc,
                           mon_dam_level_type&);

void seen_monster(monsters *monster);

bool shift_monster(monsters *mon, coord_def p = coord_def(0, 0));

int mons_weapon_damage_rating(const item_def &launcher);
int mons_missile_damage(monsters *mons, const item_def *launch,
                        const item_def *missile);
int mons_pick_best_missile(monsters *mons, item_def **launcher,
                           bool ignore_melee = false);
int mons_thrown_weapon_damage(const item_def *weap,
                              bool only_returning_weapons = false);

int mons_natural_regen_rate(monsters *monster);

void mons_relocated(monsters *mons);
void mons_att_changed(monsters *mons);

bool can_go_straight(const coord_def& p1, const coord_def& p2,
                     dungeon_feature_type allowed);

bool is_item_jelly_edible(const item_def &item);

bool monster_random_space(const monsters *monster, coord_def& target,
                          bool forbid_sanctuary = false);
bool monster_random_space(monster_type mon, coord_def& target,
                          bool forbid_sanctuary = false);
void monster_teleport(monsters *monster, bool instan, bool silent = false);
void mons_clear_trapping_net(monsters *mon);

std::string summoned_poof_msg(const monsters* monster, bool plural = false);
std::string summoned_poof_msg(const int midx, const item_def &item);
std::string summoned_poof_msg(const monsters* monster, const item_def &item);

bool mons_reaped(actor *killer, monsters *victim);

actor* forest_near_enemy(const actor *mon);
void forest_message(const coord_def pos, const std::string msg,
                    msg_channel_type ch = MSGCH_PLAIN);
void forest_damage(const actor *mon);
#endif
