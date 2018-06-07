/**
 * @file
 * @brief Contains code for the `monster` utility.
**/

#include "AppHdr.h"
#include "externs.h"
#include "directn.h"
#include "unwind.h"
#include "env.h"
#include "colour.h"
#include "coordit.h"
#include "dungeon.h"
#include "los.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-util.h"
#include "version.h"
#include "view.h"
#include "los.h"
#include "maps.h"
#include "initfile.h"
#include "libutil.h"
#include "item-name.h"
#include "item-prop.h"
#include "act-iter.h"
#include "mon-death.h"
#include "random.h"
#include "spl-util.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "syscalls.h"
#include "artefact.h"
#include <sstream>
#include <set>
#include <unistd.h>

const coord_def MONSTER_PLACE(20, 20);

const string CANG = "cang";

const int PLAYER_MAXHP = 500;
const int PLAYER_MAXMP = 50;

// Clockwise, around the compass from north (same order as enum RUN_DIR)
const struct coord_def Compass[9] = {
    coord_def(0, -1), coord_def(1, -1),  coord_def(1, 0),
    coord_def(1, 1),  coord_def(0, 1),   coord_def(-1, 1),
    coord_def(-1, 0), coord_def(-1, -1), coord_def(0, 0),
};

static bool _is_element_colour(int col)
{
    col = col & 0x007f;
    ASSERT(col < NUM_COLOURS);
    return col >= ETC_FIRE;
}

static string colour_codes[] = {"",   "02", "03", "10", "05", "06",
                                     "07", "15", "14", "12", "09", "11",
                                     "04", "13", "08", "00"};

static int bgr[8] = {0, 4, 2, 6, 1, 5, 3, 7};

#ifdef CONTROL
#undef CONTROL
#endif
#define CONTROL(x) char(x - 'A' + 1)

static string colour(int colour, string text, bool bg = false)
{
    if (_is_element_colour(colour))
        colour = element_colour(colour, true);

    if (isatty(1))
    {
        if (!colour)
            return text;
        return make_stringf("\033[0;%d%d%sm%s\033[0m", bg ? 4 : 3,
                            bgr[colour & 7],
                            (!bg && (colour & 8)) ? ";1" : "", text.c_str());
    }

    const string code(colour_codes[colour]);

    if (code.empty())
        return text;

    return string() + CONTROL('C') + code + (bg ? ",01" : "") + text
           + CONTROL('O');
}

static void record_resvul(int color, const char* name, const char* caption,
                          string& str, int rval)
{
    if (str.empty())
        str = " | " + string(caption) + ": ";
    else
        str += ", ";

    if (color && (rval == 3 || rval == 1 && color == BROWN
                  || string(caption) == "Vul")
        && (int)color <= 7)
    {
        color += 8;
    }

    string token(name);
    if (rval > 1 && rval <= 3)
    {
        while (rval-- > 0)
            token += "+";
    }

    str += colour(color, token);
}

static void record_resist(int colour, string name, string& res,
                          string& vul, int rval)
{
    if (rval > 0)
        record_resvul(colour, name.c_str(), "Res", res, rval);
    else if (rval < 0)
        record_resvul(colour, name.c_str(), "Vul", vul, -rval);
}

static void monster_action_cost(string& qual, int cost, const char* desc)
{
    if (cost != 10)
    {
        if (!qual.empty())
            qual += "; ";
        qual += desc;
        qual += ": " + to_string(cost * 10) + "%";
    }
}

static string monster_int(const monster& mon)
{
    string intel = "???";
    switch (mons_intel(mon))
    {
    case I_BRAINLESS:
        intel = "brainless";
        break;
    case I_ANIMAL:
        intel = "animal";
        break;
    case I_HUMAN:
        intel = "human";
        break;
        // Let the compiler issue warnings for missing entries.
    }

    return intel;
}

static string monster_size(const monster& mon)
{
    switch (mon.body_size())
    {
    case SIZE_TINY:
        return "tiny";
    case SIZE_LITTLE:
        return "little";
    case SIZE_SMALL:
        return "small";
    case SIZE_MEDIUM:
        return "Medium";
    case SIZE_LARGE:
        return "Large";
    case SIZE_BIG:
        return "Big";
    case SIZE_GIANT:
        return "Giant";
    default:
        return "???";
    }
}

static string monster_speed(const monster& mon, const monsterentry* me,
                                 int speed_min, int speed_max)
{
    string speed;

    if (speed_max != speed_min)
        speed += to_string(speed_min) + "-" + to_string(speed_max);
    else if (speed_max == 0)
        speed += colour(BROWN, "0");
    else
        speed += to_string(speed_max);

    const mon_energy_usage& cost = mons_energy(mon);
    string qualifiers;

    bool skip_action = false;
    if (cost.attack != 10 && cost.attack == cost.missile
        && cost.attack == cost.spell && cost.attack == cost.special
        && cost.attack == cost.item)
    {
        monster_action_cost(qualifiers, cost.attack, "act");
        skip_action = true;
    }

    monster_action_cost(qualifiers, cost.move, "move");
    if (cost.swim != cost.move)
        monster_action_cost(qualifiers, cost.swim, "swim");
    if (!skip_action)
    {
        monster_action_cost(qualifiers, cost.attack, "atk");
        monster_action_cost(qualifiers, cost.missile, "msl");
        monster_action_cost(qualifiers, cost.spell, "spell");
        monster_action_cost(qualifiers, cost.special, "special");
        monster_action_cost(qualifiers, cost.item, "item");
    }
    if (speed_max > 0 && mons_class_flag(mon.type, M_STATIONARY))
    {
        if (!qualifiers.empty())
            qualifiers += "; ";
        qualifiers += colour(BROWN, "stationary");
    }

    if (!qualifiers.empty())
        speed += " (" + qualifiers + ")";

    return speed;
}

static void mons_flag(string& flag, const string& newflag)
{
    if (flag.empty())
        flag = " | ";
    else
        flag += ", ";
    flag += newflag;
}

static void mons_check_flag(bool set, string& flag,
                            const string& newflag)
{
    if (set)
        mons_flag(flag, newflag);
}

static void initialize_crawl()
{
    init_monsters();
    init_properties();
    init_item_name_cache();

    init_zap_index();
    init_spell_descs();
    init_monster_symbols();
    init_mon_name_cache();
    init_spell_name_cache();
    init_mons_spells();
    init_element_colours();
    init_show_table(); // Initializes indices for get_feature_def.

    dgn_reset_level();
    for (rectangle_iterator ri(0); ri; ++ri)
        grd(*ri) = DNGN_FLOOR;

    los_changed();
    you.hp = you.hp_max = PLAYER_MAXHP;
    you.magic_points = you.max_magic_points = PLAYER_MAXMP;
    you.species = SP_HUMAN;
}

static string dice_def_string(dice_def dice)
{
    return dice.num == 1 ? make_stringf("d%d", dice.size) :
                           make_stringf("%dd%d", dice.num, dice.size);
}

static dice_def mi_calc_iood_damage(monster* mons)
{
    const int power =
        stepdown_value(6 * mons->get_experience_level(), 30, 30, 200, -1);
    return dice_def(9, power / 4);
}

static string mi_calc_smiting_damage(monster* mons) { return "7-17"; }

static string mi_calc_airstrike_damage(monster* mons)
{
    return make_stringf("0-%d", 10 + 2 * mons->get_experience_level());
}

static string mi_calc_glaciate_damage(monster* mons)
{
    int pow = 12 * mons->get_experience_level();
    // Minimum of the number of dice, or the max damage at max range
    int minimum = min(10, (54 + 3 * pow / 2) / 6);
    // Maximum damage at minimum range.
    int max = (54 + 3 * pow / 2) / 3;

    return make_stringf("%d-%d", minimum, max);
}

static string mi_calc_chain_lightning_damage(monster* mons)
{
    int pow = 4 * mons->get_experience_level();

    // Damage is 5d(9.2 + pow / 30), but if lots of targets are around
    // it can hit the player precisely once at very low (e.g. 1) power
    // and deal 5 damage.
    int min = 5;

    // Max damage per bounce is 46 + pow / 6; in the worst case every other
    // bounce hits the player, losing 8 pow on the bounce away and 8 on the
    // bounce back for a total of 16; thus, for n bounces, it's:
    // (46 + pow/6) * n less 16/6 times the (n - 1)th triangular number.
    int n = (pow + 15) / 16;
    int max = (46 + (pow / 6)) * n - 4 * n * (n - 1) / 3;

    return make_stringf("%d-%d", min, max);
}

static string mi_calc_vampiric_drain_damage(monster* mons)
{
    int pow = 12 * mons->get_experience_level();

    // The current formula is 3 + random2avg(9, 2) + 1 + random2(pow) / 7.
    // Min is 3 + 0 + 1 + (0 / 7) = 4.
    // Max is 3 + 8 + 1 + (pow - 1) / 7 = 12 + (pow - 1) / 7.
    int min = 4;
    int max = 12 + (pow - 1) / 7;
    return make_stringf("%d-%d", min, max);
}

static string mi_calc_major_healing(monster* mons)
{
    const int min = 50;
    const int max = min + mons->spell_hd(SPELL_MAJOR_HEALING) * 10;
    return make_stringf("%d-%d", min, max);
}

/**
 * @return e.g.: "2d6", "5-12".
 */
static string mons_human_readable_spell_damage_string(monster* monster,
                                                      spell_type sp)
{
    bolt spell_beam = mons_spell_beam(
        monster, sp, mons_power_for_hd(sp, monster->spell_hd(sp), false), true);
    switch (sp)
    {
        case SPELL_PORTAL_PROJECTILE:
        case SPELL_LRD:
            return ""; // Fake damage beam
        case SPELL_SMITING:
            return mi_calc_smiting_damage(monster);
        case SPELL_AIRSTRIKE:
            return mi_calc_airstrike_damage(monster);
        case SPELL_GLACIATE:
            return mi_calc_glaciate_damage(monster);
        case SPELL_CHAIN_LIGHTNING:
            return mi_calc_chain_lightning_damage(monster);
        case SPELL_WATERSTRIKE:
            spell_beam.damage = waterstrike_damage(*monster);
            break;
        case SPELL_RESONANCE_STRIKE:
            return dice_def_string(resonance_strike_base_damage(*monster))
                   + "+"; // could clarify further?
        case SPELL_IOOD:
            spell_beam.damage = mi_calc_iood_damage(monster);
            break;
        case SPELL_VAMPIRIC_DRAINING:
            return mi_calc_vampiric_drain_damage(monster);
        case SPELL_MAJOR_HEALING:
            return mi_calc_major_healing(monster);
        case SPELL_MINOR_HEALING:
        case SPELL_HEAL_OTHER:
            return dice_def_string(spell_beam.damage) + "+3";
        default:
            break;
    }

    if (spell_beam.origin_spell == SPELL_IOOD)
        spell_beam.damage = mi_calc_iood_damage(monster);

    if (spell_beam.damage.size && spell_beam.damage.num)
        return dice_def_string(spell_beam.damage);
    return "";
}

static string shorten_spell_name(string name)
{
    lowercase(name);
    string::size_type pos = name.find('\'');
    if (pos != string::npos)
    {
        pos = name.find(' ', pos);
        if (pos != string::npos)
        {
            // strip wizard names
            if (starts_with(name, "iskenderun's")
                || starts_with(name, "olgreb's") || starts_with(name, "lee's")
                || starts_with(name, "leda's") || starts_with(name, "lehudib's")
                || starts_with(name, "borgnjor's")
                || starts_with(name, "ozocubu's")
                || starts_with(name, "tukima's")
                || starts_with(name, "alistair's"))
            {
                name = name.substr(pos + 1);
            }
        }
    }
    if ((pos = name.find(" of ")) != string::npos)
    {
        istringstream words { name.substr(0, pos) };
        string abbrev, word;
        while (words >> word)
        {
            abbrev += word[0];
            abbrev += '.';
        }
        name = abbrev + name.substr(pos + 4);
    }
    if (starts_with(name, "summon "))
        name = "sum." + name.substr(7);
    if (ends_with(name, " bolt"))
        name = "b." + name.substr(0, name.length() - 5);
    return name;
}

static string _spell_flag_string(const mon_spell_slot& slot)
{
    string flags;

    if (!(slot.flags & MON_SPELL_ANTIMAGIC_MASK))
        flags += colour(LIGHTCYAN, "!AM");
    if (!(slot.flags & MON_SPELL_SILENCE_MASK))
    {
        if (!flags.empty())
            flags += ", ";
        flags += colour(MAGENTA, "!sil");
    }
    if (slot.flags & MON_SPELL_BREATH)
    {
        if (!flags.empty())
            flags += ", ";
        flags += colour(YELLOW, "breath");
    }
    if (slot.flags & MON_SPELL_EMERGENCY)
    {
        if (!flags.empty())
            flags += ", ";
        flags += colour(LIGHTRED, "emergency");
    }

    if (!flags.empty())
        flags = " [" + flags + "]";
    return flags;
}

// ::first is spell name, ::second is possible damages
typedef multimap<string, string> spell_damage_map;
static void record_spell_set(monster* mp, set<string>& spell_lists,
                             spell_damage_map& damages)
{
    string ret;
    for (const auto& slot : mp->spells)
    {
        spell_type sp = slot.spell;
        if (!ret.empty())
            ret += ", ";
        if (spell_is_soh_breath(sp))
        {
            const vector<spell_type> *breaths = soh_breath_spells(sp);
            ASSERT(breaths);

            ret += "{";
            for (unsigned int k = 0; k < mp->number; ++k)
            {
                const spell_type breath = (*breaths)[k];
                const string rawname = spell_title(breath);
                ret += k == 0 ? "" : ", ";
                ret += make_stringf("head %d: ", k + 1)
                       + shorten_spell_name(rawname) + " (";
                ret +=
                    mons_human_readable_spell_damage_string(mp, breath) + ")";
            }
            ret += "}";

            ret += _spell_flag_string(slot);
            continue;
        }

        string spell_name = spell_title(sp);
        spell_name = shorten_spell_name(spell_name);
        ret += spell_name;
        ret += _spell_flag_string(slot);

        for (int j = 0; j < 100; j++)
        {
            string damage =
            mons_human_readable_spell_damage_string(mp, sp);
            const auto range = damages.equal_range(spell_name);
            if (!damage.empty()
                && none_of(range.first, range.second, [&](const pair<string,string>& entry){ return entry.first == spell_name && entry.second == damage; }))
            {
                // TODO: use emplace once we drop g++ 4.7 support
                damages.insert(make_pair(spell_name, damage));
            }
        }
    }
    spell_lists.insert(ret);
}

static string construct_spells(const set<string>& spell_lists,
                               const spell_damage_map& damages)
{
    string ret;
    for (const string& spell_list : spell_lists)
    {
        if (!ret.empty())
            ret += " / ";
        ret += spell_list;
    }
    map<string, string> merged_damages;
    for (const auto& entry : damages)
    {
        string &dam = merged_damages[entry.first];
        if (!dam.empty())
            dam += " / ";
        dam += entry.second;
    }

    for (const auto& entry : merged_damages)
    {
        ret = replace_all(ret, entry.first,
                          make_stringf("%s (%s)", entry.first.c_str(),
                                                  entry.second.c_str()));
    }

    return ret;
}

static inline void set_min_max(int num, int& min, int& max)
{
    if (!min || num < min)
        min = num;
    if (!max || num > max)
        max = num;
}

static string monster_symbol(const monster& mon)
{
    string symbol;
    const monsterentry* me = mon.find_monsterentry();
    if (me)
    {
        monster_info mi(&mon, MILEV_NAME);
        symbol += me->basechar;
        symbol = colour(mi.colour(), symbol);
    }
    return symbol;
}

static int _mi_create_monster(mons_spec spec)
{
    monster* monster =
        dgn_place_monster(spec, MONSTER_PLACE, true, false, false);
    if (monster)
    {
        monster->behaviour = BEH_SEEK;
        monster->foe = MHITYOU;
        no_messages mx;
        monster->del_ench(ENCH_SUBMERGED);
        return monster->mindex();
    }
    return NON_MONSTER;
}

static string damage_flavour(const string& name,
                                  const string& damage)
{
    return "(" + name + ":" + damage + ")";
}

static string damage_flavour(const string& name, int low, int high)
{
    return make_stringf("(%s:%d-%d)", name.c_str(), low, high);
}

static void rebind_mspec(string* requested_name,
                         const string& actual_name, mons_spec* mspec)
{
    if (*requested_name != actual_name
        && (requested_name->find("draconian") == 0
            || requested_name->find("blood saint") == 0
            || requested_name->find("corrupter") == 0
            || requested_name->find("warmonger") == 0
            || requested_name->find("black sun") == 0))
    {
        // If the user requested a drac, the game might generate a
        // coloured drac in response. Try to reuse that colour for further
        // tests.
        mons_list mons;
        const string err = mons.add_mons(actual_name, false);
        if (err.empty())
        {
            *mspec = mons.get_monster(0);
            *requested_name = actual_name;
        }
    }
}

const vector<string> monsters = {
#include "vault_monster_data.h"
};

/**
 * Return a vault-defined monster spec.
 *
 * This function parses the contents of (the generated) vault_monster_data.h
 * and attempts to return a specification. If there is an invalid specification,
 * no error will be recorded.
 *
 * @param monster_name Monster being searched for.
 * @param[out] vault_spec the spec found for the monster, if any
 * @return A mons_spec instance that either contains the relevant data, or
 *         nothing if not found.
 *
**/
static mons_spec _get_vault_monster(string monster_name, string* vault_spec)
{
    lowercase(monster_name);
    trim_string(monster_name);

    monster_name = replace_all_of(monster_name, "'", "");

    mons_list mons;
    mons_spec no_monster;

    for (const string& spec : monsters)
    {
        mons.clear();

        const string err = mons.add_mons(spec, false);
        if (err.empty())
        {
            mons_spec this_mons = mons.get_monster(0);
            int index = _mi_create_monster(this_mons);

            if (index < 0 || index >= MAX_MONSTERS)
                continue;

            bool this_spec = false;

            monster* mp = &menv[index];

            if (mp)
            {
                string name =
                    replace_all_of(mp->name(DESC_PLAIN, true), "'", "");
                lowercase(name);
                if (name == monster_name)
                    this_spec = true;
            }

            mons_remove_from_grid(*mp);

            if (this_spec)
            {
                if (vault_spec)
                    *vault_spec = spec;
                return this_mons;
            }
        }
    }

    if (vault_spec)
        *vault_spec = "";

    return no_monster;
}
static string canned_reports[][2] = {
    {"cang", ("cang (" + colour(LIGHTRED, "Ω")
              + (") | Spd: c | HD: i | HP: 666 | AC/EV: e/π | Dam: 999"
                 " | Res: sanity | XP: ∞ | Int: god | Sz: !!!"))},
};

int main(int argc, char* argv[])
{
    alarm(5);
    crawl_state.test = true;
    if (argc < 2)
    {
        printf("Usage: @? <monster name>\n");
        return 0;
    }

    if (!strcmp(argv[1], "-version") || !strcmp(argv[1], "--version"))
    {
        printf("Monster stats Crawl version: %s\n", Version::Long);
        return 0;
    }
    else if (!strcmp(argv[1], "-name") || !strcmp(argv[1], "--name"))
    {
        seed_rng();
        printf("%s\n", make_name().c_str());
        return 0;
    }

    initialize_crawl();

    mons_list mons;
    string target = argv[1];

    if (argc > 2)
        for (int x = 2; x < argc; x++)
        {
            target.append(" ");
            target.append(argv[x]);
        }

    trim_string(target);

    const bool want_vault_spec = target.find("spec:") == 0;
    if (want_vault_spec)
    {
        target.erase(0, 5);
        trim_string(target);
    }

    // [ds] Nobody mess with cang.
    for (const auto& row : canned_reports)
    {
        if (row[0] == target)
        {
            printf("%s\n", row[1].c_str());
            return 0;
        }
    }

    string orig_target = string(target);

    string err = mons.add_mons(target, false);
    if (!err.empty())
    {
        target = "the " + target;
        const string test = mons.add_mons(target, false);
        if (test.empty())
            err = test;
    }

    mons_spec spec = mons.get_monster(0);
    monster_type spec_type = static_cast<monster_type>(spec.type);
    bool vault_monster = false;
    string vault_spec;

    if ((spec_type < 0 || spec_type >= NUM_MONSTERS
         || spec_type == MONS_PLAYER_GHOST)
        || !err.empty())
    {
        spec = _get_vault_monster(orig_target, &vault_spec);
        spec_type = static_cast<monster_type>(spec.type);
        if (spec_type < 0 || spec_type >= NUM_MONSTERS
            || spec_type == MONS_PLAYER_GHOST)
        {
            if (err.empty())
                printf("unknown monster: \"%s\"\n", target.c_str());
            else
                printf("%s\n", err.c_str());
            return 1;
        }

        // _get_vault_monster created the monster; make uniques ungenerated again
        if (mons_is_unique(spec_type))
            you.unique_creatures.set(spec_type, false);

        vault_monster = true;
    }

    if (want_vault_spec)
    {
        if (!vault_monster)
        {
            printf("Not a vault monster: %s\n", orig_target.c_str());
            return 1;
        }
        else
        {
            printf("%s: %s\n", orig_target.c_str(), vault_spec.c_str());
            return 0;
        }
    }

    int index = _mi_create_monster(spec);
    if (index < 0 || index >= MAX_MONSTERS)
    {
        printf("Failed to create test monster for %s\n", target.c_str());
        return 1;
    }

    const int ntrials = 100;

    long exper = 0L;
    int hp_min = 0;
    int hp_max = 0;
    int mac = 0;
    int mev = 0;
    int speed_min = 0, speed_max = 0;
    // Calculate averages.
    set<string> spell_lists;
    spell_damage_map damages;
    for (int i = 0; i < ntrials; ++i)
    {
        monster* mp = &menv[index];
        const string mname = mp->name(DESC_PLAIN, true);
        exper += exper_value(*mp);
        mac += mp->armour_class();
        mev += mp->evasion();
        set_min_max(mp->speed, speed_min, speed_max);
        set_min_max(mp->hit_points, hp_min, hp_max);

        record_spell_set(mp, spell_lists, damages);

        // If it was a unique or had unrands, let it/them generate in future
        // iterations as well.
        for (int obj : mp->inv)
            if (obj != NON_ITEM)
                set_unique_item_status(mitm[obj], UNIQ_NOT_EXISTS);
        // Destroy the monster.
        mp->reset();
        you.unique_creatures.set(spec_type, false);

        rebind_mspec(&target, mname, &spec);

        index = _mi_create_monster(spec);
        if (index == -1)
        {
            printf("Unexpected failure generating monster for %s\n",
                   target.c_str());
            return 1;
        }
    }
    exper /= ntrials;
    mac /= ntrials;
    mev /= ntrials;

    monster& mon(menv[index]);

    const string symbol(monster_symbol(mon));

    const bool shapeshifter = mon.is_shapeshifter()
                              || spec_type == MONS_SHAPESHIFTER
                              || spec_type == MONS_GLOWING_SHAPESHIFTER;

    const bool nonbase =
        mons_species(mon.type) == MONS_DRACONIAN && mon.type != MONS_DRACONIAN
        || mons_species(mon.type) == MONS_DEMONSPAWN
               && mon.type != MONS_DEMONSPAWN;

    const monsterentry* me =
        shapeshifter ? get_monster_data(spec_type) : mon.find_monsterentry();

    const monsterentry* mbase =
        nonbase ? get_monster_data(draco_or_demonspawn_subspecies(mon)) :
                  (monsterentry*)0;

    if (me)
    {
        string monsterflags;
        string monsterresistances;
        string monstervulnerabilities;
        string monsterattacks;

        lowercase(target);

        const bool changing_name =
            mon.has_hydra_multi_attack() || mon.type == MONS_PANDEMONIUM_LORD
            || shapeshifter || mon.type == MONS_DANCING_WEAPON;

        printf("%s (%s)",
               changing_name ? me->name : mon.name(DESC_PLAIN, true).c_str(),
               symbol.c_str());

        if (mons_class_flag(mon.type, M_UNFINISHED))
            printf(" | %s", colour(LIGHTRED, "UNFINISHED").c_str());

        printf(" | Spd: %s",
               monster_speed(mon, me, speed_min, speed_max).c_str());

        const int hd = mon.get_experience_level();
        printf(" | HD: %d", hd);

        printf(" | HP: ");
        const int hplow = hp_min;
        const int hphigh = hp_max;
        if (hplow < hphigh)
            printf("%i-%i", hplow, hphigh);
        else
            printf("%i", hplow);

        printf(" | AC/EV: %i/%i", mac, mev);

        string defenses;
        if (mon.is_spiny() > 0)
            defenses += colour(YELLOW, "(spiny 5d4)");
        if (mons_species(mons_base_type(mon)) == MONS_MINOTAUR)
            defenses += colour(LIGHTRED, "(headbutt: d20-1)");
        if (!defenses.empty())
            printf(" %s", defenses.c_str());

        mon.wield_melee_weapon();
        for (int x = 0; x < 4; x++)
        {
            mon_attack_def orig_attk(me->attack[x]);
            int attack_num = x;
            if (mon.has_hydra_multi_attack())
                attack_num = x == 0 ? x : x + mon.number - 1;
            mon_attack_def attk = mons_attack_spec(mon, attack_num);
            if (attk.type)
            {
                if (monsterattacks.empty())
                    monsterattacks = " | Dam: ";
                else
                    monsterattacks += ", ";

                short int dam = attk.damage;
                if (mon.has_ench(ENCH_BERSERK) || mon.has_ench(ENCH_MIGHT))
                    dam = dam * 3 / 2;

                if (mon.has_ench(ENCH_WEAK))
                    dam = dam * 2 / 3;

                monsterattacks += to_string(dam);

                if (attk.type == AT_CONSTRICT)
                    monsterattacks += colour(GREEN, "(constrict)");

                if (attk.type == AT_CLAW && mon.has_claws() >= 3)
                    monsterattacks += colour(LIGHTGREEN, "(claw)");

                const attack_flavour flavour(orig_attk.flavour == AF_KLOWN
                                                     || orig_attk.flavour
                                                            == AF_DRAIN_STAT ?
                                                 orig_attk.flavour :
                                                 attk.flavour);

                switch (flavour)
                {
                case AF_REACH:
                case AF_REACH_STING:
                    monsterattacks += "(reach)";
                    break;
                case AF_KITE:
                    monsterattacks += "(kite)";
                    break;
                case AF_SWOOP:
                    monsterattacks += "(swoop)";
                    break;
                case AF_ACID:
                    monsterattacks +=
                        colour(YELLOW, damage_flavour("acid", "7d3"));
                    break;
                case AF_BLINK:
                    monsterattacks += colour(MAGENTA, "(blink self)");
                    break;
                case AF_COLD:
                    monsterattacks += colour(
                        LIGHTBLUE, damage_flavour("cold", hd, 3 * hd - 1));
                    break;
                case AF_CONFUSE:
                    monsterattacks += colour(LIGHTMAGENTA, "(confuse)");
                    break;
                case AF_DRAIN_DEX:
                    monsterattacks += colour(RED, "(drain dexterity)");
                    break;
                case AF_DRAIN_STR:
                    monsterattacks += colour(RED, "(drain strength)");
                    break;
                case AF_DRAIN_XP:
                    monsterattacks += colour(LIGHTMAGENTA, "(drain)");
                    break;
                case AF_CHAOTIC:
                    monsterattacks += colour(LIGHTGREEN, "(chaos)");
                    break;
                case AF_ELEC:
                    monsterattacks +=
                        colour(LIGHTCYAN,
                               damage_flavour("elec", hd,
                                              hd + max(hd / 2 - 1, 0)));
                    break;
                case AF_FIRE:
                    monsterattacks += colour(
                        LIGHTRED, damage_flavour("fire", hd, hd * 2 - 1));
                    break;
                case AF_PURE_FIRE:
                    monsterattacks +=
                        colour(LIGHTRED, damage_flavour("pure fire", hd * 3 / 2,
                                                        hd * 5 / 2 - 1));
                    break;
                case AF_STICKY_FLAME:
                    monsterattacks += colour(LIGHTRED, "(napalm)");
                    break;
                case AF_HUNGER:
                    monsterattacks += colour(BLUE, "(hunger)");
                    break;
                case AF_MUTATE:
                    monsterattacks += colour(LIGHTGREEN, "(mutation)");
                    break;
                case AF_POISON_PARALYSE:
                    monsterattacks += colour(LIGHTRED, "(paralyse)");
                    break;
                case AF_POISON:
                    monsterattacks += colour(
                        YELLOW, damage_flavour("poison", hd * 2, hd * 4));
                    break;
                case AF_POISON_STRONG:
                    monsterattacks += colour(
                        LIGHTRED, damage_flavour("strong poison", hd * 11 / 3,
                                                 hd * 13 / 2));
                    break;
                case AF_ROT:
                    monsterattacks += colour(LIGHTRED, "(rot)");
                    break;
                case AF_VAMPIRIC:
                    monsterattacks += colour(RED, "(vampiric)");
                    break;
                case AF_KLOWN:
                    monsterattacks += colour(LIGHTBLUE, "(klown)");
                    break;
                case AF_SCARAB:
                    monsterattacks += colour(LIGHTMAGENTA, "(scarab)");
                    break;
                case AF_DISTORT:
                    monsterattacks += colour(LIGHTBLUE, "(distort)");
                    break;
                case AF_RAGE:
                    monsterattacks += colour(RED, "(rage)");
                    break;
                case AF_HOLY:
                    monsterattacks += colour(YELLOW, "(holy)");
                    break;
                case AF_PAIN:
                    monsterattacks += colour(RED, "(pain)");
                    break;
                case AF_ANTIMAGIC:
                    monsterattacks += colour(LIGHTBLUE, "(antimagic)");
                    break;
                case AF_DRAIN_INT:
                    monsterattacks += colour(BLUE, "(drain int)");
                    break;
                case AF_DRAIN_STAT:
                    monsterattacks += colour(BLUE, "(drain stat)");
                    break;
                case AF_STEAL:
                    monsterattacks += colour(CYAN, "(steal)");
                    break;
                case AF_ENSNARE:
                    monsterattacks += colour(WHITE, "(ensnare)");
                    break;
                case AF_DROWN:
                    monsterattacks += colour(LIGHTBLUE, "(drown)");
                    break;
                case AF_ENGULF:
                    monsterattacks += colour(LIGHTBLUE, "(engulf)");
                    break;
                case AF_DRAIN_SPEED:
                    monsterattacks += colour(LIGHTMAGENTA, "(drain speed)");
                    break;
                case AF_VULN:
                    monsterattacks += colour(LIGHTBLUE, "(vuln)");
                    break;
                case AF_SHADOWSTAB:
                    monsterattacks += colour(MAGENTA, "(shadow stab)");
                    break;
                case AF_CORRODE:
                    monsterattacks += colour(BROWN, "(corrosion)");
                    break;
                case AF_TRAMPLE:
                    monsterattacks += colour(BROWN, "(trample)");
                    break;
                case AF_WEAKNESS:
                    monsterattacks += colour(LIGHTRED, "(weakness)");
                    break;
                case AF_CRUSH:
                case AF_PLAIN:
                    break;
#if TAG_MAJOR_VERSION == 34
                case AF_DISEASE:
                case AF_PLAGUE:
                case AF_STEAL_FOOD:
                case AF_POISON_MEDIUM:
                case AF_POISON_NASTY:
                case AF_POISON_STR:
                case AF_POISON_DEX:
                case AF_POISON_INT:
                case AF_POISON_STAT:
                case AF_FIREBRAND:
                case AF_MIASMATA:
                    monsterattacks += colour(LIGHTRED, "(?\?\?)");
                    break;
#endif
                    // let the compiler issue warnings for us
                    //      default:
                    //        monsterattacks += "(???)";
                    //        break;
                }

                if (x == 0 && mon.has_hydra_multi_attack())
                    monsterattacks += " per head";
            }
        }

        printf("%s", monsterattacks.c_str());

        mons_check_flag((bool)(me->holiness & MH_NATURAL)
                        && (bool)(me->holiness & ~MH_NATURAL),
                        monsterflags, "natural");
        mons_check_flag(mon.is_holy(), monsterflags, colour(YELLOW, "holy"));
        mons_check_flag((bool)(me->holiness & MH_UNDEAD),
                        monsterflags, colour(BROWN, "undead"));
        mons_check_flag((bool)(me->holiness & MH_DEMONIC),
                        monsterflags, colour(RED, "demonic"));
        mons_check_flag((bool)(me->holiness & MH_NONLIVING),
                        monsterflags, colour(LIGHTCYAN, "non-living"));
        mons_check_flag(mons_is_plant(mon), monsterflags,
                        colour(GREEN, "plant"));

        switch (me->gmon_use)
        {
        case MONUSE_WEAPONS_ARMOUR:
            mons_flag(monsterflags, colour(CYAN, "weapons"));
        // intentional fall-through
        case MONUSE_STARTING_EQUIPMENT:
            mons_flag(monsterflags, colour(CYAN, "items"));
        // intentional fall-through
        case MONUSE_OPEN_DOORS:
            mons_flag(monsterflags, colour(CYAN, "doors"));
        // intentional fall-through
        case MONUSE_NOTHING:
            break;

        case NUM_MONUSE: // Can't happen
            mons_flag(monsterflags, colour(CYAN, "uses bugs"));
            break;
        }

        mons_check_flag(bool(me->bitfields & M_EAT_DOORS), monsterflags,
                        colour(LIGHTRED, "eats doors"));
        mons_check_flag(bool(me->bitfields & M_CRASH_DOORS), monsterflags,
                        colour(LIGHTRED, "breaks doors"));

        mons_check_flag(mons_wields_two_weapons(mon), monsterflags,
                        "two-weapon");
        mons_check_flag(mon.is_fighter(), monsterflags, "fighter");
        if (mon.is_archer())
        {
            if (me->bitfields & M_DONT_MELEE)
                mons_flag(monsterflags, "master archer");
            else
                mons_flag(monsterflags, "archer");
        }
        mons_check_flag(mon.is_priest(), monsterflags, "priest");

        mons_check_flag(me->habitat == HT_AMPHIBIOUS, monsterflags,
                        "amphibious");

        mons_check_flag(mon.evil(), monsterflags, "evil");
        mons_check_flag(mon.is_actual_spellcaster(), monsterflags,
                        "spellcaster");
        mons_check_flag(bool(me->bitfields & M_COLD_BLOOD), monsterflags,
                        "cold-blooded");
        mons_check_flag(bool(me->bitfields & M_SEE_INVIS), monsterflags,
                        "see invisible");
        mons_check_flag(bool(me->bitfields & M_FLIES), monsterflags, "fly");
        mons_check_flag(bool(me->bitfields & M_FAST_REGEN), monsterflags,
                        "regen");
        mons_check_flag(mon.can_cling_to_walls(), monsterflags, "cling");
        mons_check_flag(bool(me->bitfields & M_WEB_SENSE), monsterflags,
                        "web sense");
        mons_check_flag(mon.is_unbreathing(), monsterflags, "unbreathing");

        string spell_string = construct_spells(spell_lists, damages);
        if (shapeshifter || mon.type == MONS_PANDEMONIUM_LORD
            || mon.type == MONS_CHIMERA
                   && (mon.base_monster == MONS_PANDEMONIUM_LORD))
        {
            spell_string = "(random)";
        }

        mons_check_flag(vault_monster, monsterflags, colour(BROWN, "vault"));

        printf("%s", monsterflags.c_str());

        if (me->resist_magic == 5000)
        {
            if (monsterresistances.empty())
                monsterresistances = " | Res: ";
            else
                monsterresistances += ", ";
            monsterresistances += colour(LIGHTMAGENTA, "magic(immune)");
        }
        else if (me->resist_magic < 0)
        {
            const int res = (mbase) ? mbase->resist_magic : me->resist_magic;
            if (monsterresistances.empty())
                monsterresistances = " | Res: ";
            else
                monsterresistances += ", ";
            monsterresistances +=
                colour(MAGENTA,
                       "magic(" + to_string(hd * res * 4 / 3 * -1) + ")");
        }
        else if (me->resist_magic > 0)
        {
            if (monsterresistances.empty())
                monsterresistances = " | Res: ";
            else
                monsterresistances += ", ";
            monsterresistances += colour(
                MAGENTA, "magic(" + to_string(me->resist_magic) + ")");
        }

        const resists_t res(shapeshifter ? me->resists :
                                           get_mons_resists(mon));
#define res(c, x)                                                              \
    do                                                                         \
    {                                                                          \
        record_resist(c, lowercase_string(#x), monsterresistances,             \
                      monstervulnerabilities, get_resist(res, MR_RES_##x));    \
    } while (false)

#define res2(c, x, y)                                                          \
    do                                                                         \
    {                                                                          \
        record_resist(c, #x, monsterresistances, monstervulnerabilities, y);   \
    } while (false)

        // Don't record regular rF as damnation vulnerability.
        res(RED, FIRE);
        res(RED, DAMNATION);
        res(BLUE, COLD);
        res(CYAN, ELEC);
        res(GREEN, POISON);
        res(BROWN, ACID);
        res(0, STEAM);

        if (me->bitfields & M_UNBLINDABLE)
            res2(YELLOW, blind, 1);

        res2(LIGHTBLUE, drown, mon.res_water_drowning());
        res2(LIGHTRED, rot, mon.res_rotting());
        res2(LIGHTMAGENTA, neg, mon.res_negative_energy(true));
        res2(YELLOW, holy, mon.res_holy_energy());
        res2(LIGHTMAGENTA, torm, mon.res_torment());
        res2(LIGHTBLUE, tornado, mon.res_tornado());
        res2(LIGHTRED, napalm, mon.res_sticky_flame());
        res2(LIGHTCYAN, silver, mon.how_chaotic() ? -1 : 0);

        printf("%s", monsterresistances.c_str());
        printf("%s", monstervulnerabilities.c_str());

        if (me->corpse_thingy != CE_NOCORPSE && me->corpse_thingy != CE_CLEAN)
        {
            printf(" | Chunks: ");
            switch (me->corpse_thingy)
            {
            case CE_NOXIOUS:
                printf("%s", colour(DARKGREY, "noxious").c_str());
                break;
            // We should't get here; including these values so we can get
            // compiler
            // warnings for unhandled enum values.
            case CE_NOCORPSE:
            case CE_CLEAN:
                printf("???");
            }
        }

        printf(" | XP: %ld", exper);

        if (!spell_string.empty())
            printf(" | Sp: %s", spell_string.c_str());

        printf(" | Sz: %s", monster_size(mon).c_str());

        printf(" | Int: %s", monster_int(mon).c_str());

        printf(".\n");

        return 0;
    }
    return 1;
}

//////////////////////////////////////////////////////////////////////////
// main.cc stuff

CLua clua(true);
CLua dlua(false);      // Lua interpreter for the dungeon builder.
crawl_environment env; // Requires dlua.
player you;
game_state crawl_state;

void process_command(command_type);
void process_command(command_type) {}

void world_reacts();
void world_reacts() {}
