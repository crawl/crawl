/**
 * @file
 * @brief Misc monster related functions.
**/


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
    MDAM_DEAD,
};

enum temperature_level
{
    TEMP_MIN = 1, // Minimum (and starting) temperature. Not any warmer than bare rock.
    TEMP_COLD = 3,
    TEMP_COOL = 5,
    TEMP_ROOM = 7,
    TEMP_WARM = 9, // Warmer than most creatures.
    TEMP_HOT = 11,
    TEMP_FIRE = 13, // Hot enough to ignite paper around you.
    TEMP_MAX = 15, // Maximum temperature. As hot as lava!
};

enum temperature_effect
{
    LORC_LAVA_BOOST,
    LORC_FIRE_BOOST,
    LORC_STONESKIN,
    LORC_SLOW_MOVE,
    LORC_COLD_VULN,
    LORC_PASSIVE_HEAT,
    LORC_HEAT_AURA,
    LORC_FAST_MOVE,
    LORC_NO_SCROLLS,
    LORC_FIRE_RES_I,
    LORC_FIRE_RES_II,
    LORC_FIRE_RES_III,
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
#define ROTTING_CORPSE   99

#define YOU_KILL(x) ((x) == KILL_YOU || (x) == KILL_YOU_MISSILE \
                     || (x) == KILL_YOU_CONF)
#define MON_KILL(x) ((x) == KILL_MON || (x) == KILL_MON_MISSILE)

#define SAME_ATTITUDE(x) (x->friendly()       ? BEH_FRIENDLY : \
                          x->good_neutral()   ? BEH_GOOD_NEUTRAL : \
                          x->strict_neutral() ? BEH_STRICT_NEUTRAL : \
                          x->neutral()        ? BEH_NEUTRAL \
                                              : BEH_HOSTILE)

#define MONST_INTERESTING(x) (x->flags & MF_INTERESTING)


const item_def *give_mimic_item(monster* mimic);
dungeon_feature_type get_mimic_feat(const monster* mimic);
bool feature_mimic_at(const coord_def &c);
item_def* item_mimic_at(const coord_def &c);
bool mimic_at(const coord_def &c);

void alert_nearby_monsters(void);

beh_type attitude_creation_behavior(mon_attitude_type att);
beh_type actual_same_attitude(const monster& base);

enum poly_power_type
{
    PPT_LESS,
    PPT_MORE,
    PPT_SAME,
};

void change_monster_type(monster* mons, monster_type targetc);
bool monster_polymorph(monster* mons, monster_type targetc,
                       poly_power_type power = PPT_SAME,
                       bool force_beh = false);

int monster_die(monster* mons, const actor *killer, bool silent = false,
                bool wizard = false, bool fake = false);

int monster_die(monster* mons, killer_type killer,
                int killer_index, bool silent = false, bool wizard = false,
                bool fake = false);

int mounted_kill(monster* daddy, monster_type mc, killer_type killer,
                int killer_index);

monster_type fill_out_corpse(const monster* mons,
                             monster_type mtype,
                             item_def& corpse,
                             bool force_corpse = false);

bool explode_corpse(item_def& corpse, const coord_def& where);

int place_monster_corpse(const monster* mons, bool silent,
                         bool force = false);

void slimify_monster(monster* mons, bool hostile = false);

bool mon_can_be_slimified(monster* mons);

void corrode_monster(monster* mons, const actor* evildoer);

void mons_check_pool(monster* mons, const coord_def &oldpos,
                     killer_type killer = KILL_NONE, int killnum = -1);

void monster_cleanup(monster* mons);

void unawaken_vines(const monster* mons, bool quiet);

int dismiss_monsters(string pattern);
void zap_los_monsters(bool items_also);

bool curse_an_item(bool quiet = false);

bool is_any_item(const item_def& item);
void monster_drop_things(
    monster* mons,
    bool mark_item_origins = false,
    bool (*suitable)(const item_def& item) = is_any_item,
    int owner_id = NON_ITEM);

bool monster_blink(monster* mons, bool quiet = false);

bool simple_monster_message(const monster* mons, const char *event,
                            msg_channel_type channel = MSGCH_PLAIN,
                            int param = 0,
                            description_level_type descrip = DESC_THE);

bool choose_any_monster(const monster* mon);
monster *choose_random_nearby_monster(
    int weight,
    bool (*suitable)(const monster* mon) =
        choose_any_monster,
    bool in_sight = true,
    bool prefer_named = false, bool prefer_priest = false);

monster *choose_random_monster_on_level(
    int weight,
    bool (*suitable)(const monster* mon) =
        choose_any_monster,
    bool in_sight = true, bool near_by = false,
    bool prefer_named = false, bool prefer_priest = false);

bool swap_places(monster* mons, const coord_def &loc);
bool swap_check(monster* mons, coord_def &loc, bool quiet = false);

void print_wounds(const monster* mons);

// Return your target, if it still exists and is visible to you.
monster *get_current_target();

mon_dam_level_type mons_get_damage_level(const monster* mons);

string get_damage_level_string(mon_holy_type holi, mon_dam_level_type mdam);

void seen_monster(monster* mons);

bool shift_monster(monster* mon, coord_def p = coord_def(0, 0));

int mons_weapon_damage_rating(const item_def &launcher);
int mons_missile_damage(monster* mons, const item_def *launch,
                        const item_def *missile);
int mons_usable_missile(monster* mons, item_def **launcher);

int mons_natural_regen_rate(monster* mons);
int mons_off_level_regen_rate(monster* mons);

void mons_relocated(monster* mons);
void mons_att_changed(monster* mons);

bool can_go_straight(const monster* mon, const coord_def& p1,
                     const coord_def& p2);

bool is_item_jelly_edible(const item_def &item);

bool monster_space_valid(const monster* mons, coord_def target,
                         bool forbid_sanctuary);
void monster_teleport(monster* mons, bool instan, bool silent = false);
void mons_clear_trapping_net(monster* mon);

string summoned_poof_msg(const monster* mons, bool plural = false);
string summoned_poof_msg(const int midx, const item_def &item);
string summoned_poof_msg(const monster* mons, const item_def &item);

struct bolt;

void setup_spore_explosion(bolt & beam, const monster& origin);

bool mons_avoids_cloud(const monster* mons, int cloud_num,
                       bool placement = false);

void debuff_monster(monster* mons);
int exp_rate(int killer);
int count_monsters(monster_type mtyp, bool friendlyOnly);
int count_allies();
void record_monster_defeat(monster* mons, killer_type killer);

int temperature();
int temperature_last();
void temperature_check();
void temperature_increment(float degree);
void temperature_decrement(float degree);
void temperature_changed(float change);
void temperature_decay();
bool temperature_tier(int which);
bool temperature_effect(int which);
int temperature_colour(int temp);
string temperature_string(int temp);
string temperature_text(int temp);
#endif
