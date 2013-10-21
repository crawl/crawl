#include "AppHdr.h"

#include "status.h"

#include "areas.h"
#include "env.h"
#include "evoke.h"
#include "godabil.h"
#include "libutil.h"
#include "misc.h"
#include "mutation.h"
#include "player.h"
#include "player-stats.h"
#include "skills2.h"
#include "terrain.h"
#include "transform.h"
#include "spl-transloc.h"
#include "stuff.h"

// Status defaults for durations that are handled straight-forwardly.
struct duration_def
{
    duration_type dur;
    bool expire;         // whether to do automat expiring transforms
    int    light_colour; // status light base colour
    string light_text;   // for the status lights
    string short_text;   // for @: line
    string long_text ;   // for @ message
};

static duration_def duration_data[] =
{
    { DUR_AGILITY, false,
      0, "", "agile", "You are agile." },
    { DUR_ANTIMAGIC, true,
      RED, "-Mag", "antimagic", "You have trouble accessing your magic." },
    { DUR_BARGAIN, true,
      BLUE, "Brgn", "charismatic", "You get a bargain in shops." },
    { DUR_BERSERK, true,
      BLUE, "Berserk", "berserking", "You are possessed by a berserker rage." },
    { DUR_BREATH_WEAPON, false,
      YELLOW, "Breath", "short of breath", "You are short of breath." },
    { DUR_BRILLIANCE, false,
      0, "", "brilliant", "You are brilliant." },
    { DUR_CONF, false,
      RED, "Conf", "confused", "You are confused." },
    { DUR_CONFUSING_TOUCH, true,
      BLUE, "Touch", "confusing touch", "" },
    { DUR_CONTROL_TELEPORT, true,
      MAGENTA, "cTele", "controlling teleports", "You can control teleportations." },
    { DUR_CORONA, false,
      YELLOW, "Corona", "", "" },
    { DUR_DEATH_CHANNEL, true,
      MAGENTA, "DChan", "death channel", "You are channeling the dead." },
    { DUR_DIVINE_STAMINA, true,
      WHITE, "Vit", "vitalised", "You are divinely vitalised." },
    { DUR_DIVINE_VIGOUR, false,
      0, "", "divinely vigorous", "You are imbued with divine vigour." },
    { DUR_EXHAUSTED, false,
      YELLOW, "Exh", "exhausted", "You are exhausted." },
    { DUR_FIRE_SHIELD, true,
      BLUE, "RoF", "immune to fire clouds", "" },
    { DUR_ICY_ARMOUR, true,
      0, "", "icy armour", "You are protected by a layer of icy armour." },
    { DUR_LIQUID_FLAMES, false,
      RED, "Fire", "liquid flames", "You are covered in liquid flames." },
    { DUR_LOWERED_MR, false,
      RED, "-MR", "vulnerable", "" },
    { DUR_MAGIC_SHIELD, false,
      0, "", "shielded", "" },
    { DUR_MIGHT, false,
      0, "", "mighty", "You are mighty." },
    { DUR_MISLED, true,
      LIGHTMAGENTA, "Misled", "misled", "" },
    { DUR_PARALYSIS, false,
      RED, "Para", "paralysed", "You are paralysed." },
    { DUR_PETRIFIED, false,
      RED, "Stone", "petrified", "You are petrified." },
    { DUR_PETRIFYING, true,
      MAGENTA, "Petr", "petrifying", "You are turning to stone." },
    { DUR_JELLY_PRAYER, false,
      WHITE, "Pray", "praying", "You are praying." },
    { DUR_RESISTANCE, true,
      LIGHTBLUE, "Resist", "resistant", "You resist elements." },
    { DUR_SLAYING, false,
      0, "", "deadly", "" },
    { DUR_SLIMIFY, true,
      GREEN, "Slime", "slimy", "" },
    { DUR_SLEEP, false,
      0, "", "sleeping", "You are sleeping." },
    { DUR_STONESKIN, false,
      0, "", "stone skin", "Your skin is tough as stone." },
    { DUR_SWIFTNESS, true,
      BLUE, "Swift", "swift", "You can move swiftly." },
    { DUR_TELEPATHY, false,
      LIGHTBLUE, "Emp", "empathic", "" },
    { DUR_TELEPORT, false,
      LIGHTBLUE, "Tele", "about to teleport", "You are about to teleport." },
    { DUR_DEATHS_DOOR, true,
      LIGHTGREY, "DDoor", "death's door", "" },
    { DUR_PHASE_SHIFT, true,
      0, "", "phasing", "You are out of phase with the material plane." },
    { DUR_QUAD_DAMAGE, true,
      BLUE, "Quad", "quad damage", "" },
    { DUR_SILENCE, true,
      BLUE, "Sil", "silence", "You radiate silence." },
    { DUR_STEALTH, false,
      BLUE, "Stlth", "especially stealthy", "" },
    { DUR_AFRAID, true,
      RED, "Fear", "afraid", "You are terrified." },
    { DUR_MIRROR_DAMAGE, false,
      WHITE, "Mirror", "injury mirror", "You mirror injuries." },
    { DUR_SCRYING, false,
      LIGHTBLUE, "Scry", "scrying",
      "Your astral vision lets you see through walls." },
    { DUR_TORNADO, true,
      LIGHTGREY, "Tornado", "tornado",
      "You are in the eye of a mighty hurricane." },
    { DUR_LIQUEFYING, false,
      LIGHTBLUE, "Liquid", "liquefying",
      "The ground has become liquefied beneath your feet." },
    { DUR_HEROISM, false,
      LIGHTBLUE, "Hero", "heroism", "You possess the skills of a mighty hero." },
    { DUR_FINESSE, false,
      LIGHTBLUE, "Finesse", "finesse", "Your blows are lightning fast." },
    { DUR_LIFESAVING, true,
      LIGHTGREY, "Prot", "protection", "You ask for being saved." },
    { DUR_DARKNESS, true,
      BLUE, "Dark", "darkness", "You emit darkness." },
    { DUR_SHROUD_OF_GOLUBRIA, true,
      BLUE, "Shroud", "shrouded", "You are protected by a distorting shroud." },
    { DUR_TORNADO_COOLDOWN, false,
      YELLOW, "Tornado", "", "" ,},
    { DUR_DISJUNCTION, true,
      BLUE, "Disjoin", "disjoining", "You are disjoining your surroundings." },
    { DUR_SENTINEL_MARK, true,
      MAGENTA, "Mark", "marked", "You are marked for hunting." },
    { DUR_INFUSION, true,
      BLUE, "Infus", "infused", "Your attacks are magically infused."},
    { DUR_SONG_OF_SLAYING, true,
      BLUE, "Slay", "singing", "Your melee attacks are strengthened by your song."},
    { DUR_SONG_OF_SHIELDING, true,
      BLUE, "SShield", "shielded", "Your magic is protecting you."},
    { DUR_FLAYED, true,
      RED, "Flay", "flayed", "You are covered in terrible wounds." },
    { DUR_RETCHING, true,
      RED, "Retch", "retching", "You are retching with violent nausea." },
    { DUR_WEAK, false,
      RED, "Weak", "weakened", "Your attacks are enfeebled." },
    { DUR_DIMENSION_ANCHOR, false,
      RED, "-Tele", "cannot translocate", "You are firmly anchored to this plane." },
    { DUR_SPIRIT_HOWL, false,
      MAGENTA, "Howl", "spirit howling", "The howling of a spirit pack pursues you." },
    { DUR_TOXIC_RADIANCE, false,
      MAGENTA, "Toxic", "radiating poison", "You are radiating toxic energy."},
    { DUR_RECITE, false,
      WHITE, "Recite", "reciting", "You are reciting Zin's Axioms of Law." },
    { DUR_GRASPING_ROOTS, false,
      BROWN, "Roots", "grasped by roots", "Your movement is impeded by grasping roots." },
    { DUR_FIRE_VULN, false,
      RED, "-rF", "fire vulnerable", "You are more vulnerable to fire." },
};

static int duration_index[NUM_DURATIONS];

void init_duration_index()
{
    for (int i = 0; i < NUM_DURATIONS; ++i)
        duration_index[i] = -1;

    for (unsigned i = 0; i < ARRAYSZ(duration_data); ++i)
    {
        duration_type dur = duration_data[i].dur;
        ASSERT_RANGE(dur, 0, NUM_DURATIONS);
        ASSERT(duration_index[dur] == -1);
        duration_index[dur] = i;
    }
}

static const duration_def* _lookup_duration(duration_type dur)
{
    ASSERT_RANGE(dur, 0, NUM_DURATIONS);
    if (duration_index[dur] == -1)
        return NULL;
    else
        return &duration_data[duration_index[dur]];
}

static void _reset_status_info(status_info* inf)
{
    inf->light_colour = 0;
    inf->light_text = "";
    inf->short_text = "";
    inf->long_text = "";
};

static int _bad_ench_colour(int lvl, int orange, int red)
{
    if (lvl > red)
        return RED;
    else if (lvl > orange)
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

static void _mark_expiring(status_info* inf, bool expiring)
{
    if (expiring)
    {
        if (!inf->short_text.empty())
            inf->short_text += " (expiring)";
        if (!inf->long_text.empty())
            inf->long_text = "Expiring: " + inf->long_text;
    }
}

static void _describe_airborne(status_info* inf);
static void _describe_burden(status_info* inf);
static void _describe_glow(status_info* inf);
static void _describe_hunger(status_info* inf);
static void _describe_regen(status_info* inf);
static void _describe_rotting(status_info* inf);
static void _describe_sickness(status_info* inf);
static void _describe_speed(status_info* inf);
static void _describe_sage(status_info* inf);
static void _describe_poison(status_info* inf);
static void _describe_transform(status_info* inf);
static void _describe_stat_zero(status_info* inf, stat_type st);
static void _describe_terrain(status_info* inf);
static void _describe_missiles(status_info* inf);

bool fill_status_info(int status, status_info* inf)
{
    _reset_status_info(inf);

    bool found = false;

    // Sort out inactive durations, and fill in data from duration_data
    // for the simple durations.
    if (status >= 0 && status < NUM_DURATIONS)
    {
        duration_type dur = static_cast<duration_type>(status);

        if (!you.duration[dur])
            return false;

        const duration_def* ddef = _lookup_duration(dur);
        if (ddef)
        {
            found = true;
            inf->light_colour = ddef->light_colour;
            inf->light_text   = ddef->light_text;
            inf->short_text   = ddef->short_text;
            inf->long_text    = ddef->long_text;
            if (ddef->expire)
            {
                inf->light_colour = _dur_colour(inf->light_colour,
                                                 dur_expiring(dur));
                _mark_expiring(inf, dur_expiring(dur));
            }
        }
    }

    // Now treat special status types and durations, possibly
    // completing or overriding the defaults set above.
    switch (status)
    {
    case DUR_CONTROL_TELEPORT:
        if (!allow_control_teleport(true))
            inf->light_colour = DARKGREY;
        break;

    case DUR_SWIFTNESS:
        if (you.in_liquid())
            inf->light_colour = DARKGREY;
        break;

    case STATUS_AIRBORNE:
        _describe_airborne(inf);
        break;

    case STATUS_BEHELD:
        if (you.beheld())
        {
            inf->light_colour = RED;
            inf->light_text   = "Mesm";
            inf->short_text   = "mesmerised";
            inf->long_text    = "You are mesmerised.";
        }
        break;

    case STATUS_BURDEN:
        _describe_burden(inf);
        break;

    case STATUS_CONTAMINATION:
        _describe_glow(inf);
        break;

    case STATUS_BACKLIT:
        if (you.backlit())
        {
            inf->short_text = "glowing";
            inf->long_text  = "You are glowing.";
        }
        break;

    case STATUS_UMBRA:
        if (you.umbra())
        {
            inf->light_colour = MAGENTA;
            inf->light_text   = "Umbra";
            inf->short_text   = "wreathed by umbra";
            inf->long_text    = "You are wreathed by an unholy umbra.";
        }
        break;

    case STATUS_SUPPRESSED:
        if (you.suppressed())
        {
            inf->light_colour = LIGHTGREEN;
            inf->light_text   = "Suppress";
            inf->short_text   = "magically suppressed";
            inf->long_text    = "You are enveloped in a field of magical suppression.";
        }
        break;

    case STATUS_NET:
        if (you.attribute[ATTR_HELD])
        {
            inf->light_colour = RED;
            inf->light_text   = "Held";
            inf->short_text   = "held";
            inf->long_text    = make_stringf("You are %s.", held_status());
        }
        break;

    case STATUS_HUNGER:
        _describe_hunger(inf);
        break;

    case STATUS_REGENERATION:
        // DUR_REGENERATION + some vampire and non-healing stuff
        _describe_regen(inf);
        break;

    case STATUS_ROT:
        _describe_rotting(inf);
        break;

    case STATUS_SICK:
        _describe_sickness(inf);
        break;

    case STATUS_SPEED:
        _describe_speed(inf);
        break;

    case STATUS_LIQUEFIED:
    {
        if (you.liquefied_ground())
        {
            inf->light_colour = BROWN;
            inf->light_text   = "SlowM";
            inf->short_text   = "slowed movement";
            inf->long_text    = "Your movement is slowed on this liquid ground.";
        }
        break;
    }

    case STATUS_SAGE:
        _describe_sage(inf);
        break;

    case STATUS_AUGMENTED:
    {
        int level = augmentation_amount();

        if (level > 0)
        {
            inf->light_colour = (level == 3) ? WHITE :
                                (level == 2) ? LIGHTBLUE
                                             : BLUE;

            inf->light_text = "Aug";
        }
        break;
    }

    case DUR_CONFUSING_TOUCH:
    {
        const int dur = you.duration[DUR_CONFUSING_TOUCH];
        const int high = 40 * BASELINE_DELAY;
        const int low  = 20 * BASELINE_DELAY;
        inf->long_text = string("Your ") + you.hand_name(true) + " are glowing ";
        if (dur > high)
            inf->long_text += "an extremely bright ";
        else if (dur > low)
            inf->long_text += "bright ";
        else
           inf->long_text += "a soft ";
        inf->long_text += "red.";
        break;
    }

    case DUR_FIRE_SHIELD:
    {
        // Might be better to handle this with an extra virtual status.
        const bool exp = dur_expiring(DUR_FIRE_SHIELD);
        if (exp)
            inf->long_text += "Expiring: ";
        inf->long_text += "You are surrounded by a ring of flames.\n";
        if (exp)
            inf->long_text += "Expiring: ";
        inf->long_text += "You are immune to clouds of flame.";
        break;
    }

    case DUR_INVIS:
        if (you.attribute[ATTR_INVIS_UNCANCELLABLE])
            inf->light_colour = _dur_colour(BLUE, dur_expiring(DUR_INVIS));
        else
            inf->light_colour = _dur_colour(MAGENTA, dur_expiring(DUR_INVIS));
        inf->light_text   = "Invis";
        inf->short_text   = "invisible";
        if (you.backlit())
        {
            inf->light_colour = DARKGREY;
            inf->short_text += " (but backlit and visible)";
        }
        inf->long_text = "You are " + inf->short_text + ".";
        _mark_expiring(inf, dur_expiring(DUR_INVIS));
        break;

    case DUR_POISONING:
        _describe_poison(inf);
        break;

    case DUR_POWERED_BY_DEATH:
        if (handle_pbd_corpses(false) > 0)
        {
            inf->light_colour = LIGHTMAGENTA;
            inf->light_text   = "Regen+";
        }
        break;

    case STATUS_MISSILES:
        _describe_missiles(inf);
        break;

    case STATUS_MANUAL:
    {
        string skills = manual_skill_names();
        if (!skills.empty())
        {
            inf->short_text = "studying " + manual_skill_names(true);
            inf->long_text = "You are studying " + skills + ".";
        }
        break;
    }

    case DUR_SURE_BLADE:
    {
        inf->light_colour = BLUE;
        inf->light_text   = "Blade";
        inf->short_text   = "bonded with blade";
        string desc;
        if (you.duration[DUR_SURE_BLADE] > 15 * BASELINE_DELAY)
            desc = "strong ";
        else if (you.duration[DUR_SURE_BLADE] >  5 * BASELINE_DELAY)
            desc = "";
        else
            desc = "weak";
        inf->long_text = "You have a " + desc + "bond with your blade.";
        break;
    }

    case DUR_TRANSFORMATION:
        _describe_transform(inf);
        break;

    case STATUS_CLINGING:
        if (you.is_wall_clinging())
        {
            inf->light_text   = "Cling";
            inf->short_text   = "clinging";
            inf->long_text    = "You cling to the nearby walls.";
            const dungeon_feature_type feat = grd(you.pos());
            if (is_feat_dangerous(feat))
                inf->light_colour = LIGHTGREEN;
            else if (feat == DNGN_LAVA || feat_is_water(feat))
                inf->light_colour = GREEN;
            else
                inf->light_colour = DARKGREY;
            if (you.form == TRAN_SPIDER)
                _mark_expiring(inf, dur_expiring(DUR_TRANSFORMATION));
        }
        break;

    case STATUS_HOVER:
        if (is_hovering())
        {
            inf->light_colour = RED;
            inf->light_text   = "Hover";
            inf->short_text   = "hovering above liquid";
            inf->long_text    = "You are exerting yourself to hover high above the liquid.";
        }
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

    case STATUS_FIREBALL:
        if (you.attribute[ATTR_DELAYED_FIREBALL])
        {
            inf->light_colour = LIGHTMAGENTA;
            inf->light_text   = "Fball";
            inf->short_text   = "delayed fireball";
            inf->long_text    = "You have a stored fireball ready to release.";
        }
        break;

    case STATUS_CONSTRICTED:
        if (you.is_constricted())
        {
            inf->light_colour = YELLOW;
            inf->light_text   = you.held == HELD_MONSTER ? "Held" : "Constr";
            inf->short_text   = you.held == HELD_MONSTER ? "held" : "constricted";
        }
        break;

    case STATUS_TERRAIN:
        _describe_terrain(inf);
        break;

    // Silenced by an external source.
    case STATUS_SILENCE:
        if (silenced(you.pos()) && !you.duration[DUR_SILENCE])
        {
            inf->light_colour = LIGHTMAGENTA;
            inf->light_text   = "Sil";
            inf->short_text   = "silenced";
            inf->long_text    = "You are silenced.";
        }
        break;

    case DUR_SONG_OF_SLAYING:
        inf->light_text = make_stringf("Slay (%u)",
                                       you.props["song_of_slaying_bonus"].get_int());
        break;

    case STATUS_NO_CTELE:
        if (!allow_control_teleport(true))
        {
            inf->light_colour = RED;
            inf->light_text = "-cTele";
        }
        break;

    case STATUS_BEOGH:
        if (env.level_state & LSTATE_BEOGH && can_convert_to_beogh())
        {
            inf->light_colour = WHITE;
            inf->light_text = "Beogh";
        }
        break;

    case STATUS_RECALL:
        if (you.attribute[ATTR_NEXT_RECALL_INDEX] > 0)
        {
            inf->light_colour = WHITE;
            inf->light_text   = "Recall";
            inf->short_text   = "recalling";
            inf->long_text    = "You are recalling your allies.";
        }
        break;

    case DUR_WATER_HOLD:
        inf->light_text   = "Engulf";
        if (you.res_water_drowning())
        {
            inf->short_text   = "engulfed";
            inf->long_text    = "You are engulfed in water.";
            if (you.can_swim())
                inf->light_colour = DARKGREY;
            else
                inf->light_colour = YELLOW;
        }
        else
        {
            inf->short_text   = "engulfed (cannot breathe)";
            inf->long_text    = "You are engulfed in water and unable to breathe.";
            inf->light_colour = RED;
        }
        break;

    case STATUS_DRAINED:
        if (you.attribute[ATTR_XP_DRAIN] > 250)
        {
            inf->light_colour = RED;
            inf->light_text   = "Drain";
            inf->short_text   = "very heavily drained";
            inf->long_text    = "Your life force is very heavily drained.";
        }
        else if (you.attribute[ATTR_XP_DRAIN] > 100)
        {
            inf->light_colour = LIGHTRED;
            inf->light_text   = "Drain";
            inf->short_text   = "heavily drained";
            inf->long_text    = "Your life force is heavily drained.";
        }
        else if (you.attribute[ATTR_XP_DRAIN])
        {
            inf->light_colour = YELLOW;
            inf->light_text   = "Drain";
            inf->short_text   = "drained";
            inf->long_text    = "Your life force is drained.";
        }
        break;

    case STATUS_RAY:
        if (you.attribute[ATTR_SEARING_RAY])
        {
            inf->light_colour = WHITE;
            inf->light_text   = "Ray";
        }
        break;

    case STATUS_ELIXIR:
        if (you.duration[DUR_ELIXIR_HEALTH] || you.duration[DUR_ELIXIR_MAGIC])
        {
            if (you.duration[DUR_ELIXIR_HEALTH] && you.duration[DUR_ELIXIR_MAGIC])
                inf->light_colour = WHITE;
            else if (you.duration[DUR_ELIXIR_HEALTH])
                inf->light_colour = LIGHTBLUE;
            else
                inf->light_colour = LIGHTMAGENTA;
            inf->light_text   = "Elixir";
        }
        break;

    default:
        if (!found)
        {
            inf->light_colour = RED;
            inf->light_text   = "Missing";
            inf->short_text   = "missing status";
            inf->long_text    = "Missing status description.";
            return false;
        }
        else
            break;
    }
    return true;
}

static void _describe_hunger(status_info* inf)
{
    const bool vamp = (you.species == SP_VAMPIRE);

    switch (you.hunger_state)
    {
    case HS_ENGORGED:
        inf->light_colour = LIGHTGREEN;
        inf->light_text   = (vamp ? "Alive" : "Engorged");
        break;
    case HS_VERY_FULL:
        inf->light_colour = GREEN;
        inf->light_text   = "Very Full";
        break;
    case HS_FULL:
        inf->light_colour = GREEN;
        inf->light_text   = "Full";
        break;
    case HS_HUNGRY:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? "Thirsty" : "Hungry");
        break;
    case HS_VERY_HUNGRY:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? "Very Thirsty" : "Very Hungry");
        break;
    case HS_NEAR_STARVING:
        inf->light_colour = YELLOW;
        inf->light_text   = (vamp ? "Near Bloodless" : "Near Starving");
        break;
    case HS_STARVING:
        inf->light_colour = RED;
        inf->light_text   = (vamp ? "Bloodless" : "Starving");
        inf->short_text   = (vamp ? "bloodless" : "starving");
        break;
    case HS_SATIATED: // no status light
    default:
        break;
    }
}

static void _describe_glow(status_info* inf)
{
    const int cont = get_contamination_level();
    if (cont > 0)
    {
        inf->light_colour = DARKGREY;
        if (cont > 1)
            inf->light_colour = _bad_ench_colour(cont, 2, 3);
        if (cont > 1 || you.species != SP_DJINNI)
            inf->light_text = "Contam";
    }

    if (cont > 0)
    {
        inf->short_text =
                 (cont == 1) ? "very slightly " :
                 (cont == 2) ? "slightly " :
                 (cont == 3) ? "" :
                 (cont == 4) ? "moderately " :
                 (cont == 5) ? "heavily "
                             : "really heavily ";
        inf->short_text += "contaminated";
        inf->long_text = describe_contamination(cont);
    }
}

static void _describe_regen(status_info* inf)
{
    const bool regen = (you.duration[DUR_REGENERATION] > 0);
    const bool no_heal =
            (you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING)
            || (player_mutation_level(MUT_SLOW_HEALING) == 3);
    // Does vampire hunger level affect regeneration rate significantly?
    const bool vampmod = !no_heal && !regen && you.species == SP_VAMPIRE
                         && you.hunger_state != HS_SATIATED;

    if (regen)
    {
        inf->light_colour = _dur_colour(BLUE, dur_expiring(DUR_REGENERATION));
        inf->light_text   = "Regen";
        if (you.attribute[ATTR_DIVINE_REGENERATION])
            inf->light_text += " MR";
        else if (no_heal)
            inf->light_colour = DARKGREY;
    }

    if ((you.disease && !regen) || no_heal)
       inf->short_text = "non-regenerating";
    else if (regen)
    {
        if (you.disease)
        {
            inf->short_text = "recuperating";
            inf->long_text  = "You are recuperating from your illness.";
        }
        else
        {
            inf->short_text = "regenerating";
            inf->long_text  = "You are regenerating.";
        }
        _mark_expiring(inf, dur_expiring(DUR_REGENERATION));
    }
    else if (vampmod)
    {
        if (you.disease)
            inf->short_text = "recuperating";
        else
            inf->short_text = "regenerating";

        if (you.hunger_state < HS_SATIATED)
            inf->short_text += " slowly";
        else if (you.hunger_state < HS_ENGORGED)
            inf->short_text += " quickly";
        else
            inf->short_text += " very quickly";
    }
}

static void _describe_poison(status_info* inf)
{
    int pois = you.duration[DUR_POISONING];
    inf->light_colour = (player_res_poison(false) >= 3
                         ? DARKGREY : _bad_ench_colour(pois, 5, 10));
    inf->light_text   = "Pois";
    const string adj =
         (pois > 10) ? "extremely" :
         (pois > 5)  ? "very" :
         (pois > 3)  ? "quite"
                     : "mildly";
    inf->short_text   = adj + " poisoned";
    inf->long_text    = "You are " + inf->short_text + ".";
}

static void _describe_speed(status_info* inf)
{
    if (you.duration[DUR_SLOW] && you.duration[DUR_HASTE])
    {
        inf->light_colour = MAGENTA;
        inf->light_text   = "Fast+Slow";
        inf->short_text   = "hasted and slowed";
        inf->long_text = "You are under both slowing and hasting effects.";
    }
    else if (you.duration[DUR_SLOW])
    {
        inf->light_colour = RED;
        inf->light_text   = "Slow";
        inf->short_text   = "slowed";
        inf->long_text    = "You are slowed.";
    }
    else if (you.duration[DUR_HASTE])
    {
        inf->light_colour = _dur_colour(BLUE, dur_expiring(DUR_HASTE));
        inf->light_text   = "Fast";
        inf->short_text = "hasted";
        inf->long_text = "Your actions are hasted.";
        _mark_expiring(inf, dur_expiring(DUR_HASTE));
    }
}

static void _describe_sage(status_info* inf)
{
    if (you.sage_skills.empty())
        return;

    vector<const char*> sages;
    for (unsigned long i = 0; i < you.sage_skills.size(); ++i)
        sages.push_back(skill_name(you.sage_skills[i]));

    inf->light_colour = LIGHTBLUE;
    inf->light_text   = "Sage";
    inf->short_text   = "sage [" + comma_separated_line(sages.begin(),
                        sages.end(), ", ") + "]";
    inf->long_text    = "You feel studious about " + comma_separated_line(
                        sages.begin(), sages.end()) + ".";
}

static void _describe_airborne(status_info* inf)
{
    if (!you.airborne())
        return;

    const bool perm     = you.permanent_flight();
    const bool expiring = (!perm && dur_expiring(DUR_FLIGHT));

    inf->light_colour = you.tengu_flight() ? BLUE : perm ? WHITE : MAGENTA;
    inf->light_text   = "Fly";
    inf->short_text   = "flying";
    inf->long_text    = "You are flying.";
    inf->light_colour = _dur_colour(inf->light_colour, expiring);
    _mark_expiring(inf, expiring);
}

static void _describe_rotting(status_info* inf)
{
    if (you.rotting)
    {
        inf->light_colour = _bad_ench_colour(you.rotting, 4, 8);
        inf->light_text   = "Rot";
    }

    if (you.rotting || you.species == SP_GHOUL)
    {
        inf->short_text = "rotting";
        inf->long_text = "Your flesh is rotting";
        int rot = you.rotting;
        if (you.species == SP_GHOUL)
            rot += 1 + (1 << max(0, HS_SATIATED - you.hunger_state));
        if (rot > 15)
            inf->long_text += " before your eyes";
        else if (rot > 8)
            inf->long_text += " away quickly";
        else if (rot > 4)
            inf->long_text += " badly";
        else if (you.species == SP_GHOUL)
            if (rot > 2)
                inf->long_text += " faster than usual";
            else
                inf->long_text += " at the usual pace";
        inf->long_text += ".";
    }
}

static void _describe_sickness(status_info* inf)
{
    if (you.disease)
    {
        const int high = 120 * BASELINE_DELAY;
        const int low  =  40 * BASELINE_DELAY;

        inf->light_colour   = _bad_ench_colour(you.disease, low, high);
        inf->light_text     = "Sick";

        string mod = (you.disease > high) ? "badly "  :
                     (you.disease >  low) ? ""
                                          : "mildly ";

        inf->short_text = mod + "diseased";
        inf->long_text  = "You are " + mod + "diseased.";
    }
}

static void _describe_burden(status_info* inf)
{
    switch (you.burden_state)
    {
    case BS_OVERLOADED:
        inf->light_colour = RED;
        inf->light_text   = "Overloaded";
        inf->short_text   = "overloaded";
        inf->long_text    = "You are overloaded with stuff.";
        break;
    case BS_ENCUMBERED:
        inf->light_colour = LIGHTRED;
        inf->light_text   = "Burdened";
        inf->short_text   = "burdened";
        inf->long_text    = "You are burdened.";
        break;
    case BS_UNENCUMBERED:
        break;
    }
}

static void _describe_transform(status_info* inf)
{
    const bool vampbat = (you.species == SP_VAMPIRE && you.form == TRAN_BAT);
    const bool expire  = dur_expiring(DUR_TRANSFORMATION) && !vampbat;

    switch (you.form)
    {
    case TRAN_BAT:
        inf->light_text     = "Bat";
        inf->short_text     = "bat-form";
        inf->long_text      = "You are in ";
        if (vampbat)
            inf->long_text += "vampire ";
        inf->long_text     += "bat-form.";
        break;
    case TRAN_BLADE_HANDS:
        inf->light_text = "Blades";
        inf->short_text = "blade " + blade_parts(true);
        inf->long_text  = "You have blades for " + blade_parts() + ".";
        break;
    case TRAN_DRAGON:
        inf->light_text = "Dragon";
        inf->short_text = "dragon-form";
        inf->long_text  = "You are in dragon-form.";
        break;
    case TRAN_ICE_BEAST:
        inf->light_text = "Ice";
        inf->short_text = "ice-form";
        inf->long_text  = "You are an ice creature.";
        break;
    case TRAN_LICH:
        inf->light_text = "Lich";
        inf->short_text = "lich-form";
        inf->long_text  = "You are in lich-form.";
        break;
    case TRAN_PIG:
        inf->light_text = "Pig";
        inf->short_text = "pig-form";
        inf->long_text  = "You are a filthy swine.";
        break;
    case TRAN_SPIDER:
        inf->light_text = "Spider";
        inf->short_text = "spider-form";
        inf->long_text  = "You are in spider-form.";
        break;
    case TRAN_STATUE:
        inf->light_text = "Statue";
        inf->short_text = "statue-form";
        inf->long_text  = "You are a statue.";
        break;
    case TRAN_APPENDAGE:
        inf->light_text = "App";
        inf->short_text = "appendage";
        inf->long_text  = "You have a beastly appendage.";
        break;
    case TRAN_FUNGUS:
        inf->light_text = "Fungus";
        inf->short_text = "fungus-form";
        inf->long_text  = "You are a sentient fungus.";
        break;
    case TRAN_TREE:
        inf->light_text = "Tree";
        inf->short_text = "tree-form";
        inf->long_text  = "You are an animated tree.";
        break;
    case TRAN_JELLY:
        inf->light_text = "Jelly";
        inf->short_text = "jelly-form";
        inf->long_text  = "You are a lump of jelly.";
        break;
    case TRAN_PORCUPINE:
        inf->light_text = "Porc";
        inf->short_text = "porcupine-form";
        inf->long_text  = "You are a porcupine.";
        break;
    case TRAN_WISP:
        inf->light_text = "Wisp";
        inf->short_text = "wisp-form";
        inf->long_text  = "You are an insubstantial wisp.";
        break;
    case TRAN_NONE:
        break;
    }

    inf->light_colour = _dur_colour(GREEN, expire);
    _mark_expiring(inf, expire);
}

static const char* s0_names[NUM_STATS] = { "Collapse", "Brainless", "Clumsy", };

static void _describe_stat_zero(status_info* inf, stat_type st)
{
    if (you.stat_zero[st])
    {
        inf->light_colour = you.stat(st) ? LIGHTRED : RED;
        inf->light_text   = s0_names[st];
        inf->short_text   = make_stringf("lost %s", stat_desc(st, SD_NAME));
        inf->long_text    = make_stringf(you.stat(st) ?
                "You are recovering from loss of %s." : "You have no %s!",
                stat_desc(st, SD_NAME));
    }
}

static void _describe_terrain(status_info* inf)
{
    switch (grd(you.pos()))
    {
    case DNGN_SHALLOW_WATER:
        inf->light_colour = LIGHTBLUE;
        inf->light_text = "Water";
        break;
    case DNGN_DEEP_WATER:
        inf->light_colour = BLUE;
        inf->light_text = "Water";
        break;
    case DNGN_LAVA:
        inf->light_colour = RED;
        inf->light_text = "Lava";
        break;
    default:
        ;
    }
}

static void _describe_missiles(status_info* inf)
{
    const int level = you.missile_deflection();
    if (!level)
        return;

    bool expiring;
    if (level > 1)
    {
        inf->light_colour = MAGENTA;
        inf->light_text   = "DMsl";
        inf->short_text   = "deflect missiles";
        inf->long_text    = "You deflect missiles.";
        expiring = dur_expiring(DUR_DEFLECT_MISSILES);
    }
    else
    {
        bool perm = player_mutation_level(MUT_DISTORTION_FIELD) == 3
                    || !you.suppressed() && you.scan_artefacts(ARTP_RMSL);
        inf->light_colour = perm ? WHITE : BLUE;
        inf->light_text   = "RMsl";
        inf->short_text   = "repel missiles";
        inf->long_text    = "You repel missiles.";
        expiring = (!perm && dur_expiring(DUR_REPEL_MISSILES));
    }

    inf->light_colour = _dur_colour(inf->light_colour, expiring);
    _mark_expiring(inf, expiring);
}
