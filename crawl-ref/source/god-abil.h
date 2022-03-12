/**
 * @file
 * @brief God-granted abilities.
**/

#pragma once

#include "ability-type.h"
#include "beh-type.h"
#include "enum.h"
#include "god-type.h"
#include "item-prop-enum.h" // brand_type
#include "los-type.h"
#include "recite-eligibility.h"
#include "recite-type.h"
#include "spl-cast.h"

class dist;

#define BEOGH_RANGE_WPN_GIFT_KEY "given beogh range weapon"
#define BEOGH_MELEE_WPN_GIFT_KEY "given beogh melee weapon"
#define BEOGH_ARM_GIFT_KEY "given beogh armour"
#define BEOGH_SH_GIFT_KEY "given beogh shield"

#define AVAILABLE_SAC_KEY "available_sacrifices"
#define HEALTH_SAC_KEY "current_health_sacrifice"
#define ESSENCE_SAC_KEY "current_essence_sacrifice"
#define PURITY_SAC_KEY "current_purity_sacrifice"
#define ARCANA_SAC_KEY "current_arcane_sacrifices"

#define RU_SACRIFICE_PROGRESS_KEY "ru_progress_to_next_sacrifice"
#define RU_SACRIFICE_DELAY_KEY "ru_sacrifice_delay"
#define RU_SACRIFICE_PENALTY_KEY "ru_sacrifice_penalty"
#define RU_SAC_XP_LEVELS 2

#define OKAWARU_DUEL_TARGET_KEY "okawaru_duel_target"
#define OKAWARU_DUEL_CURRENT_KEY "okawaru_duel_current"
#define OKAWARU_DUEL_ABANDONED_KEY "okawaru_duel_abandoned"

const char * const GOZAG_POTIONS_KEY = "gozag_potions%d";
const char * const GOZAG_PRICE_KEY = "gozag_price%d";

const char * const GOZAG_SHOPKEEPER_NAME_KEY = "gozag_shopkeeper_%d";
const char * const GOZAG_SHOP_TYPE_KEY       = "gozag_shop_type_%d";
const char * const GOZAG_SHOP_SUFFIX_KEY     = "gozag_shop_suffix_%d";
const char * const GOZAG_SHOP_COST_KEY       = "gozag_shop_cost_%d";

#define GOZAG_GOLD_AURA_KEY "gozag_gold_aura_amount"
#define GOZAG_GOLD_AURA_MAX 10
#define GOZAG_POTION_PETITION_AMOUNT 400
#define GOZAG_SHOP_BASE_MULTIPLIER 100
#define GOZAG_SHOP_MOD_MULTIPLIER 25
#define GOZAG_BRIBE_AMOUNT 3000
#define GOZAG_MAX_BRIBABILITY 8
#define GOZAG_MAX_POTIONS 3
#define GOZAG_MAX_SHOPS 3

#define USKAYAW_AUDIENCE_TIMER "uskayaw_audience_timer"
#define USKAYAW_BOND_TIMER "uskayaw_bond_timer"
#define USKAYAW_NUM_MONSTERS_HURT "uskayaw_num_monsters_hurt"
#define USKAYAW_MONSTER_HURT_VALUE "uskayaw_monster_hurt_value"
#define USKAYAW_AUT_SINCE_PIETY_GAIN "uskayaw_aut_since_piety_gain"

#define WU_JIAN_HEAVENLY_STORM_KEY "wu_jian_heavenly_storm_amount"
#define WU_JIAN_HEAVENLY_STORM_INITIAL 5
#define WU_JIAN_HEAVENLY_STORM_MAX 15

#define AVAILABLE_CURSE_KEY "available_curses"
#define CURSE_KNOWLEDGE_KEY "current_curse_offer"

#define ASHENZARI_CURSE_PROGRESS_KEY "ashenzari_progress_to_next_curse"
#define ASHENZARI_CURSE_DELAY_KEY "ashenzari_curse_delay"
#define ASHENZARI_BASE_PIETY 2
#define ASHENZARI_PIETY_SCALE 168

struct bolt;
class stack_iterator;

typedef FixedVector<int, NUM_RECITE_TYPES> recite_counts;

bool can_do_capstone_ability(god_type god);
bool bless_weapon(god_type god, brand_type brand, colour_t colour);
bool zin_donate_gold();
string zin_recite_text(const int seed, const int prayertype, int step);
bool zin_check_able_to_recite(bool quiet = false);
vector<coord_def> find_recite_targets();
recite_eligibility zin_check_recite_to_single_monster(const monster *mon,
                                                  recite_counts &eligibility,
                                                  bool quiet = false);
int zin_check_recite_to_monsters(bool quiet = false);
bool zin_recite_to_single_monster(const coord_def& where);
int zin_recite_power();
bool zin_vitalisation();
void zin_remove_divine_stamina();
spret zin_imprison(const coord_def& target, bool fail);
void zin_sanctuary();

void tso_divine_shield();
void tso_remove_divine_shield();

void elyvilon_purification();
void elyvilon_divine_vigour();
void elyvilon_remove_divine_vigour();

bool vehumet_supports_spell(spell_type spell);

void sif_do_channel_energy(int pow);

void trog_do_trogs_hand(int power);
void trog_remove_trogs_hand();

bool given_gift(const monster* mons);
bool beogh_can_gift_items_to(const monster* mons, bool quiet = true);
bool beogh_gift_item();
bool beogh_resurrect();

bool yred_can_bind_soul(monster* mon);
void yred_make_bound_soul(monster* mon, bool force_hostile = false);

bool kiku_gift_capstone_spells();

bool fedhas_passthrough_class(const monster_type mc);
bool fedhas_passthrough(const monster* target);
bool fedhas_passthrough(const monster_info* target);

void lugonu_bend_space();

void cheibriados_time_bend(int pow);
void cheibriados_temporal_distortion();
spret cheibriados_slouch(bool fail);
void cheibriados_time_step(int pow);

void ashenzari_offer_new_curse();
bool ashenzari_curse_item();
bool ashenzari_uncurse_item();
string desc_curse_skills(const CrawlStoreValue& curse);
string curse_abbr(const CrawlStoreValue& curse);
string curse_name(const CrawlStoreValue& curse);
const vector<skill_type>& curse_skills(const CrawlStoreValue& curse);

bool can_convert_to_beogh();
void spare_beogh_convert();

spret dithmenos_shadow_step(bool fail);

bool gozag_setup_potion_petition(bool quiet = false);
bool gozag_potion_petition();
int gozag_price_for_shop(bool max = false);
bool gozag_setup_call_merchant(bool quiet = false);
bool gozag_call_merchant();
branch_type gozag_fixup_branch(branch_type branch);
int gozag_type_bribable(monster_type type);
bool gozag_branch_bribable(branch_type branch);
void gozag_deduct_bribe(branch_type br, int amount);
bool gozag_check_bribe_branch(bool quiet = false);
bool gozag_bribe_branch();

spret qazlal_upheaval(coord_def target, bool quiet = false,
                           bool fail = false, dist *player_target=nullptr);
vector<coord_def> find_elemental_targets();
spret qazlal_elemental_force(bool fail);
spret qazlal_disaster_area(bool fail);

void init_sac_index();
int get_sacrifice_piety(ability_type sac, bool include_skill = true);
void ru_offer_new_sacrifices();
string ru_sac_text(ability_type sac);
string ru_sacrifice_description(ability_type sac);
bool ru_do_sacrifice(ability_type sac);
bool ru_reject_sacrifices(bool forced_rejection = false);
void ru_reset_sacrifice_timer(bool clear_timer = false,
                              bool faith_penalty = false);
bool will_ru_retaliate();
void ru_do_retribution(monster* mons, int damage);
void ru_draw_out_power();
bool ru_power_leap();
int cell_has_valid_target(coord_def where);
bool ru_apocalypse();
string ru_sacrifice_vector(ability_type sac);

bool uskayaw_stomp();
bool uskayaw_line_pass();
spret uskayaw_grand_finale(bool fail);

bool hepliaklqana_choose_ancestor_type(int ancestor_type);
spret hepliaklqana_idealise(bool fail);
spret hepliaklqana_transference(bool fail);
void hepliaklqana_choose_identity();

bool wu_jian_can_wall_jump_in_principle(const coord_def& target);
bool wu_jian_can_wall_jump(const coord_def& target, string &error_ret);
bool wu_jian_do_wall_jump(coord_def targ);
spret wu_jian_wall_jump_ability();
void wu_jian_heavenly_storm();

bool okawaru_duel_active();
spret okawaru_duel(const coord_def& target, bool fail);
void okawaru_remove_heroism();
void okawaru_remove_finesse();
void okawaru_end_duel();

vector<coord_def> find_slimeable_walls();
spret jiyva_oozemancy(bool fail);
void jiyva_end_oozemancy();
