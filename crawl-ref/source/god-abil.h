/**
 * @file
 * @brief God-granted abilities.
**/

#ifndef GODABIL_H
#define GODABIL_H

#include "enum.h"
#include "item-prop-enum.h" // brand_type
#include "spl-cast.h"

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

const char * const GOZAG_POTIONS_KEY = "gozag_potions%d";
const char * const GOZAG_PRICE_KEY = "gozag_price%d";

const char * const GOZAG_SHOPKEEPER_NAME_KEY = "gozag_shopkeeper_%d";
const char * const GOZAG_SHOP_TYPE_KEY       = "gozag_shop_type_%d";
const char * const GOZAG_SHOP_SUFFIX_KEY     = "gozag_shop_suffix_%d";
const char * const GOZAG_SHOP_COST_KEY       = "gozag_shop_cost_%d";

#define GOZAG_GOLD_AURA_KEY "gozag_gold_aura_amount"
#define GOZAG_POTION_PETITION_AMOUNT 400
#define GOZAG_SHOP_BASE_MULTIPLIER 100
#define GOZAG_SHOP_MOD_MULTIPLIER 25
#define GOZAG_BRIBE_AMOUNT 3000
#define GOZAG_MAX_BRIBABILITY 8
#define GOZAG_MAX_POTIONS 3

#define USKAYAW_AUDIENCE_TIMER "uskayaw_audience_timer"
#define USKAYAW_BOND_TIMER "uskayaw_bond_timer"
#define USKAYAW_NUM_MONSTERS_HURT "uskayaw_num_monsters_hurt"
#define USKAYAW_MONSTER_HURT_VALUE "uskayaw_monster_hurt_value"
#define USKAYAW_AUT_SINCE_PIETY_GAIN "uskayaw_aut_since_piety_gain"

struct bolt;
class stack_iterator;

typedef FixedVector<int, NUM_RECITE_TYPES> recite_counts;

bool can_do_capstone_ability(god_type god);
bool bless_weapon(god_type god, brand_type brand, colour_t colour);
bool zin_donate_gold();
string zin_recite_text(const int seed, const int prayertype, int step);
bool zin_check_able_to_recite(bool quiet = false);
recite_eligibility zin_check_recite_to_single_monster(const monster *mon,
                                                  recite_counts &eligibility,
                                                  bool quiet = false);
int zin_check_recite_to_monsters(bool quiet = false);
bool zin_recite_to_single_monster(const coord_def& where);
int zin_recite_power();
bool zin_vitalisation();
void zin_remove_divine_stamina();
bool zin_remove_all_mutations();
void zin_sanctuary();

void tso_divine_shield();
void tso_remove_divine_shield();

void elyvilon_purification();
bool elyvilon_divine_vigour();
void elyvilon_remove_divine_vigour();

bool vehumet_supports_spell(spell_type spell);

void sif_do_channel_energy(int pow);

bool trog_burn_spellbooks();
void trog_do_trogs_hand(int power);
void trog_remove_trogs_hand();

bool jiyva_remove_bad_mutation();

bool beogh_can_gift_items_to(const monster* mons, bool quiet = true);
bool beogh_gift_item();
bool beogh_resurrect();

bool yred_injury_mirror();
void yred_make_enslaved_soul(monster* mon, bool force_hostile = false);

bool kiku_receive_corpses(int pow);
bool kiku_take_corpse();
bool kiku_gift_necronomicon();

bool fedhas_passthrough_class(const monster_type mc);
bool fedhas_passthrough(const monster* target);
bool fedhas_passthrough(const monster_info* target);
struct mgen_data;
int place_ring(vector<coord_def>& ring_points,
               const coord_def& origin,
               mgen_data prototype,
               int n_arcs,
               int arc_occupancy,
               int& seen_count);
// Collect lists of points that are within LOS, unoccupied, and not solid
// (walls/statues).
void collect_radius_points(vector<vector<coord_def> > &radius_points,
                           const coord_def &origin, los_type los);
int fedhas_fungal_bloom();
spret_type fedhas_sunlight(bool fail = false);
void process_sunlights(bool future = false);
bool prioritise_adjacent(const coord_def& target, vector<coord_def>& candidates);
bool fedhas_plant_ring_from_fruit();
int fedhas_rain(const coord_def &target);
int count_corpses_in_los(vector<stack_iterator> *positions);
int fedhas_check_corpse_spores(bool quiet = false);
int fedhas_corpse_spores(beh_type attitude = BEH_FRIENDLY);
bool mons_is_evolvable(const monster* mon);
bool fedhas_check_evolve_flora(bool quiet);
spret_type fedhas_evolve_flora(bool fail);

void lugonu_bend_space();

void cheibriados_time_bend(int pow);
void cheibriados_temporal_distortion();
bool cheibriados_slouch();
void cheibriados_time_step(int pow);
bool ashenzari_transfer_knowledge();
bool ashenzari_end_transfer(bool finished = false, bool force = false);
bool ashenzari_curse_item(int num_rc);

bool can_convert_to_beogh();
void spare_beogh_convert();

bool dithmenos_shadow_step();

int gozag_potion_price();
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

spret_type qazlal_upheaval(coord_def target, bool quiet = false,
                           bool fail = false);
spret_type qazlal_elemental_force(bool fail);
bool qazlal_disaster_area();

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

bool pakellas_check_quick_charge(bool quiet);
int pakellas_effective_hex_power(int pow);
int pakellas_surge_devices();

bool uskayaw_stomp();
bool uskayaw_line_pass();
spret_type uskayaw_grand_finale(bool fail);

bool hepliaklqana_choose_ancestor_type(int ancestor_type);
spret_type hepliaklqana_idealise(bool fail);
spret_type hepliaklqana_transference(bool fail);
void hepliaklqana_choose_identity();

bool ieoh_jian_heavenly_blade();
#endif
