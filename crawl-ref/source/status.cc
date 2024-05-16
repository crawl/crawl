#include "AppHdr.h"

#include "status.h"

#include "ability.h"
#include "areas.h"
#include "art-enum.h" // bearserk
#include "artefact.h"
#include "branch.h"
#include "cloud.h"
#include "dungeon.h" // DESCENT_STAIRS_KEY
#include "duration-type.h"
#include "env.h"
#include "evoke.h"
#include "fight.h" // weapon_cleaves
#include "god-abil.h"
#include "god-passive.h"
#include "item-prop.h"
#include "level-state-type.h"
#include "mon-transit.h" // untag_followers() in duration-data
#include "mutation.h"
#include "options.h"
#include "orb.h" // orb_limits_translocation in fill_status_info
#include "player-stats.h"
#include "random.h" // for midpoint_msg.offset() in duration-data
#include "religion.h"
#include "spl-damage.h" // COUPLING_TIME_KEY
#include "spl-summoning.h" // NEXT_DOOM_HOUND_KEY in duration-data
#include "spl-transloc.h" // for you_teleport_now() in duration-data
#include "stairs.h" // rise_through_ceiling
#include "state.h" // crawl_state
#include "stringutil.h"
#include "throw.h"
#include "transform.h"
#include "traps.h"
#include "zot.h" // bezotting_level

#include "duration-data.h"

static int duration_index[NUM_DURATIONS];

void init_duration_index()
{
    COMPILE_CHECK(ARRAYSZ(duration_data) == NUM_DURATIONS);
    for (int i = 0; i < NUM_DURATIONS; ++i)
        duration_index[i] = -1;

    for (unsigned i = 0; i < ARRAYSZ(duration_data); ++i)
    {
        duration_type dur = duration_data[i].dur;
        ASSERT_RANGE(dur, 0, NUM_DURATIONS);
        // Catch redefinitions.
        ASSERT(duration_index[dur] == -1);
        duration_index[dur] = i;
    }
}

static const duration_def* _lookup_duration(duration_type dur)
{
    ASSERT_RANGE(dur, 0, NUM_DURATIONS);
    if (duration_index[dur] == -1)
        return nullptr;
    else
        return &duration_data[duration_index[dur]];
}

const char *duration_name(duration_type dur)
{
    return _lookup_duration(dur)->name();
}

bool duration_dispellable(duration_type dur)
{
    return _lookup_duration(dur)->duration_has_flag(D_DISPELLABLE);
}

static int _bad_ench_colour(int lvl, int orange, int red)
{
    if (lvl >= red)
        return RED;
    else if (lvl >= orange)
        return LIGHTRED;

    return YELLOW;
}

static int _dur_colour(int exp_colour, bool expiring)
{
    if (expiring)
        return exp_colour;
    else
    {
        switch (exp_colour)
        {
        case GREEN:
            return LIGHTGREEN;
        case BLUE:
            return LIGHTBLUE;
        case MAGENTA:
            return LIGHTMAGENTA;
        case LIGHTGREY:
            return WHITE;
        default:
            return exp_colour;
        }
    }
}

static void _mark_expiring(status_info& inf, bool expiring)
{
    if (expiring)
    {
        if (!inf.short_text.empty())
            inf.short_text += " (expiring)";
        if (!inf.long_text.empty())
            inf.long_text = "Expiring: " + inf.long_text;
    }
}

static string _ray_text()
{
    const int n_plusses = max(you.attribute[ATTR_SEARING_RAY] - 1, 0);
    return "Ray" + string(n_plusses, '+');
}

static vector<string> _charge_strings = { "Charge-", "Charge/",
                                          "Charge|", "Charge\\"};

static string _charge_text()
{
    static int charge_index = 0;
    charge_index = (charge_index + 1) % 4;
    return _charge_strings[charge_index];
}

/**
 * Populate a status_info struct from the duration_data struct corresponding
 * to the given duration_type.
 *
 * @param[in] dur    The duration in question.
 * @param[out] inf   The status_info struct to be filled.
 * @return           Whether a duration_data struct was found.
 */
static bool _fill_inf_from_ddef(duration_type dur, status_info& inf)
{
    const duration_def* ddef = _lookup_duration(dur);
    if (!ddef)
        return false;

    inf.light_colour = ddef->light_colour;
    inf.light_text   = ddef->light_text;
    inf.short_text   = ddef->short_text;
    inf.long_text    = ddef->long_text;
    if (ddef->duration_has_flag(D_EXPIRES))
    {
        inf.light_colour = _dur_colour(inf.light_colour, dur_expiring(dur));
        _mark_expiring(inf, dur_expiring(dur));
    }

    return true;
}

static void _describe_airborne(status_info& inf);
static void _describe_glow(status_info& inf);
static void _describe_regen(status_info& inf);
static void _describe_speed(status_info& inf);
static void _describe_poison(status_info& inf);
static void _describe_transform(status_info& inf);
static void _describe_stat_zero(status_info& inf, stat_type st);
static void _describe_terrain(status_info& inf);
static void _describe_invisible(status_info& inf);
static void _describe_zot(status_info& inf);
static void _describe_gem(status_info& inf);
static void _describe_rev(status_info& inf);

bool fill_status_info(int status, status_info& inf)
{
    inf = status_info();

    bool found = false;

    // Sort out inactive durations, and fill in data from duration_data
    // for the simple durations.
    if (status >= 0 && status < NUM_DURATIONS)
    {
        duration_type dur = static_cast<duration_type>(status);

        if (!you.duration[dur])
            return false;

        found = _fill_inf_from_ddef(dur, inf);
    }

    // Now treat special status types and durations, possibly
    // completing or overriding the defaults set above.
    switch (status)
    {
    case STATUS_DRACONIAN_BREATH:
    {
        if (!species::is_draconian(you.species) || you.experience_level < 7)
            break;

        inf.light_text = "Breath";

        const int num = draconian_breath_uses_available();
        if (num == 0)
            inf.light_colour = DARKGREY;
        else
        {
            inf.light_colour = LIGHTCYAN;
            if (num == 2)
                inf.light_text += "+";
            else if (num == 3)
                inf.light_text += "++";
        }
        break;
    }

    case STATUS_BLACK_TORCH:
        if (!you_worship(GOD_YREDELEMNUL))
            break;

        if (!yred_torch_is_raised())
        {
            if (yred_cannot_light_torch_reason().empty())
            {
                inf.light_colour = DARKGRAY;
                inf.light_text = "Torch";
            }
        }
        else
        {
            inf.light_colour = MAGENTA;

            if (player_has_ability(ABIL_YRED_HURL_TORCHLIGHT))
            {
                inf.light_text = make_stringf("Torch (%d)",
                                    yred_get_torch_power());
            }
            else
                inf.light_text = "Torch";
        }
    break;

    case STATUS_CORROSION:
        // No blank or double lights
        if (you.corrosion_amount() == 0 || you.duration[DUR_CORROSION])
            break;
        _fill_inf_from_ddef(DUR_CORROSION, inf);
        // Intentional fallthrough
    case DUR_CORROSION:
        inf.light_text = make_stringf("Corr (%d)",
                          (-1 * you.corrosion_amount()));
        break;

    case DUR_FLAYED:
        inf.light_text = make_stringf("Flay (%d)",
                          (-1 * you.props[FLAY_DAMAGE_KEY].get_int()));
        break;

    case DUR_BERSERK:
        if (player_equip_unrand(UNRAND_BEAR_SPIRIT))
            inf.light_text = "Bearserk";
        break;

    case STATUS_NO_POTIONS:
        if (you.duration[DUR_NO_POTIONS] || player_in_branch(BRANCH_COCYTUS))
        {
            inf.light_colour = !you.can_drink(false) ? DARKGREY : RED;
            inf.light_text   = "-Potion";
            inf.short_text   = "unable to drink";
            inf.long_text    = "You cannot drink potions.";
        }
        break;

    case DUR_SWIFTNESS:
        if (you.attribute[ATTR_SWIFTNESS] < 0)
        {
            inf.light_text   = "-Swift";
            inf.light_colour = RED;
            inf.short_text   = "unswift";
            inf.long_text    = "You are covering ground slowly.";
        }
        break;

    case STATUS_ZOT:
        _describe_zot(inf);
        break;

    case STATUS_GEM:
        _describe_gem(inf);
        break;

    case STATUS_REV:
        _describe_rev(inf);
        break;

    case STATUS_AIRBORNE:
        _describe_airborne(inf);
        break;

    case STATUS_BEHELD:
        if (you.beheld())
        {
            inf.light_colour = RED;
            inf.light_text   = "Mesm";
            inf.short_text   = "mesmerised";
            inf.long_text    = "You are mesmerised.";
        }
        break;

    case STATUS_PEEKING:
        if (crawl_state.game_is_descent() && !env.properties.exists(DESCENT_STAIRS_KEY)
            && you.elapsed_time > 0)
        {
            inf.light_colour = WHITE;
            inf.light_text   = "Peek";
            inf.short_text   = "peeking";
            inf.long_text    = "You are peeking down the stairs.";
        }
        break;

    case STATUS_IN_DEBT:
        if (you.props.exists(DESCENT_DEBT_KEY))
        {
            inf.light_colour = RED;
            inf.light_text = make_stringf("Debt (%d)",
                          you.props[DESCENT_DEBT_KEY].get_int());
            inf.short_text   = "in debt";
            inf.long_text    = "You are in debt. Gold earned will pay it off.";
        }
        break;

    case STATUS_CONTAMINATION:
        _describe_glow(inf);
        break;

    case STATUS_BACKLIT:
        if (you.backlit())
        {
            inf.short_text = "glowing";
            inf.long_text  = "You are glowing.";
        }
        break;

    case STATUS_UMBRA:
        if (you.umbra())
        {
            inf.short_text   = "wreathed by umbra";
            inf.long_text    = "You are wreathed by an umbra.";
        }
        break;

    case STATUS_NET:
        if (you.attribute[ATTR_HELD])
        {
            inf.light_colour = RED;
            inf.light_text   = "Held";
            inf.short_text   = "held";
            inf.long_text    = make_stringf("You are %s.", held_status());
        }
        break;

    case STATUS_ALIVE_STATE:
        if (you.has_mutation(MUT_VAMPIRISM))
        {
            if (!you.vampire_alive)
            {
                inf.light_colour = LIGHTRED;
                inf.light_text = "Bloodless";
                inf.short_text = "bloodless";
            }
            else
            {
                inf.light_colour = GREEN;
                inf.light_text = "Alive";
            }
        }
        break;

    case STATUS_REGENERATION:
        // DUR_TROGS_HAND + some vampire and non-healing stuff
        _describe_regen(inf);
        break;

    case STATUS_SPEED:
        _describe_speed(inf);
        break;

    case STATUS_LIQUEFIED:
    {
        if (you.liquefied_ground() || you.duration[DUR_LIQUEFYING])
        {
            inf.light_colour = BROWN;
            inf.light_text   = "SlowM";
            inf.short_text   = "slowed movement";
            inf.long_text    = "Your movement is slowed on this liquid ground.";
        }
        break;
    }

    case STATUS_AUGMENTED:
    {
        int level = augmentation_amount();

        if (level > 0)
        {
            inf.light_colour = (level == 3) ? WHITE :
                               (level == 2) ? LIGHTBLUE
                                            : BLUE;

            inf.light_text = "Aug";
        }
        break;
    }

    case DUR_CONFUSING_TOUCH:
    {
        inf.long_text = you.hands_act("are", "glowing red.");
        break;
    }

    case DUR_SLIMIFY:
    {
        inf.long_text = you.hands_act("are", "covered in slime.");
        break;
    }

    case DUR_POISONING:
        _describe_poison(inf);
        break;

    case DUR_POWERED_BY_DEATH:
    {
        const int pbd_str = you.props[POWERED_BY_DEATH_KEY].get_int();
        if (pbd_str > 0)
        {
            inf.light_colour = LIGHTMAGENTA;
            inf.light_text   = make_stringf("Regen (%d)", pbd_str);
        }
        break;
    }

    case DUR_RAMPAGE_HEAL:
    {
        const int rh_pwr = you.props[RAMPAGE_HEAL_KEY].get_int();
        if (rh_pwr > 0)
        {
            const int rh_lvl = you.get_mutation_level(MUT_ROLLPAGE);
            inf.light_colour = rh_lvl < 2 ? LIGHTBLUE : LIGHTMAGENTA;
            inf.light_text   = make_stringf(rh_lvl < 2 ? "MPRegen (%d)"
                                                       : "Regen (%d)", rh_pwr);
        }
        break;
    }

    case STATUS_INVISIBLE:
        _describe_invisible(inf);
        break;

    case STATUS_MANUAL:
    {
        string skills = manual_skill_names();
        if (!skills.empty())
        {
            inf.short_text = "studying " + manual_skill_names(true);
            inf.long_text = "You are studying " + skills + ".";
        }
        break;
    }

    case DUR_TRANSFORMATION:
        _describe_transform(inf);
        break;

    case STATUS_STR_ZERO:
        _describe_stat_zero(inf, STAT_STR);
        break;
    case STATUS_INT_ZERO:
        _describe_stat_zero(inf, STAT_INT);
        break;
    case STATUS_DEX_ZERO:
        _describe_stat_zero(inf, STAT_DEX);
        break;

    case STATUS_CONSTRICTED:
        if (you.is_constricted())
        {
            // Our constrictor isn't, valid so don't report this status.
            if (you.has_invalid_constrictor())
                return false;

            const monster * const cstr = monster_by_mid(you.constricted_by);
            ASSERT(cstr);

            inf.light_colour = YELLOW;
            inf.light_text   = "Constr";
            inf.short_text   = "constricted";
        }
        break;

    case STATUS_TERRAIN:
        _describe_terrain(inf);
        break;

    // Also handled by DUR_SILENCE, see duration-data.h
    case STATUS_SILENCE:
        if (silenced(you.pos()) && !you.duration[DUR_SILENCE])
        {
            // Only display the status light if not using the noise bar.
            if (Options.equip_bar)
            {
                inf.light_colour = LIGHTRED;
                inf.light_text   = "Sil";
            }
            inf.short_text   = "silenced";
            inf.long_text    = "You are silenced.";
        }
        if (Options.equip_bar && you.duration[DUR_SILENCE])
        {
            inf.light_colour = LIGHTMAGENTA;
            inf.light_text = "Sil";
        }
        break;

    case STATUS_SERPENTS_LASH:
        if (you.attribute[ATTR_SERPENTS_LASH] > 0)
        {
            inf.light_colour = WHITE;
            inf.light_text
               = make_stringf("Lash (%u)",
                              you.attribute[ATTR_SERPENTS_LASH]);
            inf.short_text = "serpent's lash";
            inf.long_text = "You are moving at supernatural speed.";
        }
        break;

    case STATUS_HEAVENLY_STORM:
        if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
        {
            inf.light_colour = WHITE;
            inf.light_text
                = make_stringf("Heavenly (%d)",
                               you.props[WU_JIAN_HEAVENLY_STORM_KEY].get_int());
        }
        break;

    case DUR_FUGUE:
    {
        int fugue_pow = you.props[FUGUE_KEY].get_int();
        // Hey now / you're a damned star / get your fugue on / go slay
        const char* fugue_star = fugue_pow == FUGUE_MAX_STACKS ? "*" : "";
        inf.light_text = make_stringf("Fugue (%s%u%s)",
                                      fugue_star, fugue_pow, fugue_star);
    }
    break;

    case DUR_STICKY_FLAME:
    {
        int intensity = you.props[STICKY_FLAME_POWER_KEY].get_int();

        // These thresholds are fairly arbitrary and likely could use adjusting.
        if (intensity >= 13)
        {
            inf.light_colour = LIGHTRED;
            inf.light_text = "Fire++";
        }
        else if (intensity > 7)
            inf.light_text = "Fire+";
        else
            inf.light_text = "Fire";
    }

    case STATUS_BEOGH:
        if (env.level_state & LSTATE_BEOGH && can_convert_to_beogh())
        {
            inf.light_colour = WHITE;
            inf.light_text = "Beogh";
        }
        break;

    case DUR_WATER_HOLD:
        inf.light_text   = "Engulf";
        if (you.res_water_drowning())
        {
            inf.short_text   = "engulfed";
            inf.long_text    = "You are engulfed.";
            inf.light_colour = DARKGREY;
        }
        else
        {
            inf.short_text   = "engulfed (cannot breathe)";
            inf.long_text    = "You are engulfed and unable to breathe.";
            inf.light_colour = RED;
        }
        break;

    case STATUS_DRAINED:
    {
        const int drain_perc = 100 * -you.hp_max_adj_temp / get_real_hp(false, false);

        if (drain_perc >= 50)
        {
            inf.light_colour = MAGENTA;
            inf.light_text   = "Drain";
            inf.short_text   = "extremely drained";
            inf.long_text    = "Your life force is extremely drained.";
        }
        else if (drain_perc >= 30)
        {
            inf.light_colour = RED;
            inf.light_text   = "Drain";
            inf.short_text   = "very heavily drained";
            inf.long_text    = "Your life force is very heavily drained.";
        }
        else if (drain_perc >= 20)
        {
            inf.light_colour = LIGHTRED;
            inf.light_text   = "Drain";
            inf.short_text   = "heavily drained";
            inf.long_text    = "Your life force is heavily drained.";
        }
        else if (drain_perc >= 10)
        {
            inf.light_colour = YELLOW;
            inf.light_text   = "Drain";
            inf.short_text   = "drained";
            inf.long_text    = "Your life force is drained.";
        }
        else if (you.hp_max_adj_temp)
        {
            inf.light_colour = LIGHTGREY;
            inf.light_text   = "Drain";
            inf.short_text   = "lightly drained";
            inf.long_text    = "Your life force is lightly drained.";
        }
        break;

    }
    case STATUS_RAY:
        if (you.attribute[ATTR_SEARING_RAY] && can_cast_spells(true))
        {
            inf.light_colour = WHITE;
            inf.light_text   = _ray_text().c_str();
        }
        break;

    case STATUS_FLAME_WAVE:
        if (you.props.exists(FLAME_WAVE_KEY) && can_cast_spells(true))
        {
            // It's only possible to hit the prop = 0 case if we reprint the
            // screen after the spell was cast but before the end of the
            // player's turn, which mostly happens in webtiles. Great!
            const int lvl = max(you.props[FLAME_WAVE_KEY].get_int() - 1, 0);
            inf.light_colour = WHITE;
            inf.light_text   = "Wave" + string(lvl, '+');
        }
        break;

    case STATUS_DIG:
        if (you.digging)
        {
            inf.light_colour = WHITE;
            inf.light_text   = "Dig";
        }
        break;

    case STATUS_BRIBE:
    {
        int bribe = 0;
        vector<const char *> places;
        for (int i = 0; i < NUM_BRANCHES; i++)
        {
            branch_type br = gozag_fixup_branch(static_cast<branch_type>(i));

            if (branch_bribe[br] > 0)
            {
                if (player_in_branch(static_cast<branch_type>(i)))
                    bribe = branch_bribe[br];

                places.push_back(branches[static_cast<branch_type>(i)]
                                 .longname);
            }
        }

        if (bribe > 0)
        {
            inf.light_colour = (bribe >= 2000) ? WHITE :
                                (bribe >= 1000) ? LIGHTBLUE
                                                : BLUE;

            inf.light_text = "Bribe";
            inf.short_text = make_stringf("bribing [%s]",
                                           comma_separated_line(places.begin(),
                                                                places.end(),
                                                                ", ", ", ")
                                                                .c_str());
            inf.long_text = "You are bribing "
                             + comma_separated_line(places.begin(),
                                                    places.end())
                             + ".";
        }
        break;
    }

    case DUR_HORROR:
    {
        const int horror = you.props[HORROR_PENALTY_KEY].get_int();
        inf.light_text = make_stringf("Horr(%d)", -1 * horror);
        if (horror >= HORROR_LVL_OVERWHELMING)
        {
            inf.light_colour = RED;
            inf.short_text   = "overwhelmed with horror";
            inf.long_text    = "Horror overwhelms you!";
        }
        else if (horror >= HORROR_LVL_EXTREME)
        {
            inf.light_colour = LIGHTRED;
            inf.short_text   = "extremely horrified";
            inf.long_text    = "You are extremely horrified!";
        }
        else if (horror)
        {
            inf.light_colour = YELLOW;
            inf.short_text   = "horrified";
            inf.long_text    = "You are horrified!";
        }
        break;
    }

    case STATUS_CLOUD:
    {
        cloud_type cloud = cloud_type_at(you.pos());
        if (Options.cloud_status && cloud != CLOUD_NONE)
        {
            inf.light_text = "Cloud";
            // TODO: make the colour based on the cloud's color; requires elemental
            // status lights, though.
            const bool yours = cloud_is_yours_at(you.pos());
            const bool danger = cloud_damages_over_time(cloud, true, yours);
            inf.light_colour = danger ? LIGHTRED : DARKGREY;
        }
        break;
    }

    case DUR_CLEAVE:
    {
        const item_def* weapon = you.weapon();
        if (weapon && weapon_cleaves(*weapon))
            inf.light_colour = DARKGREY;
        break;
    }

    case STATUS_ORB:
    {
        if (player_has_orb())
        {
            inf.light_colour = LIGHTMAGENTA;
            inf.light_text = "Orb";
        }
        else if (player_equip_unrand(UNRAND_CHARLATANS_ORB))
        {
            inf.light_colour = LIGHTMAGENTA;
            inf.light_text = "Orb?";
        }
        else if (orb_limits_translocation())
        {
            inf.light_colour = MAGENTA;
            inf.light_text = "Orb";
        }

        break;
    }

    case STATUS_STILL_WINDS:
        if (env.level_state & LSTATE_STILL_WINDS)
        {
            inf.light_colour = BROWN;
            inf.light_text = "-Clouds";
        }
        break;

    case STATUS_MAXWELLS:
        if (you.props.exists(COUPLING_TIME_KEY) && can_cast_spells(true))
        {
            inf.light_colour = LIGHTCYAN;
            inf.light_text   = _charge_text().c_str();
        }
        break;

    case STATUS_DUEL:
        if (okawaru_duel_active())
        {
            inf.light_colour = WHITE;
            inf.light_text   = "Duel";
            inf.short_text   = "duelling";
            inf.long_text    = "You are engaged in single combat.";
        }
        break;

    case STATUS_CANINE_FAMILIAR_ACTIVE:
        if (canine_familiar_is_alive())
        {
            inf.light_colour = WHITE;
            inf.light_text   = "Dog";
            inf.short_text   = "inugami summoned";
            inf.long_text    = "Your inugami has been summoned.";
        }
        break;

    case STATUS_NO_SCROLL:
        if (you.duration[DUR_NO_SCROLLS] || you.duration[DUR_BRAINLESS]
            || player_in_branch(BRANCH_GEHENNA))
        {
            inf.light_colour = RED;
            inf.light_text   = "-Scroll";
            inf.short_text   = "unable to read";
            inf.long_text    = "You cannot read scrolls.";
        }
        break;

    case STATUS_RF_ZERO:
        if (!you.penance[GOD_IGNIS]
            || player_res_fire(false, true, true) < 0)
        {
            // XXX: would it be better to only show this
            // if you would otherwise have rF+ & to warn
            // on using a potion of resistance..?
            break;
        }
        inf.light_colour = RED;
        inf.light_text   = "rF0";
        inf.short_text   = "fire susceptible";
        inf.long_text    = "You cannot resist fire.";
        break;

    case STATUS_LOWERED_WL:
        // Don't double the light if under a duration
        if (!player_in_branch(BRANCH_TARTARUS) || you.duration[DUR_LOWERED_WL])
            break;
        if (player_in_branch(BRANCH_TARTARUS))
            _fill_inf_from_ddef(DUR_LOWERED_WL, inf);
        break;

    case DUR_FUSILLADE:
        if (!enough_mp(2, true))
            inf.light_colour = DARKGREY;
        break;

    default:
        if (!found)
        {
            inf.light_colour = RED;
            inf.light_text   = "Missing";
            inf.short_text   = "missing status";
            inf.long_text    = "Missing status description.";
            return false;
        }
        else
            break;
    }
    return true;
}

static colour_t _gem_light_colour(int d_aut_left)
{
    if (d_aut_left < 100)
        return LIGHTMAGENTA;
    if (d_aut_left < 250)
        return RED;
    if (d_aut_left < 500)
        return YELLOW;
    return WHITE;
}

static void _describe_gem(status_info& inf)
{
    if (!Options.always_show_gems || !gem_clock_active())
        return;

    const gem_type gem = gem_for_branch(you.where_are_you);
    if (gem == NUM_GEM_TYPES)
        return;

    if (!Options.more_gem_info && you.gems_found[gem])
        return;

    const int time_taken = you.gem_time_spent[gem];
    const int limit = gem_time_limit(gem);
    if (time_taken >= limit)
        return; // already lost...

    const int d_aut_left = (limit - time_taken + 9) / 10;
    inf.light_text = make_stringf("Gem (%d)", d_aut_left);
    inf.light_colour = _gem_light_colour(d_aut_left);
}

static void _describe_zot(status_info& inf)
{
    const int lvl = bezotting_level();
    if (lvl > 0)
    {
        inf.short_text = "bezotted";
        inf.long_text = "Zot is approaching!";
    }
    else if (!Options.always_show_zot && !you.has_mutation(MUT_SHORT_LIFESPAN)
             || !zot_clock_active())
    {
        return;
    }

    // XX code dup with overview screen
    inf.light_text = make_stringf("Zot (%d)", turns_until_zot());
    switch (lvl)
    {
        case 0:
            inf.light_colour = WHITE;
            break;
        case 1:
            inf.light_colour = YELLOW;
            break;
        case 2:
            inf.light_colour = RED;
            break;
        case 3:
        default:
            inf.light_colour = LIGHTMAGENTA;
            break;
    }
}

static void _describe_glow(status_info& inf)
{
    const int signed_cont = get_contamination_level();
    if (signed_cont <= 0)
        return;

    const unsigned int cont = signed_cont; // so we don't get compiler warnings
    if (player_severe_contamination())
    {
        inf.light_colour = _bad_ench_colour(cont, SEVERE_CONTAM_LEVEL + 1,
                                                  SEVERE_CONTAM_LEVEL + 2);
    }
    else if (cont > 1)
        inf.light_colour = LIGHTGREY;
    else
        inf.light_colour = DARKGREY;
    inf.light_text = "Contam";

    /// Mappings from contamination levels to descriptions.
    static const string contam_adjectives[] =
    {
        "",
        "very slightly ",
        "slightly ",
        "",
        "heavily ",
        "very heavily ",
        "very very heavily ", // this is silly but no one will ever see it
        "impossibly ",        // (likewise)
    };
    ASSERT(signed_cont >= 0);

    const int adj_i = min((size_t) cont, ARRAYSZ(contam_adjectives) - 1);
    inf.short_text = contam_adjectives[adj_i] + "contaminated";
    inf.long_text = describe_contamination(cont);
}

static void _describe_rev(status_info& inf)
{
    if (!you.has_mutation(MUT_WARMUP_STRIKES) || !you.rev_percent())
        return;

    const int tier = you.rev_tier();
    switch (tier)
    {
        case 1:
            inf.light_colour = BLUE;
            inf.light_text   = "Rev";
            inf.short_text   = "revving";
            inf.long_text    = "You're starting to limber up.";
            return;

        case 2:
            inf.light_colour = LIGHTBLUE;
            inf.light_text   = "Rev+";
            inf.short_text   = "revving";
            inf.long_text    = "You're limbering up.";
            return;

        case 3:
            inf.light_colour = WHITE;
            inf.light_text   = "Rev*";
            inf.short_text   = "revved";
            inf.long_text    = "You're fully limbered up.";
            return;
    }
}

static void _describe_regen(status_info& inf)
{
    if (you.duration[DUR_TROGS_HAND])
    {
        inf.light_colour = _dur_colour(BLUE, dur_expiring(DUR_TROGS_HAND));
        inf.light_text = "Regen Will++";
        inf.short_text = "regenerating";
        inf.long_text  = "You are regenerating.";
        _mark_expiring(inf, dur_expiring(DUR_TROGS_HAND));
    }
    else if (you.has_mutation(MUT_VAMPIRISM)
             && you.vampire_alive
             && !you.duration[DUR_SICKNESS])
    {
        inf.short_text = "healing quickly";
    }
    else if (regeneration_is_inhibited())
    {
        inf.light_colour = RED;
        inf.light_text = "-Regen";
        inf.short_text = "inhibited regen";
        inf.long_text = "Your regeneration is inhibited by nearby monsters.";
    }
}

static void _describe_poison(status_info& inf)
{
    int pois_perc = (you.hp <= 0) ? 100
                                  : ((you.hp - max(0, poison_survival())) * 100 / you.hp);
    inf.light_colour = (player_res_poison(false) >= 3
                        ? DARKGREY : _bad_ench_colour(pois_perc, 35, 100));
    inf.light_text   = "Pois";
    const string adj =
         (pois_perc >= 100) ? "lethally" :
         (pois_perc > 65)   ? "seriously" :
         (pois_perc > 35)   ? "quite"
                            : "mildly";
    inf.short_text   = adj + " poisoned"
        + make_stringf(" (%d -> %d)", you.hp, poison_survival());
    inf.long_text    = "You are " + inf.short_text + ".";
}

static void _describe_speed(status_info& inf)
{
    bool slow = you.duration[DUR_SLOW] || have_stat_zero();
    bool fast = you.duration[DUR_HASTE];

    if (slow && fast)
    {
        inf.light_colour = MAGENTA;
        inf.light_text   = "Fast+Slow";
        inf.short_text   = "hasted and slowed";
        inf.long_text = "You are under both slowing and hasting effects.";
    }
    else if (slow)
    {
        inf.light_colour = RED;
        inf.light_text   = "Slow";
        inf.short_text   = "slowed";
        inf.long_text    = "You are slowed.";
    }
    else if (fast)
    {
        inf.light_colour = _dur_colour(BLUE, dur_expiring(DUR_HASTE));
        inf.light_text   = "Fast";
        inf.short_text = "hasted";
        inf.long_text = "Your actions are hasted.";
        _mark_expiring(inf, dur_expiring(DUR_HASTE));
    }
}

static void _describe_airborne(status_info& inf)
{
    if (!you.airborne())
        return;

    const bool perm      = you.permanent_flight();
    const bool expiring  = (!perm && dur_expiring(DUR_FLIGHT));
    const bool emergency = you.props[EMERGENCY_FLIGHT_KEY].get_bool();

    inf.light_colour = perm ? WHITE : emergency ? LIGHTRED : BLUE;
    inf.light_text   = "Fly";
    inf.short_text   = "flying";
    inf.long_text    = "You are flying.";
    inf.light_colour = _dur_colour(inf.light_colour, expiring);
    _mark_expiring(inf, expiring);
}

/**
 * Populate a status info struct with a description of the player's current
 * form.
 *
 * @param[out] inf  The status info struct to be populated.
 */
static void _describe_transform(status_info& inf)
{
    if (you.form == transformation::none)
        return;

    const Form * const form = get_form();
    inf.light_text = form->short_name;
    inf.short_text = form->get_long_name();
    inf.long_text = form->get_description();

    const bool vampbat = (you.get_mutation_level(MUT_VAMPIRISM) >= 2
                          && you.form == transformation::bat);
    const bool expire  = dur_expiring(DUR_TRANSFORMATION) && !vampbat;

    inf.light_colour = _dur_colour(GREEN, expire);
    _mark_expiring(inf, expire);
}

static const char* s0_names[NUM_STATS] = { "Collapse", "Brainless", "Clumsy", };

static void _describe_stat_zero(status_info& inf, stat_type st)
{
    if (you.duration[stat_zero_duration(st)])
    {
        inf.light_colour = you.stat(st) ? LIGHTRED : RED;
        inf.light_text   = s0_names[st];
        inf.short_text   = make_stringf("lost %s", stat_desc(st, SD_NAME));
        inf.long_text    = make_stringf(you.stat(st) ?
                "You are recovering from loss of %s." : "You have no %s!",
                stat_desc(st, SD_NAME));
    }
}

static void _describe_terrain(status_info& inf)
{
    switch (env.grid(you.pos()))
    {
    case DNGN_SHALLOW_WATER:
        inf.light_colour = LIGHTBLUE;
        inf.light_text = "Water";
        break;
    case DNGN_DEEP_WATER:
        inf.light_colour = BLUE;
        inf.light_text = "Water";
        break;
    case DNGN_LAVA:
        inf.light_colour = RED;
        inf.light_text = "Lava";
        break;
    default:
        ;
    }
}

static void _describe_invisible(status_info& inf)
{
    if (!you.duration[DUR_INVIS])
        return;

    inf.light_colour = _dur_colour(BLUE, dur_expiring(DUR_INVIS));
    inf.light_text   = "Invis";
    inf.short_text   = "invisible";
    if (you.backlit())
    {
        inf.light_colour = DARKGREY;
        inf.short_text += " (but backlit and visible)";
    }
    inf.long_text = "You are " + inf.short_text + ".";
    _mark_expiring(inf, dur_expiring(DUR_INVIS));
}

/**
 * Does a given duration tick down simply over time?
 *
 * @param dur   The duration in question (e.g. DUR_PETRIFIED).
 * @return      Whether the duration's end_msg is non-null.
 */
bool duration_decrements_normally(duration_type dur)
{
    return _lookup_duration(dur)->decr.end.msg != nullptr;
}

/**
 * What message should a given duration print when it expires, if any?
 *
 * @param dur   The duration in question (e.g. DUR_PETRIFIED).
 * @return      A message to print for the duration when it ends.
 */
const char *duration_end_message(duration_type dur)
{
    return _lookup_duration(dur)->decr.end.msg;
}

/**
 * What message should a given duration print when it passes its
 * expiring threshold, if any?
 *
 * @param dur   The duration in question (e.g. DUR_PETRIFIED).
 * @return      A message to print.
 */
const char *duration_expire_message(duration_type dur)
{
    return _lookup_duration(dur)->decr.expire_msg.msg;
}

/**
 * How much should the duration be decreased by when it passes its
 * expiring threshold (to fuzz the remaining time), if at all?
 *
 * @param dur   The duration in question (e.g. DUR_PETRIFIED).
 * @return      A random value to reduce the remaining duration by; may be 0.
 */
int duration_expire_offset(duration_type dur)
{
    return _lookup_duration(dur)->decr.expire_msg.offset();
}

/**
 * At what number of turns remaining is the given duration considered to be
 * 'expiring', for purposes of messaging & status light colouring?
 *
 * @param dur   The duration in question (e.g. DUR_PETRIFIED).
 * @return      The maximum number of remaining turns at which the duration
 *              is considered 'expiring'; may be 0.
 */
int duration_expire_point(duration_type dur)
{
    return _lookup_duration(dur)->expire_threshold * BASELINE_DELAY;
}

/**
 * What channel should the duration messages be printed in?
 *
 * @param dur   The duration in question (e.g. DUR_PETRIFIED).
 * @return      The appropriate message channel, e.g. MSGCH_RECOVERY.
 */
msg_channel_type duration_expire_chan(duration_type dur)
{
    return _lookup_duration(dur)->decr.recovery ? MSGCH_RECOVERY
                                                : MSGCH_DURATION;
}

/**
 * If a duration has some special effect when ending, trigger it.
 *
 * @param dur   The duration in question (e.g. DUR_PETRIFIED).
 */
void duration_end_effect(duration_type dur)
{
    if (_lookup_duration(dur)->decr.end.on_end)
        _lookup_duration(dur)->decr.end.on_end();
}
