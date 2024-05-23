/**
 * @file
 * @brief Functions used to print information about various game objects.
**/

#include "AppHdr.h"

#include "describe.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

#include "ability.h"
#include "adjust.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "branch.h"
#include "cloud.h" // cloud_type_name
#include "clua.h"
#include "colour.h"
#include "command.h"
#include "database.h"
#include "dbg-util.h"
#include "decks.h"
#include "delay.h"
#include "describe-spells.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "tile-env.h"
#include "evoke.h"
#include "fight.h"
#include "ghost.h"
#include "god-abil.h"
#include "god-item.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "jobs.h"
#include "lang-fake.h"
#include "libutil.h"
#include "macro.h"
#include "melee-attack.h" // describe_to_hit
#include "message.h"
#include "mon-behv.h"
#include "mon-cast.h" // mons_spell_range
#include "mon-death.h"
#include "mon-explode.h" // mon_explode_dam/on_death
#include "mon-tentacle.h"
#include "movement.h"
#include "mutation.h" // mutation_name, get_mutation_desc
#include "output.h"
#include "potion.h"
#include "prompt.h"
#include "player.h"
#include "ranged-attack.h" // describe_to_hit
#include "religion.h"
#include "rltiles/tiledef-feat.h"
#include "skills.h"
#include "species.h"
#include "spl-cast.h"
#include "spl-book.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h" // to_string on Cygwin
#include "tag-version.h"
#include "terrain.h"
#include "tile-flags.h"
#include "tilepick.h"
#ifdef USE_TILE_LOCAL
 #include "tilereg-crt.h"
 #include "rltiles/tiledef-dngn.h"
#endif
#ifdef USE_TILE
 #include "tileview.h"
#endif
#include "transform.h"
#include "travel.h"
#include "unicode.h"
#include "viewchar.h"
#include "viewmap.h"

using namespace ui;

static command_type _get_action(int key, vector<command_type> actions);
static void _print_bar(int value, int scale, const string &name,
                       ostringstream &result, int base_value = INT_MAX);

static void _describe_mons_to_hit(const monster_info& mi, ostringstream &result);
static string _describe_weapon_brand(const item_def &item);

struct property_descriptor;
static const property_descriptor & _get_artp_desc_data(artefact_prop_type p);

int show_description(const string &body, const tile_def *tile)
{
    describe_info inf;
    inf.body << body;
    return show_description(inf, tile);
}

int show_description(const describe_info &inf, const tile_def *tile)
{
    auto vbox = make_shared<Box>(Widget::VERT);

    if (!inf.title.empty())
    {
        auto title_hbox = make_shared<Box>(Widget::HORZ);

#ifdef USE_TILE
        if (tile)
        {
            auto icon = make_shared<Image>();
            icon->set_tile(*tile);
            icon->set_margin_for_sdl(0, 10, 0, 0);
            title_hbox->add_child(std::move(icon));
        }
#else
        UNUSED(tile);
#endif

        auto title = make_shared<Text>(inf.title);
        title_hbox->add_child(std::move(title));

        title_hbox->set_cross_alignment(Widget::CENTER);
        title_hbox->set_margin_for_sdl(0, 0, 20, 0);
        title_hbox->set_margin_for_crt(0, 0, 1, 0);
        vbox->add_child(std::move(title_hbox));
    }

    auto desc_sw = make_shared<Switcher>();
    auto more_sw = make_shared<Switcher>();
    desc_sw->current() = 0;
    more_sw->current() = 0;

    const string descs[2] =  {
        trimmed_string(process_description(inf, false)),
        trimmed_string(inf.quote),
    };

    // TODO: maybe use CMD_MENU_CYCLE_MODE
    const char* mores[2] = {
        "[<w>!</w>]: <w>Description</w>|Quote",
        "[<w>!</w>]: Description|<w>Quote</w>",
    };

    for (int i = 0; i < (inf.quote.empty() ? 1 : 2); i++)
    {
        const auto &desc = descs[static_cast<int>(i)];
        auto scroller = make_shared<Scroller>();
        auto fs = formatted_string::parse_string(trimmed_string(desc));
        auto text = make_shared<Text>(fs);
        text->set_wrap_text(true);
        scroller->set_child(text);
        desc_sw->add_child(std::move(scroller));
        more_sw->add_child(make_shared<Text>(
                formatted_string::parse_string(mores[i])));
    }

    more_sw->set_margin_for_sdl(20, 0, 0, 0);
    more_sw->set_margin_for_crt(1, 0, 0, 0);
    desc_sw->expand_h = false;
    desc_sw->align_x = Widget::STRETCH;
    vbox->add_child(desc_sw);
    if (!inf.quote.empty())
        vbox->add_child(more_sw);

#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif

    auto popup = make_shared<ui::Popup>(vbox);

    bool done = false;
    int lastch;
    popup->on_keydown_event([&](const KeyEvent& ev) {
        lastch = ev.key();
        if (!inf.quote.empty() && (lastch == '!' || lastch == '^'))
            desc_sw->current() = more_sw->current() = 1 - desc_sw->current();
        else
            done = ui::key_exits_popup(lastch, true);
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    if (tile)
    {
        tiles.json_open_object("tile");
        tiles.json_write_int("t", tile->tile);
        tiles.json_write_int("tex", get_tile_texture(tile->tile));
        if (tile->ymax != TILE_Y)
            tiles.json_write_int("ymax", tile->ymax);
        tiles.json_close_object();
    }
    tiles.json_write_string("title", inf.title);
    tiles.json_write_string("prefix", inf.prefix);
    tiles.json_write_string("suffix", inf.suffix);
    tiles.json_write_string("footer", inf.footer);
    tiles.json_write_string("quote", inf.quote);
    tiles.json_write_string("body", inf.body.str());
    tiles.push_ui_layout("describe-generic", 0);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    ui::run_layout(std::move(popup), done);

    return lastch;
}

string process_description(const describe_info &inf, bool include_title)
{
    string desc;
    if (!inf.prefix.empty())
        desc += "\n\n" + trimmed_string(filtered_lang(inf.prefix));
    if (!inf.title.empty() && include_title)
        desc += "\n\n" + trimmed_string(filtered_lang(inf.title));
    desc += "\n\n" + trimmed_string(filtered_lang(inf.body.str()));
    if (!inf.suffix.empty())
        desc += "\n\n" + trimmed_string(filtered_lang(inf.suffix));
    if (!inf.footer.empty())
        desc += "\n\n" + trimmed_string(filtered_lang(inf.footer));
    trim_string(desc);
    return desc;
}

const char* jewellery_base_ability_string(int subtype)
{
    switch (subtype)
    {
#if TAG_MAJOR_VERSION == 34
    case RING_SUSTAIN_ATTRIBUTES: return "SustAt";
#endif
    case RING_FIRE:               return "Fire";
    case RING_ICE:                return "Ice";
#if TAG_MAJOR_VERSION == 34
    case RING_TELEPORTATION:      return "*Tele";
    case RING_TELEPORT_CONTROL:   return "+cTele";
    case AMU_HARM:                return "Harm";
    case AMU_THE_GOURMAND:        return "Gourm";
#endif
    case AMU_ACROBAT:             return "Acrobat";
#if TAG_MAJOR_VERSION == 34
    case AMU_CONSERVATION:        return "Cons";
    case AMU_CONTROLLED_FLIGHT:   return "cFly";
#endif
    case AMU_GUARDIAN_SPIRIT:     return "Spirit";
    case AMU_FAITH:               return "Faith";
    case AMU_REFLECTION:          return "Reflect";
#if TAG_MAJOR_VERSION == 34
    case AMU_INACCURACY:          return "Inacc";
#endif
    }
    return "";
}

#define known_proprt(prop) (proprt[(prop)] && known[(prop)])

/// How to display props of a given type?
enum class prop_note
{
    /// The raw numeral; e.g "Slay+3", "Int-1"
    numeral,
    /// Plusses and minuses; "rF-", "rC++"
    symbolic,
    /// Don't note the number; e.g. "rMut"
    plain,
};

struct property_descriptor
{
    artefact_prop_type property;
    const char* desc;           // If it contains %d, will be replaced by value.
    prop_note display_type;
};

// TODO: the prop_note values seem at least partly redundant with value_types
// on the regular artefact prop data? Could this be refactored into that
// structure?
static const vector<property_descriptor> & _get_all_artp_desc_data()
{
    // order here determines order of long form descriptions
    // TODO: why not just use the same order as annotations?
    static vector<property_descriptor> data =
    {
        { ARTP_AC,
            "It affects your AC (%d).",
            prop_note::numeral },
        { ARTP_EVASION,
            "It affects your evasion (%d).",
            prop_note::numeral },
        { ARTP_STRENGTH,
            "It affects your strength (%d).",
            prop_note::numeral },
        { ARTP_INTELLIGENCE,
            "It affects your intelligence (%d).",
            prop_note::numeral },
        { ARTP_DEXTERITY,
            "It affects your dexterity (%d).",
            prop_note::numeral },
        { ARTP_SLAYING,
            "It affects your accuracy & damage with ranged weapons and melee (%d).",
            prop_note::numeral },
        { ARTP_FIRE,
            "fire",
            prop_note::symbolic },
        { ARTP_COLD,
            "cold",
            prop_note::symbolic },
        { ARTP_ELECTRICITY,
            "It insulates you from electricity.",
            prop_note::plain },
        { ARTP_POISON,
            "It protects you from poison.",
            prop_note::plain },
        { ARTP_NEGATIVE_ENERGY,
            "negative energy",
            prop_note::symbolic },
        { ARTP_WILLPOWER,
            "buggy willpower",
            prop_note::symbolic },
        { ARTP_HP,
            "It affects your health (%d).",
            prop_note::numeral },
        { ARTP_MAGICAL_POWER,
            "It affects your magic capacity (%d).",
            prop_note::numeral },
        { ARTP_SEE_INVISIBLE,
            "It lets you see invisible.",
            prop_note::plain },
        { ARTP_INVISIBLE,
            "It lets you turn invisible.",
            prop_note::plain },
        { ARTP_FLY,
            "It grants you flight.",
            prop_note::plain },
        { ARTP_BLINK,
            "It lets you blink.",
            prop_note::plain },
        { ARTP_NOISE,
            "It may make a loud noise when swung.",
            prop_note::plain },
        { ARTP_PREVENT_SPELLCASTING,
            "It prevents spellcasting.",
            prop_note::plain },
        { ARTP_PREVENT_TELEPORTATION,
            "It prevents most forms of teleportation.",
            prop_note::plain },
        { ARTP_ANGRY,
            "It berserks you when you make melee attacks (%d% chance).",
            prop_note::plain },
        { ARTP_CLARITY,
            "It protects you from confusion, rage, mesmerisation and fear.",
            prop_note::plain },
        { ARTP_CONTAM,
            "It causes magical contamination when unequipped.",
            prop_note::plain },
        { ARTP_RMSL,
            "It protects you from missiles.",
            prop_note::plain },
        { ARTP_REGENERATION,
            "It increases your rate of health regeneration.",
            prop_note::symbolic },
        { ARTP_RCORR,
            "It protects you from acid and corrosion.",
            prop_note::plain },
        { ARTP_RMUT,
            "It protects you from mutation.",
            prop_note::plain },
        { ARTP_CORRODE,
            "It may corrode you when you take damage.",
            prop_note::plain },
        { ARTP_DRAIN,
            "It drains your maximum health when unequipped.",
            prop_note::plain },
        { ARTP_SLOW,
            "It may slow you when you take damage.",
            prop_note::plain },
        { ARTP_FRAGILE,
            "It will be destroyed if unequipped.",
            prop_note::plain },
        { ARTP_SHIELDING,
            "It affects your SH (%d).",
            prop_note::numeral },
        { ARTP_HARM,
            "It increases damage dealt and taken.",
            prop_note::plain },
        { ARTP_RAMPAGING,
            "It bestows one free step when moving towards enemies.",
            prop_note::plain },
        { ARTP_STEALTH,
            "buggy stealth",
            prop_note::symbolic },
        { ARTP_ARCHMAGI,
            "It increases the power of your magical spells.",
            prop_note::plain },
        { ARTP_ENHANCE_CONJ,
            "It increases the power of your Conjurations spells.",
            prop_note::plain },
        { ARTP_ENHANCE_HEXES,
            "It increases the power of your Hexes spells.",
            prop_note::plain },
        { ARTP_ENHANCE_SUMM,
            "It increases the power of your Summonings spells.",
            prop_note::plain },
        { ARTP_ENHANCE_NECRO,
            "It increases the power of your Necromancy spells.",
            prop_note::plain },
        { ARTP_ENHANCE_TLOC,
            "It increases the power of your Translocations spells.",
            prop_note::plain },
        { ARTP_ENHANCE_FIRE,
            "It increases the power of your Fire spells.",
            prop_note::plain },
        { ARTP_ENHANCE_ICE,
            "It increases the power of your Ice spells.",
            prop_note::plain },
        { ARTP_ENHANCE_AIR,
            "It increases the power of your Air spells.",
            prop_note::plain },
        { ARTP_ENHANCE_EARTH,
            "It increases the power of your Earth spells.",
            prop_note::plain },
        { ARTP_ENHANCE_ALCHEMY,
            "It increases the power of your Alchemy spells.",
            prop_note::plain },
        { ARTP_ACROBAT,
            "It increases your evasion after moving or waiting.",
            prop_note::plain },
        { ARTP_MANA_REGENERATION,
            "It increases your rate of magic regeneration.",
            prop_note::symbolic },
        { ARTP_WIZARDRY,
            "It increases the success rate of your magical spells.",
            prop_note::plain },
    };
    return data;
}

static string _randart_prop_abbrev(artefact_prop_type prop, int val)
{
    const property_descriptor &ann = _get_artp_desc_data(prop);
    switch (ann.display_type)
    {
    case prop_note::numeral: // e.g. AC+4
        return make_stringf("%s%+d", artp_name(prop), val);
    case prop_note::symbolic: // e.g. F++
    {
        const char symbol = val > 0 ? '+' : '-';
        if (val > 4)
            return make_stringf("%s%c%d", artp_name(prop), symbol, abs(val));
        else
            return make_stringf("%s%s", artp_name(prop), string(abs(val), symbol).c_str());
    }
    case prop_note::plain: // e.g. rPois or SInv
        return artp_name(prop);
    }
    return "buggy";
}

static vector<string> _randart_propnames(const item_def& item,
                                         bool no_comma = false)
{
    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_desc_properties(item, proprt, known);

    vector<string> propnames;

    static const vector<artefact_prop_type> annotation_order =
    {
        // (Generally) negative attributes
        // These come first, so they don't get chopped off!
        ARTP_PREVENT_SPELLCASTING,
        ARTP_PREVENT_TELEPORTATION,
        ARTP_CONTAM,
        ARTP_ANGRY,
        ARTP_NOISE,
        ARTP_HARM,
        ARTP_RAMPAGING,
        ARTP_ACROBAT,
        ARTP_CORRODE,
        ARTP_DRAIN,
        ARTP_SLOW,
        ARTP_FRAGILE,

        // Evokable abilities come second
        ARTP_BLINK,
        ARTP_INVISIBLE,
        ARTP_FLY,

        // Resists, also really important
        ARTP_ELECTRICITY,
        ARTP_POISON,
        ARTP_FIRE,
        ARTP_COLD,
        ARTP_NEGATIVE_ENERGY,
        ARTP_WILLPOWER,
        ARTP_REGENERATION,
        ARTP_MANA_REGENERATION,
        ARTP_RMUT,
        ARTP_RCORR,

        // Quantitative attributes
        ARTP_HP,
        ARTP_MAGICAL_POWER,
        ARTP_WIZARDRY,
        ARTP_AC,
        ARTP_EVASION,
        ARTP_STRENGTH,
        ARTP_INTELLIGENCE,
        ARTP_DEXTERITY,
        ARTP_SLAYING,
        ARTP_SHIELDING,

        // Qualitative attributes (and Stealth)
        ARTP_SEE_INVISIBLE,
        ARTP_STEALTH,
        ARTP_CLARITY,
        ARTP_RMSL,

        // spell enhancers
        ARTP_ARCHMAGI,
        ARTP_ENHANCE_CONJ,
        ARTP_ENHANCE_HEXES,
        ARTP_ENHANCE_SUMM,
        ARTP_ENHANCE_NECRO,
        ARTP_ENHANCE_TLOC,
        ARTP_ENHANCE_FIRE,
        ARTP_ENHANCE_ICE,
        ARTP_ENHANCE_AIR,
        ARTP_ENHANCE_EARTH,
        ARTP_ENHANCE_ALCHEMY,
    };

    const unrandart_entry *entry = nullptr;
    if (is_unrandom_artefact(item))
        entry = get_unrand_entry(item.unrand_idx);
    const bool skip_ego = is_unrandom_artefact(item)
                          && entry && entry->flags & UNRAND_FLAG_SKIP_EGO;

    // For randart jewellery, note the base jewellery type if it's not
    // covered by artefact_desc_properties()
    if (item.base_type == OBJ_JEWELLERY
        && (item_ident(item, ISFLAG_KNOW_PROPERTIES)) && !skip_ego)
    {
        const char* type = jewellery_base_ability_string(item.sub_type);
        if (*type)
            propnames.push_back(type);
    }
    else if (item_brand_known(item)
             && !skip_ego)
    {
        string ego;
        if (item.base_type == OBJ_WEAPONS)
            ego = weapon_brand_name(item, true);
        else if (item.base_type == OBJ_ARMOUR)
            ego = armour_ego_name(item, true);
        else if (item.base_type == OBJ_GIZMOS)
            ego = gizmo_effect_name(item.brand);
        if (!ego.empty())
        {
            // XXX: Ugly hack for adding a comma if needed.
            bool extra_props = false;
            for (const artefact_prop_type &other_prop : annotation_order)
                if (known_proprt(other_prop) && other_prop != ARTP_BRAND)
                {
                    extra_props = true;
                    break;
                }

            if (!no_comma && extra_props
                || is_unrandom_artefact(item)
                   && entry && entry->inscrip != nullptr)
            {
                ego += ",";
            }

            propnames.push_back(ego);
        }
    }

    if (is_unrandom_artefact(item) && entry && entry->inscrip != nullptr)
        propnames.push_back(entry->inscrip);

    for (const artefact_prop_type &prop : annotation_order)
    {
        if (known_proprt(prop))
        {
            const int val = proprt[prop];

            // Don't show rF+/rC- for =Fire, or vice versa for =Ice.
            if (item.base_type == OBJ_JEWELLERY)
            {
                if (item.sub_type == RING_FIRE
                    && (prop == ARTP_FIRE && val == 1
                        || prop == ARTP_COLD && val == -1))
                {
                    continue;
                }
                if (item.sub_type == RING_ICE
                    && (prop == ARTP_COLD && val == 1
                        || prop == ARTP_FIRE && val == -1))
                {
                    continue;
                }
            }

            // Don't show the rF+ rC+ twice.
            if (get_armour_ego_type(item) == SPARM_RESISTANCE
                && (prop == ARTP_COLD && val == 1
                    || prop == ARTP_FIRE && val == 1))
            {
                continue;
            }

            propnames.push_back(_randart_prop_abbrev(prop, val));
        }
    }

    return propnames;
}

string artefact_inscription(const item_def& item)
{
    if (item.base_type == OBJ_BOOKS)
        return "";

    const vector<string> propnames = _randart_propnames(item);

    string insc = comma_separated_line(propnames.begin(), propnames.end(),
                                       " ", " ");
    if (!insc.empty() && insc[insc.length() - 1] == ',')
        insc.erase(insc.length() - 1);
    return insc;
}

void add_inscription(item_def &item, string inscrip)
{
    if (!item.inscription.empty())
    {
        if (ends_with(item.inscription, ","))
            item.inscription += " ";
        else
            item.inscription += ", ";
    }

    item.inscription += inscrip;
}

static const char* _jewellery_base_ability_description(int subtype)
{
    switch (subtype)
    {
#if TAG_MAJOR_VERSION == 34
    case RING_SUSTAIN_ATTRIBUTES:
        return "It sustains your strength, intelligence and dexterity.";
#endif
    case RING_WIZARDRY:
        return "It improves your spell success rate.";
    case RING_FIRE:
        return "It enhances your fire magic.";
    case RING_ICE:
        return "It enhances your ice magic.";
#if TAG_MAJOR_VERSION == 34
    case RING_TELEPORTATION:
        return "It may teleport you next to monsters.";
    case RING_TELEPORT_CONTROL:
        return "It can be evoked for teleport control.";
    case AMU_HARM:
        return "It increases damage dealt and taken.";
    case AMU_THE_GOURMAND:
        return "It allows you to eat raw meat even when not hungry.";
#endif
    case AMU_MANA_REGENERATION:
        return "It increases your rate of magic regeneration.";
    case AMU_ACROBAT:
        return "It increases your evasion while moving and waiting.";
#if TAG_MAJOR_VERSION == 34
    case AMU_CONSERVATION:
        return "It protects your inventory from destruction.";
#endif
    case AMU_GUARDIAN_SPIRIT:
        return "It causes incoming damage to be divided between your reserves "
               "of health and magic.";
    case AMU_FAITH:
        return "It allows you to gain divine favour quickly.";
    case AMU_REFLECTION:
        return "It reflects blocked missile attacks.";
#if TAG_MAJOR_VERSION == 34
    case AMU_INACCURACY:
        return "It reduces the accuracy of all your attacks.";
#endif
    }
    return "";
}

static const unordered_map<artefact_prop_type, property_descriptor, std::hash<int>> & _get_artp_desc_data_map()
{
    static bool init = false;
    static unordered_map<artefact_prop_type, property_descriptor, std::hash<int>> data_map;
    if (!init)
    {
        for (const auto &d : _get_all_artp_desc_data())
            data_map[d.property] = d;
        init = true;
    }
    return data_map;
}

static const property_descriptor & _get_artp_desc_data(artefact_prop_type p)
{
    const auto &data_map = _get_artp_desc_data_map();
    ASSERT(data_map.count(p));
    return data_map.at(p);
}

static const int MAX_ARTP_NAME_LEN = 10;

static string _padded_artp_name(artefact_prop_type prop, int val)
{
    // XX it would be nice to use a dynamic pad, but annoyingly, the constant
    // is used separately for egos.
    return make_stringf("%-*s", MAX_ARTP_NAME_LEN + 1,
        (_randart_prop_abbrev(prop, val) + ":").c_str());
}

void desc_randart_props(const item_def &item, vector<string> &lines)
{
    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_desc_properties(item, proprt, known);

    for (const property_descriptor &desc : _get_all_artp_desc_data())
    {
        if (!known_proprt(desc.property)) // can this ever happen..?
            continue;

        const int stval = proprt[desc.property];

        // these two have some custom string replacement
        if (desc.property == ARTP_WILLPOWER)
        {
            lines.push_back(make_stringf("%s It %s%s your willpower.",
                     _padded_artp_name(ARTP_WILLPOWER, stval).c_str(),
                     (stval < -1 || stval > 1) ? "greatly " : "",
                     (stval < 0) ? "decreases" : "increases"));
            continue;
        }
        else if (desc.property == ARTP_STEALTH)
        {
            lines.push_back(make_stringf("%s It makes you %s%s stealthy.",
                     _padded_artp_name(ARTP_STEALTH, stval).c_str(),
                     (stval < -1 || stval > 1) ? "much " : "",
                     (stval < 0) ? "less" : "more"));
            continue;
        }

        // otherwise, we use desc.desc in some form

        string sdesc = desc.desc;

        if (sdesc.find("%d") != string::npos)
            sdesc = replace_all(sdesc, "%d", make_stringf("%+d", stval));

        if (desc.display_type == prop_note::symbolic
            && desc.property != ARTP_REGENERATION // symbolic, but no text modification
            && desc.property != ARTP_MANA_REGENERATION)
        {
            // for symbolic props, desc.desc is just the resist name, need
            // to fill in to get a complete sentence
            const int idx = max(min(stval + 3, 6), 0);

            const char* prefixes[] =
            {
                "It makes you extremely vulnerable to ", // XX these two are worded badly? are they even used?
                "It makes you very vulnerable to ",
                "It makes you vulnerable to ",
                "Buggy descriptor!",
                "It protects you from ",
                "It greatly protects you from ",
                "It renders you almost immune to "
            };
            sdesc = prefixes[idx] + sdesc + '.';
        }

        lines.push_back(make_stringf("%s %s",
                                     _padded_artp_name(desc.property, stval).c_str(),
                                     sdesc.c_str()));
    }
}

static string _desc_randart_jewel(const item_def &item)
{
    // why are these specially hardcoded?
    const char* type = jewellery_base_ability_string(item.sub_type);
    const char* desc = _jewellery_base_ability_description(item.sub_type);
    if (!*desc)
        return "";
    if (!*type)
    {
        // if this case happens, the formatting can get mixed with
        // DBRANDS in weird ways
        return string(desc);
    }
    // XX a custom ego description isn't well handled here. The
    // main case of this is Vitality
    return make_stringf("%-*s %s", MAX_ARTP_NAME_LEN + 1,
                        (string(type) + ":").c_str(), desc);
}

static string _randart_descrip(const item_def &item)
{
    vector<string> lines;

    // Give a short description of the base type, for base types with no
    // corresponding ARTP.
    if (item.base_type == OBJ_JEWELLERY && item_ident(item, ISFLAG_KNOW_TYPE))
    {
        const string jewel_desc = _desc_randart_jewel(item);
        if (!jewel_desc.empty())
            lines.push_back(jewel_desc);
    }

    desc_randart_props(item, lines);
    return join_strings(lines.begin(), lines.end(), "\n");
}
#undef known_proprt

static string _format_dbrand(string dbrand)
{
    vector<string> out;

    vector<string> lines = split_string("\n", dbrand);
    for (const auto &l : lines)
    {
        vector<string> brand = split_string(":", l, true, false, 1);
        if (brand.size() == 0)
            continue;
        if (brand.size() == 1)
            out.push_back(brand[0]);
        else
        {
            ASSERT(brand.size() == 2);
            const string &desc = brand[1];
            const int prefix_len = max(MAX_ARTP_NAME_LEN, (int)brand[0].size());
            const string pre = padded_str(brand[0] + ":", prefix_len + 2);
                                                          // +2 for ": "
            out.push_back(pre + desc);
        }
    }
    return join_strings(out.begin(), out.end(), "\n");
}

// If item is an unrandart with a DESCRIP field, return its contents.
// Otherwise, return "".
static string _artefact_descrip(const item_def &item)
{
    if (!is_artefact(item))
        return "";

    ostringstream out;
    if (is_unrandom_artefact(item))
    {
        bool need_newline = false;
        auto entry = get_unrand_entry(item.unrand_idx);
        // Weapons have brands added earlier.
        if (entry->dbrand
            && item.base_type != OBJ_WEAPONS
            && item.base_type != OBJ_STAVES)
        {
            out << _format_dbrand(entry->dbrand);
            need_newline = true;
        }
        if (entry->descrip)
        {
            out << (need_newline ? "\n" : "") << _format_dbrand(entry->descrip);
            need_newline = true;
        }
        if (!_randart_descrip(item).empty())
            out << (need_newline ? "\n" : "") << _randart_descrip(item);
    }
    else
        out << _randart_descrip(item);

    // XXX: Can't happen, right?
    if (!item_ident(item, ISFLAG_KNOW_PROPERTIES) && item_type_known(item))
        out << "\nIt may have some hidden properties.";

    return out.str();
}

static const char *trap_names[] =
{
#if TAG_MAJOR_VERSION == 34
    "dart", "arrow", "spear",
#endif
#if TAG_MAJOR_VERSION > 34
    "dispersal",
    "teleport",
#endif
    "permanent teleport",
    "alarm",
#if TAG_MAJOR_VERSION == 34
    "blade", "bolt",
#endif
    "net",
    "Zot",
#if TAG_MAJOR_VERSION == 34
    "needle",
#endif
    "shaft",
    "passage",
    "pressure plate",
    "web",
#if TAG_MAJOR_VERSION == 34
    "gas", "teleport", "shadow", "dormant shadow", "dispersal"
#endif
};

string trap_name(trap_type trap)
{
    COMPILE_CHECK(ARRAYSZ(trap_names) == NUM_TRAPS);

    if (trap >= 0 && trap < NUM_TRAPS)
        return trap_names[trap];
    return "";
}

string full_trap_name(trap_type trap)
{
    string basename = trap_name(trap);
    switch (trap)
    {
    case TRAP_GOLUBRIA:
        return basename + " of Golubria";
    case TRAP_PLATE:
    case TRAP_WEB:
    case TRAP_SHAFT:
        return basename;
    default:
        return basename + " trap";
    }
}

int str_to_trap(const string &s)
{
    // "Zot trap" is capitalised in trap_names[], but the other trap
    // names aren't.
    const string tspec = lowercase_string(s);

    // allow a couple of synonyms
    if (tspec == "random" || tspec == "any")
        return TRAP_RANDOM;

    for (int i = 0; i < NUM_TRAPS; ++i)
        if (tspec == lowercase_string(trap_names[i]))
            return i;

    return -1;
}

/**
 * How should this panlord be described?
 *
 * @param name   The panlord's name; used as a seed for its appearance.
 * @param flying Whether the panlord can fly.
 * @returns a string including a description of its head, its body, its flight
 *          mode (if any), and how it smells or looks.
 */
static string _describe_demon(const string& name, bool flying, colour_t colour)
{
    const uint32_t seed = hash32(&name[0], name.size());
    #define HRANDOM_ELEMENT(arr, id) arr[hash_with_seed(ARRAYSZ(arr), seed, id)]

    static const char* body_types[] =
    {
        "armoured",
        "vast, spindly",
        "fat",
        "obese",
        "muscular",
        "spiked",
        "splotchy",
        "slender",
        "tentacled",
        "emaciated",
        "bug-like",
        "skeletal",
        "mantis",
        "slithering",
    };

    static const char* wing_names[] =
    {
        "with small, bat-like wings",
        "with bony wings",
        "with sharp, metallic wings",
        "with the wings of a moth",
        "with thin, membranous wings",
        "with dragonfly wings",
        "with large, powerful wings",
        "with fluttering wings",
        "with great, sinister wings",
        "with hideous, tattered wings",
        "with sparrow-like wings",
        "with hooked wings",
        "with strange knobs attached",
        "which hovers in mid-air",
        "with sacs of gas hanging from its back",
    };

    const char* head_names[] =
    {
        "a cubic structure in place of a head",
        "a brain for a head",
        "a hideous tangle of tentacles for a mouth",
        "the head of an elephant",
        "an eyeball for a head",
        "wears a helmet over its head",
        "a horn in place of a head",
        "a thick, horned head",
        "the head of a horse",
        "a vicious glare",
        "snakes for hair",
        "the face of a baboon",
        "the head of a mouse",
        "a ram's head",
        "the head of a rhino",
        "eerily human features",
        "a gigantic mouth",
        "a mass of tentacles growing from its neck",
        "a thin, worm-like head",
        "huge, compound eyes",
        "the head of a frog",
        "an insectoid head",
        "a great mass of hair",
        "a skull for a head",
        "a cow's skull for a head",
        "the head of a bird",
        "a large fungus growing from its neck",
        "an ominous eye at the end of a thin stalk",
        "a face from nightmares",
    };

    static const char* misc_descs[] =
    {
        " It seethes with hatred of the living.",
        " Tiny orange flames dance around it.",
        " Tiny purple flames dance around it.",
        " It is surrounded by a weird haze.",
        " It glows with a malevolent light.",
        " It looks incredibly angry.",
        " It oozes with slime.",
        " It dribbles constantly.",
        " Mould grows all over it.",
        " Its body is covered in fungus.",
        " It is covered with lank hair.",
        " It looks diseased.",
        " It looks as frightened of you as you are of it.",
        " It moves in a series of hideous convulsions.",
        " It moves with an unearthly grace.",
        " It leaves a glistening oily trail.",
        " It shimmers before your eyes.",
        " It is surrounded by a brilliant glow.",
        " It radiates an aura of extreme power.",
        " It seems utterly heartbroken.",
        " It seems filled with irrepressible glee.",
        " It constantly shivers and twitches.",
        " Blue sparks crawl across its body.",
        " It seems uncertain.",
        " A cloud of flies swarms around it.",
        " The air around it ripples with heat.",
        " Crystalline structures grow on everything near it.",
        " It appears supremely confident.",
        " Its skin is covered in a network of cracks.",
        " Its skin has a disgusting oily sheen.",
        " It seems somehow familiar.",
        " It is somehow always in shadow.",
        " It is difficult to look away.",
        " It is constantly speaking in tongues.",
        " It babbles unendingly.",
        " Its body is scourged by damnation.",
        " Its body is extensively scarred.",
        " You find it difficult to look away.",
        " Oddly mechanical noises accompany its jarring movements.",
        " Its skin looks unnervingly wrinkled.",
    };

    static const char* smell_descs[] =
    {
        " It smells of brimstone.",
        " It is surrounded by a sickening stench.",
        " It smells of rotting flesh.",
        " It stinks of death.",
        " It stinks of decay.",
        " It smells delicious!",
    };

    ostringstream description;
    description << "One of the many lords of Pandemonium, " << name << " has ";

    description << article_a(HRANDOM_ELEMENT(body_types, 2));
    // ETC_RANDOM is also possible, handled later
    if (colour >= 0 && colour < NUM_TERM_COLOURS)
        description << " " << colour_to_str(colour, true);
    description << " body ";

    if (flying)
    {
        description << HRANDOM_ELEMENT(wing_names, 3);
        description << " ";
    }

    description << "and ";
    description << HRANDOM_ELEMENT(head_names, 1) << ".";

    if (!hash_with_seed(5, seed, 4) && you.can_smell()) // 20%
        description << HRANDOM_ELEMENT(smell_descs, 5);

    if (hash_with_seed(2, seed, 6)) // 50%
        description << HRANDOM_ELEMENT(misc_descs, 6);

    if (colour == ETC_RANDOM)
        description << " It changes colour whenever you look at it.";

    return description.str();
}

/**
 * Describe a given mutant beast's tier.
 *
 * @param tier      The mutant_beast_tier of the beast in question.
 * @return          A string describing the tier; e.g.
 *              "It is a juvenile, out of the larval stage but still below its
 *              mature strength."
 */
static string _describe_mutant_beast_tier(int tier)
{
    static const string tier_descs[] = {
        "It is of an unusually buggy age.",
        "It is larval and weak, freshly emerged from its mother's pouch.",
        "It is a juvenile, no longer larval but below its mature strength.",
        "It is mature, stronger than a juvenile but weaker than its elders.",
        "It is an elder, stronger than mature beasts.",
        "It is a primal beast, the most powerful of its kind.",
    };
    COMPILE_CHECK(ARRAYSZ(tier_descs) == NUM_BEAST_TIERS);

    ASSERT_RANGE(tier, 0, NUM_BEAST_TIERS);
    return tier_descs[tier];
}


/**
 * Describe a given mutant beast's facets.
 *
 * @param facets    A vector of the mutant_beast_facets in question.
 * @return          A string describing the facets; e.g.
 *              "It flies and flits around unpredictably, and its breath
 *               smoulders ominously."
 */
static string _describe_mutant_beast_facets(const CrawlVector &facets)
{
    static const string facet_descs[] = {
        " seems unusually buggy.",
        " sports a set of venomous tails",
        " flies swiftly and unpredictably",
        "s breath smoulders ominously",
        " is covered with eyes and tentacles",
        " flickers and crackles with electricity",
        " is covered in dense fur and muscle",
    };
    COMPILE_CHECK(ARRAYSZ(facet_descs) == NUM_BEAST_FACETS);

    if (facets.size() == 0)
        return "";

    return "It" + comma_separated_fn(begin(facets), end(facets),
                      [] (const CrawlStoreValue &sv) -> string {
                          const int facet = sv.get_int();
                          ASSERT_RANGE(facet, 0, NUM_BEAST_FACETS);
                          return facet_descs[facet];
                      }, ", and it", ", it")
           + ".";

}

/**
 * Describe a given mutant beast's special characteristics: its tier & facets.
 *
 * @param mi    The player-visible information about the monster in question.
 * @return      A string describing the monster; e.g.
 *              "It is a juvenile, out of the larval stage but still below its
 *              mature strength. It flies and flits around unpredictably, and
 *              its breath has a tendency to ignite when angered."
 */
static string _describe_mutant_beast(const monster_info &mi)
{
    const int xl = mi.props[MUTANT_BEAST_TIER].get_short();
    const int tier = mutant_beast_tier(xl);
    const CrawlVector facets = mi.props[MUTANT_BEAST_FACETS].get_vector();
    return _describe_mutant_beast_facets(facets)
           + " " + _describe_mutant_beast_tier(tier);
}

/**
 * Is the item associated with some specific training goal?  (E.g. mindelay)
 *
 * @return the goal, or 0 if there is none, scaled by 10.
 */
static int _item_training_target(const item_def &item)
{
    const int throw_dam = property(item, PWPN_DAMAGE);
    if (item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES)
        return weapon_min_delay_skill(item) * 10;
    if (item.base_type == OBJ_MISSILES && is_throwable(&you, item))
        return (((10 + throw_dam / 2) - FASTEST_PLAYER_THROWING_SPEED) * 2) * 10;
    if (item.base_type == OBJ_TALISMANS)
        return get_form(form_for_talisman(item))->min_skill * 10;
    return 0;
}

/**
 * Does an item improve with training some skill?
 *
 * @return the skill, or SK_NONE if there is none. Note: SK_NONE is *not* 0.
 */
static skill_type _item_training_skill(const item_def &item)
{
    if (item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES)
        return item_attack_skill(item);
    if (is_shield(item))
        return SK_SHIELDS; // shields are armour, so do shields before armour
    if (item.base_type == OBJ_ARMOUR)
        return SK_ARMOUR;
    if (item.base_type == OBJ_MISSILES && is_throwable(&you, item))
        return SK_THROWING;
    if (item.base_type == OBJ_TALISMANS)
        return SK_SHAPESHIFTING;
    if (item_ever_evokable(item)) // not very accurate
        return SK_EVOCATIONS;
    return SK_NONE;
}

/**
 * Return whether the character is below a plausible training target for an
 * item.
 *
 * @param item the item to check.
 * @param ignore_current if false, return false if there already is a higher
 *        target set for this skill.
 */
static bool _is_below_training_target(const item_def &item, bool ignore_current)
{
    if (!crawl_state.need_save || is_useless_item(item))
        return false;

    const skill_type skill = _item_training_skill(item);
    if (skill == SK_NONE)
        return false;

    const int target = min(_item_training_target(item), 270);

    return target && !is_useless_skill(skill)
       && you.skill(skill, 10, false, false) < target
       && (ignore_current || you.get_training_target(skill) < target);
}


/**
 * Produce the "Your skill:" line for item descriptions where specific skill targets
 * are relevant (weapons, missiles)
 *
 * @param skill the skill to look at.
 * @param show_target_button whether to show the button for setting a skill target.
 * @param scaled_target a target, scaled by 10, to use when describing the button.
 */
static string _your_skill_desc(skill_type skill, bool show_target_button,
                               int scaled_target, string padding = "")
{
    if (!crawl_state.need_save || skill == SK_NONE)
        return "";
    string target_button_desc = "";
    int min_scaled_target = min(scaled_target, 270);
    if (show_target_button &&
            you.get_training_target(skill) < min_scaled_target)
    {
        target_button_desc = make_stringf(
            "; use <white>(s)</white> to set %d.%d as a target for %s.",
                                min_scaled_target / 10, min_scaled_target % 10,
                                skill_name(skill));
    }
    int you_skill_temp = you.skill(skill, 10);
    int you_skill = you.skill(skill, 10, false, false);

    return make_stringf("Your %sskill: %s%d.%d%s",
                            (you_skill_temp != you_skill ? "(base) " : ""),
                            padding.c_str(), you_skill / 10, you_skill % 10,
                            target_button_desc.c_str());
}

/**
 * Produce a description of a skill target for items where specific targets are
 * relevant.
 *
 * @param skill the skill to look at.
 * @param scaled_target a skill level target, scaled by 10.
 * @param training a training value, from 0 to 100. Need not be the actual training
 * value.
 */
static string _skill_target_desc(skill_type skill, int scaled_target,
                                        unsigned int training)
{
    string description = "";
    scaled_target = min(scaled_target, 270);

    const bool max_training = (training == 100);
    const bool hypothetical = !crawl_state.need_save ||
                                    (training != you.training[skill]);

    const skill_diff diffs = skill_level_to_diffs(skill,
                                (double) scaled_target / 10, training, false);
    const int level_diff = xp_to_level_diff(diffs.experience / 10, 10);

    if (max_training)
        description += "At 100% training ";
    else if (!hypothetical)
    {
        description += make_stringf("At current training (%d%%) ",
                                        you.training[skill]);
    }
    else
        description += make_stringf("At a training level of %d%% ", training);

    description += make_stringf(
        "you %sreach %d.%d in %s %d.%d XLs.",
            hypothetical ? "would " : "",
            scaled_target / 10, scaled_target % 10,
            (you.experience_level + (level_diff + 9) / 10) > 27
                                ? "the equivalent of" : "about",
            level_diff / 10, level_diff % 10);
    if (you.wizard)
    {
        description += make_stringf("\n    (%d xp, %d skp)",
                                    diffs.experience, diffs.skill_points);
    }
    return description;
}

/**
 * Append two skill target descriptions: one for 100%, and one for the
 * current training rate.
 */
static void _append_skill_target_desc(string &description, skill_type skill,
                                        int scaled_target)
{
    if (!you.has_mutation(MUT_DISTRIBUTED_TRAINING))
        description += "\n    " + _skill_target_desc(skill, scaled_target, 100);
    if (you.training[skill] > 0 && you.training[skill] < 100)
    {
        description += "\n    " + _skill_target_desc(skill, scaled_target,
                                                    you.training[skill]);
    }
}

static int _get_delay(const item_def &item)
{
    if (!is_range_weapon(item))
        return you.attack_delay_with(nullptr, false, &item).expected();
    item_def fake_proj;
    populate_fake_projectile(item, fake_proj);
    return you.attack_delay_with(&fake_proj, false, &item).expected();
}

static string _desc_attack_delay(const item_def &item)
{
    const int base_delay = property(item, PWPN_SPEED);
    const int cur_delay = _get_delay(get_item_known_info(item));
    if (weapon_adjust_delay(item, base_delay, false) == cur_delay)
        return "";
    return make_stringf("\n    Current attack delay: %.1f.", (float)cur_delay / 10);
}

static string _describe_missile_brand(const item_def &item)
{
    switch (item.brand)
    {
    case SPMSL_POISONED:
    case SPMSL_CURARE:
    case SPMSL_SILVER:
    case SPMSL_CHAOS:
        break;
    default: // Other brands don't deal extra damage.
        return "";
    }
     const string brand_name = missile_brand_name(item, MBN_BRAND);

     if (brand_name.empty())
         return brand_name;

     return " + " + uppercase_first(brand_name);
}

string damage_rating(const item_def *item, int *rating_value)
{
    if (item && is_unrandom_artefact(*item, UNRAND_WOE))
    {
        if (rating_value)
            *rating_value = 666;

        return "your enemies will bleed and die for Makhleb.";
    }

    const bool thrown = item && item->base_type == OBJ_MISSILES;
    if (item && !thrown && !is_weapon(*item))
        return "0.";

    brand_type brand = SPWPN_NORMAL;
    if (!item)
        brand = get_form()->get_uc_brand();
    else if (item_type_known(*item) && !thrown)
        brand = get_weapon_brand(*item);

    // Would be great to have a breakdown of UC damage by skill, form, claws etc.
    const int base_dam = item ? property(*item, PWPN_DAMAGE)
                              : unarmed_base_damage(false);
    // This is just SPWPN_HEAVY.
    const int post_brand_dam = brand_adjust_weapon_damage(base_dam, brand, false);
    const int heavy_dam = post_brand_dam - base_dam;
    const int extra_base_dam = thrown ? throwing_base_damage_bonus(*item) :
                               !item ? unarmed_base_damage_bonus(false) :
                                    heavy_dam; // 0 for non-heavy weapons
    const skill_type skill = item ? _item_training_skill(*item) : SK_UNARMED_COMBAT;
    const int stat_mult = stat_modify_damage(100, skill, true);
    const bool use_str = weapon_uses_strength(skill, true);
    // Throwing weapons and UC only get a damage mult from Fighting skill,
    // not from Throwing/UC skill.
    const bool use_weapon_skill = item && !thrown;
    const int weapon_skill_mult = use_weapon_skill ? apply_weapon_skill(100, skill, false) : 100;
    const int skill_mult = apply_fighting_skill(weapon_skill_mult, false, false);

    const int slaying = slaying_bonus(thrown, false);
    const int ench = item && item_ident(*item, ISFLAG_KNOW_PLUSES) ? item->plus : 0;
    const int plusses = slaying + ench;

    const int DAM_RATE_SCALE = 100;
    int rating = (base_dam + extra_base_dam) * DAM_RATE_SCALE;
    rating = stat_modify_damage(rating, skill, true);
    if (use_weapon_skill)
        rating = apply_weapon_skill(rating, skill, false);
    rating = apply_fighting_skill(rating, false, false);
    rating /= DAM_RATE_SCALE;
    rating += plusses;

    if (rating_value)
        *rating_value = rating;

    const string base_dam_desc = thrown ? make_stringf("[%d + %d (Thrw)]",
                                                       base_dam, extra_base_dam) :
                                  !item ? make_stringf("[%d + %d (UC)]",
                                                       base_dam, extra_base_dam) :
                   brand == SPWPN_HEAVY ? make_stringf("[%d + %d (Hvy)]",
                                                       base_dam, extra_base_dam) :
                                          make_stringf("%d", base_dam);

    string plusses_desc;
    if (plusses)
    {
        plusses_desc = make_stringf(" %s %d (%s)",
                                    plusses < 0 ? "-" : "+",
                                    abs(plusses),
                                    slaying && ench ? "Ench + Slay" :
                                               ench ? "Ench"
                                                    : "Slay");
    }

    const string brand_desc = thrown ? _describe_missile_brand(*item) : "";

    return make_stringf(
        "%d (Base %s x %d%% (%s) x %d%% (%s)%s)%s.",
        rating,
        base_dam_desc.c_str(),
        stat_mult,
        use_str ? "Str" : "Dex",
        skill_mult,
        use_weapon_skill ? "Skill" : "Fight",
        plusses_desc.c_str(),
        brand_desc.c_str());
}

static void _append_weapon_stats(string &description, const item_def &item)
{
    const int base_dam = property(item, PWPN_DAMAGE);
    const skill_type skill = _item_training_skill(item);
    const int mindelay_skill = _item_training_target(item);

    const bool below_target = _is_below_training_target(item, true);
    const bool can_set_target = below_target
        && in_inventory(item) && !you.has_mutation(MUT_DISTRIBUTED_TRAINING);

    if (item.base_type == OBJ_STAVES
        && item_type_known(item)
        && staff_skill(static_cast<stave_type>(item.sub_type)) != SK_NONE
        && is_useless_skill(staff_skill(static_cast<stave_type>(item.sub_type))))
    {
        description += make_stringf(
            "Your inability to study %s prevents you from drawing on the"
            " full power of this staff in melee.\n\n",
            skill_name(staff_skill(static_cast<stave_type>(item.sub_type))));
    }

    if (is_unrandom_artefact(item, UNRAND_WOE))
    {
        const char *inf = Options.char_set == CSET_ASCII ? "inf" : "\u221e"; //"∞"
        description += make_stringf(
            "Base accuracy: %s  Base damage: %s  ",
            inf,
            inf);
    }
    else
    {
        description += make_stringf(
            "Base accuracy: %+d  Base damage: %d  ",
            property(item, PWPN_HIT),
            base_dam);
    }

    description += make_stringf(
        "Base attack delay: %.1f\n"
        "This weapon's minimum attack delay (%.1f) is reached at skill level %d.",
            (float) property(item, PWPN_SPEED) / 10,
            (float) weapon_min_delay(item, item_brand_known(item)) / 10,
            mindelay_skill / 10);

    const bool want_player_stats = !is_useless_item(item) && crawl_state.need_save;
    if (want_player_stats)
    {
        description += "\n    "
            + _your_skill_desc(skill, can_set_target, mindelay_skill);
    }

    if (below_target)
        _append_skill_target_desc(description, skill, mindelay_skill);

    if (is_slowed_by_armour(&item))
    {
        const int penalty_scale = 100;
        const int armour_penalty = you.adjusted_body_armour_penalty(penalty_scale);
        description += "\n";
        if (armour_penalty)
        {
            const item_def *body_armour = you.slot_item(EQ_BODY_ARMOUR, false);
            description += (body_armour ? uppercase_first(
                                              body_armour->name(DESC_YOUR))
                                        : "Your heavy armour");

            const bool significant = armour_penalty >= penalty_scale;
            if (significant)
            {
                description +=
                    make_stringf(" slows your attacks with this weapon by %.1f",
                                 armour_penalty / (10.0f * penalty_scale));
            }
            else
                description += " slightly slows your attacks with this weapon";
        }
        else
        {
            description += "Wearing heavy armour would reduce your attack "
                           "speed with this weapon";
        }
        description += ".";
    }

    if (want_player_stats)
    {
        description += _desc_attack_delay(item);
        description += "\nDamage rating: " + damage_rating(&item);
    }

    const string brand_desc = _describe_weapon_brand(item);
    if (!brand_desc.empty())
    {
        const brand_type brand = get_weapon_brand(item);
        string brand_name = uppercase_first(brand_type_name(brand, true));
        // Hack to match artefact prop formatting.
        description += make_stringf("\n\n%-*s %s",
                                    MAX_ARTP_NAME_LEN + 1,
                                    (brand_name + ":").c_str(),
                                    brand_desc.c_str());
    }
    if (is_unrandom_artefact(item))
    {
        auto entry = get_unrand_entry(item.unrand_idx);
        if (entry->dbrand)
        {
            description += make_stringf("\n%s%s",
                (brand_desc.empty() ? "\n" : ""),
                _format_dbrand(entry->dbrand).c_str());
        }
        // XX spacing following brand and dbrand for randarts/unrands is a bit
        // inconsistent with other object types
    }
}

static string _handedness_string(const item_def &item)
{
    const bool quad = you.has_mutation(MUT_QUADRUMANOUS);
    string handname = species::hand_name(you.species);
    if (quad)
        handname += "-pair";

    string n;
    switch (you.hands_reqd(item))
    {
    case HANDS_ONE:
        n = "one";
        break;
    case HANDS_TWO:
        if (quad)
            handname = pluralise(handname);
        n = "two";
        break;
    }

    if (quad)
        return make_stringf("It is a weapon for %s %s.", n.c_str(), handname.c_str());
    else
    {
        return make_stringf("It is a %s-%s%s weapon.", n.c_str(),
            handname.c_str(),
            ends_with(handname, "e") ? "d" : "ed");
    }

}

static string _category_string(const item_def &item, bool monster)
{
    if (is_unrandom_artefact(item, UNRAND_LOCHABER_AXE))
        return ""; // handled in art-data DBRAND

    string description = "";
    description += "This ";
    if (is_unrandom_artefact(item))
        description += get_artefact_base_name(item);
    else
        description += "weapon";
    description += " falls into the";

    const skill_type skill = item_attack_skill(item);

    description +=
        make_stringf(" '%s' category. ",
                     skill == SK_FIGHTING ? "buggy" : skill_name(skill));

    switch (item_attack_skill(item))
    {
    case SK_POLEARMS:
        // TODO(PF): maybe remove this whole section for util/monster summaries..?
        description += "It has an extended reach";
        if (!monster)
            description += " (target with [<white>v</white>])";
        description += ". ";
        break;
    case SK_AXES:
        description += "It hits all enemies adjacent to the wielder";
        if (!is_unrandom_artefact(item, UNRAND_WOE))
            description += ", dealing less damage to those not targeted";
        description += ". ";
        break;
    case SK_SHORT_BLADES:
        {
            description += make_stringf(
                "It is%s good for stabbing helpless or unaware enemies. ",
                (item.sub_type == WPN_DAGGER) ? " extremely" : "");

        }
        break;
    default:
        break;
    }

    return description;
}

static string _ghost_brand_extra_info(brand_type brand)
{
    switch (brand)
    {
    case SPWPN_FLAMING:
    case SPWPN_FREEZING:      return "+1/4 damage after AC";
    case SPWPN_HOLY_WRATH:    return "+3/4 damage vs evil after AC"; // ish
    case SPWPN_ELECTROCUTION: return "1/4 chance of 8-20 damage";
    case SPWPN_ACID:          return "2d4 damage, corrosion";
    // Would be nice to show Pain/Foul Flame damage and chance
    default: return "";
    }
}

static string _desc_ghost_brand(brand_type brand)
{
    const string base_name = uppercase_first(brand_type_name(brand, true));
    const string extra_info = _ghost_brand_extra_info(brand);
    if (extra_info.empty())
        return base_name;
    return make_stringf("%s (%s)", base_name.c_str(), extra_info.c_str());
}

static string _describe_weapon_brand(const item_def &item)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);
        if (entry && entry->flags & UNRAND_FLAG_SKIP_EGO)
            return ""; // XXX FIXME
    }

    if (!item_type_known(item))
        return "";

    const brand_type brand = get_weapon_brand(item);
    const bool ranged = is_range_weapon(item);

    switch (brand)
    {
    case SPWPN_FLAMING:
    {
        const int damtype = get_vorpal_type(item);
        const string desc = "It burns victims, dealing an additional "
                            "one-quarter of any damage that pierces defenders'"
                            " armour.";
        if (ranged || damtype != DVORP_SLICING && damtype != DVORP_CHOPPING)
            return desc;
        return desc +
            " Big, fiery blades are also staple armaments of hydra-hunters.";
    }
    case SPWPN_FREEZING:
        return "It freezes victims, dealing an additional one-quarter of any "
               "damage that pierces defenders' armour. It may also slow down "
               "cold-blooded creatures.";
    case SPWPN_HOLY_WRATH:
        return "It has been blessed by the Shining One, dealing an additional "
               "three-quarters of any damage that pierces undead and demons' "
               "armour. Undead and demons cannot use this.";
    case SPWPN_FOUL_FLAME:
        return "It has been infused with foul flame, dealing an additional "
               "three-quarters damage to holy beings, an additional "
               "one-quarter damage to undead and demons, and an additional "
               "half damage to all others, so long as it pierces armour. "
               "Holy beings and good god worshippers cannot use this.";
    case SPWPN_ELECTROCUTION:
        return "It sometimes electrocutes victims (1/4 chance, 8-20 damage).";
    case SPWPN_VENOM:
        return "It poisons victims.";
    case SPWPN_PROTECTION:
        return "It grants its wielder temporary protection after it strikes "
               "(+7 AC).";
    case SPWPN_DRAINING:
        return "It sometimes drains living victims (1/2 chance). This deals "
               "an additional one-quarter of any damage that pierces "
               "defenders' armour as well as a flat 2-4 damage, and also "
               "weakens them slightly.";
    case SPWPN_SPEED:
        return "Attacks with this weapon are significantly faster.";
    case SPWPN_HEAVY:
    {
        string desc = ranged ? "Any ammunition fired from it" : "It";
        return desc + " deals dramatically more damage, but attacks with "
                      "it are much slower.";
    }
    case SPWPN_CHAOS:
        return "Each hit has a different, random effect.";
    case SPWPN_VAMPIRISM:
        return "It occasionally heals its wielder for a portion "
               "of the damage dealt when it wounds a living foe.";
    case SPWPN_PAIN:
        {
            string desc = "In the hands of one skilled in necromantic "
                 "magic, it inflicts extra damage on living creatures.";
            if (you_worship(GOD_TROG))
                return desc + " Trog prevents you from unleashing this effect.";
            if (!is_useless_skill(SK_NECROMANCY))
                return desc;
            return desc + " Your inability to study Necromancy prevents "
                     "you from drawing on the full power of this weapon.";
        }
    case SPWPN_DISTORTION:
        return "It warps and distorts space around it, and may blink, banish, "
               "or inflict extra damage upon those it strikes. Unwielding it "
               "can teleport you to foes or banish you to the Abyss.";
    case SPWPN_PENETRATION:
        return "Any ammunition fired by it continues flying after striking "
               "targets, potentially hitting everything in its path until it "
               "leaves sight.";
    case SPWPN_REAPING:
        return "Any living foe damaged by it may be reanimated upon death as a "
               "zombie friendly to the wielder, with an increasing chance as "
               "more damage is dealt.";
    case SPWPN_ANTIMAGIC:
        return "It reduces the magical energy of the wielder, and disrupts "
               "the spells and magical abilities of those it strikes. Natural "
               "abilities and divine invocations are not affected.";
    case SPWPN_SPECTRAL:
        return "When its wielder attacks, the weapon's spirit leaps out and "
               "launches a second, slightly weaker strike. The spirit shares "
               "part of any damage it takes with its wielder.";
    case SPWPN_ACID:
        return "It splashes victims with acid (2d4 damage, Corrosion).";
    default:
        return "";
    }
}

static string _describe_point_change(float points)
{
    string point_diff_description;

    point_diff_description += make_stringf("%s by %.1f",
                                           points > 0 ? "increase" : "decrease",
                                           abs(points));

    return point_diff_description;
}

static string _describe_point_diff(int original,
                                   int changed, int scale = 100)
{
    string description;

    if (original == changed)
        return "remain unchanged";

    float difference = ((float)(changed - original)) / scale;

    description += _describe_point_change(difference);
    description += " (";
    description += make_stringf("%.1f", ((float)(original)) / scale);
    description += " -> ";
    description += make_stringf("%.1f", ((float)(changed)) / scale);
    description += ")";

    return description;
}

static string _equip_type_name(const item_def &item)
{
    if (item.base_type == OBJ_JEWELLERY)
    {
        if (jewellery_is_amulet(item))
            return "amulet";
        else
            return "ring";
    }

    return base_type_string(item.base_type);
}

static string _equipment_switchto_string(const item_def &item)
{
    if (item.base_type == OBJ_WEAPONS)
        return "wielding";
    // Not always the same verb used elsewhere, but "switch putting on" sounds weird
    else
        return "wearing";
}

/**
 * Describe how (un)equipping a piece of equipment might change the player's
 * AC/EV/SH stats. We don't include temporary buffs in this calculation.
 *
 * @param item    The item whose description we are writing.
 * @param remove  Whether the item is already equipped, and thus whether to
                  show the change solely from unequipping the item.
 * @return        The description.
 */
static string _equipment_ac_ev_sh_change_description(const item_def &item,
                                                     bool remove = false)
{
    // First, test if there is any AC/EV/SH change at all.
    const int cur_ac = you.base_ac(100);
    const int cur_ev = you.evasion_scaled(100, true);
    const int cur_sh = player_displayed_shield_class(100, true);
    int new_ac, new_ev, new_sh;

    if (remove)
        you.ac_ev_sh_without_specific_item(100, item, &new_ac, &new_ev, &new_sh);
    else
        you.ac_ev_sh_with_specific_item(100, item, &new_ac, &new_ev, &new_sh);

    // If we're previewing non-armour and there is no AC/EV/SH change, print no
    // extra description at all (since almost all items of these types will
    // change nothing)
    if (cur_ac == new_ac && cur_ev == new_ev && cur_sh == new_sh
        && (item.base_type != OBJ_ARMOUR || item.sub_type == ARM_ORB))
    {
        return "";
    }

    string description;
    description.reserve(100);
    description = "\n\n";

    if (remove)
    {
        description += "If you " + item_unequip_verb(item) + " this "
                        + _equip_type_name(item) + ":";
    }
    else if (item.base_type == OBJ_JEWELLERY && !jewellery_is_amulet(item))
        description += "If you were wearing this ring:";
    else if (item.base_type == OBJ_WEAPONS && you.has_mutation(MUT_WIELD_OFFHAND))
        description += "If you switch to wielding this weapon in your main hand:";
    else
    {
        description += "If you switch to " + _equipment_switchto_string(item)
                         + " this " + _equip_type_name(item) + ":";
    }

    // Always display AC line on proper armour, even if there is no change
    if (item.base_type == OBJ_ARMOUR && get_armour_slot(item) != EQ_OFFHAND
        || cur_ac != new_ac)
    {
        description += "\nYour AC would "
                       + _describe_point_diff(cur_ac, new_ac) + ".";
    }

    // Always display EV line on non-orb armour, even if there is no change
    // XXX perhaps this shouldn't display on basic aux armour?
    if (item.base_type == OBJ_ARMOUR && item.sub_type != ARM_ORB
        || cur_ev != new_ev)
    {
        description += "\nYour EV would "
                       + _describe_point_diff(cur_ev, new_ev) + ".";
    }

    // Always display SH line on shields, even if there is no change
    if (is_shield(item) || cur_sh != new_sh)
    {
        description += "\nYour SH would "
                       + _describe_point_diff(cur_sh, new_sh) + ".";
    }

    return description;
}

static bool _you_are_wearing_item(const item_def &item)
{
    return get_equip_slot(&item) != EQ_NONE;
}

static string _equipment_ac_ev_sh_change(const item_def &item)
{
    string description;

    if (!_you_are_wearing_item(item))
        description = _equipment_ac_ev_sh_change_description(item);
    else
        description = _equipment_ac_ev_sh_change_description(item, true);

    return description;
}

static string _describe_weapon(const item_def &item, bool verbose, bool monster)
{
    string description;

    description.reserve(200);

    description = "";

    if (verbose)
    {
        if (!monster)
            description += "\n\n";
        _append_weapon_stats(description, item);
    }

    // ident known & no brand but still glowing
    // TODO: deduplicate this with the code in item-name.cc
    if (verbose
        && item_type_known(item)
        && get_weapon_brand(item) == SPWPN_NORMAL
        && get_equip_desc(item)
        && !item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        description += "\n\nIt has no special brand (it is not flaming, "
            "freezing, etc), but is still enchanted in some way - "
            "positive or negative.";
     }

    string art_desc = _artefact_descrip(item);
    if (!art_desc.empty())
        description += "\n\n" + art_desc;

    if (verbose && crawl_state.need_save && you.could_wield(item, true, true))
        description += _equipment_ac_ev_sh_change(item);

    if (verbose)
    {
        description += "\n\n" + _category_string(item, monster);



        // XX this is shown for felids, does that actually make sense?
        description += _handedness_string(item);

        if (!you.could_wield(item, true, true) && crawl_state.need_save)
            description += "\nIt is too large for you to wield.";
    }

    if (!is_artefact(item) && !monster)
    {
        if (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus >= MAX_WPN_ENCHANT)
            description += "\nIt cannot be enchanted further.";
        else
        {
            description += "\nIt can be maximally enchanted to +"
                           + to_string(MAX_WPN_ENCHANT) + ".";
        }
    }

    return description;
}

static string _describe_ammo(const item_def &item)
{
    string description;

    description.reserve(64);

    if (item.brand && item_type_known(item))
    {
        description += "\n\n";
        switch (item.brand)
        {
#if TAG_MAJOR_VERSION == 34
        case SPMSL_FLAME:
            description += "It burns those it strikes, causing extra injury "
                    "to most foes and up to half again as much damage against "
                    "particularly susceptible opponents. Compared to normal "
                    "ammo, it is twice as likely to be destroyed on impact.";
            break;
        case SPMSL_FROST:
            description += "It freezes those it strikes, causing extra injury "
                    "to most foes and up to half again as much damage against "
                    "particularly susceptible opponents. It can also slow down "
                    "cold-blooded creatures. Compared to normal ammo, it is "
                    "twice as likely to be destroyed on impact.";
            break;
#endif
        case SPMSL_CHAOS:
            description += "When thrown, it has a random effect.";
            break;
        case SPMSL_POISONED:
            description += "It is coated with poison.";
            break;
        case SPMSL_CURARE:
            description += "It is tipped with a substance that causes "
                           "asphyxiation, dealing direct damage as well as "
                           "poisoning and slowing those it strikes.\n\n"
                           "It is twice as likely to be destroyed on impact as "
                           "other darts.";
            break;
        case SPMSL_FRENZY:
            description += "It is tipped with a substance that sends those it "
                           "hits into a mindless frenzy, attacking friend and "
                           "foe alike.\n\n"
                           "The chance of successfully applying its effect "
                           "increases with Throwing and Stealth skill.";

            break;
        case SPMSL_BLINDING:
            description += "It is tipped with a substance that causes "
                           "blindness and brief confusion.\n\n"
                           "The chance of successfully applying its effect "
                           "increases with Throwing and Stealth skill.";
            break;
        case SPMSL_DISPERSAL:
            description += "It causes any target it hits to blink, with a "
                           "tendency towards blinking further away from the "
                           "one who threw it.";
            break;
        case SPMSL_SILVER:
            description += "It deals increased damage compared to normal ammo "
                           "and substantially increased damage to chaotic "
                           "and magically transformed beings. It also inflicts "
                           "extra damage against mutated beings, according to "
                           "how mutated they are.";
            break;
        }
    }

    const int dam = property(item, PWPN_DAMAGE);
    const bool player_throwable = is_throwable(&you, item);
    if (player_throwable)
    {
        const int throw_delay = (10 + dam / 2);
        const int target_skill = _item_training_target(item);

        const bool below_target = _is_below_training_target(item, true);
        const bool can_set_target = below_target && in_inventory(item)
            && !you.has_mutation(MUT_DISTRIBUTED_TRAINING);

        description += make_stringf(
            "\n\nBase damage: %d  Base attack delay: %.1f"
            "\nThis projectile's minimum attack delay (%.1f) "
                "is reached at skill level %d.",
            dam,
            (float) throw_delay / 10,
            (float) FASTEST_PLAYER_THROWING_SPEED / 10,
            target_skill / 10
        );

        if (!is_useless_item(item))
        {
            description += "\n    " +
                    _your_skill_desc(SK_THROWING, can_set_target, target_skill);
        }
        if (below_target)
            _append_skill_target_desc(description, SK_THROWING, target_skill);

        if (!is_useless_item(item) && property(item, PWPN_DAMAGE))
            description += "\nDamage rating: " + damage_rating(&item);
    }

    if (ammo_always_destroyed(item))
        description += "\n\nIt is always destroyed on impact.";
    else if (!ammo_never_destroyed(item))
        description += "\n\nIt may be destroyed on impact.";

    return description;
}

static string _warlock_mirror_reflect_desc()
{
    const int scaled_SH = crawl_state.need_save ? player_shield_class(100, false) : 0;
    const int SH = scaled_SH / 100;
    // We use random-rounded SH, so take a weighted average of the
    // chances with SH and SH+1.
    const int reflect_chance_numer = (100 - (scaled_SH % 100)) * SH * omnireflect_chance_denom(SH+1) + (scaled_SH % 100) * (SH+1) * omnireflect_chance_denom(SH);
    const int reflect_chance_denom = omnireflect_chance_denom(SH) * omnireflect_chance_denom(SH+1);
    const int reflect_chance = reflect_chance_numer / reflect_chance_denom;
    return "\n\nWith your current SH, it has a " + to_string(reflect_chance) +
           "% chance to reflect attacks against your willpower and other "
           "normally unblockable effects.";
}

static const char* _item_ego_desc(special_armour_type ego)
{
    switch (ego)
    {
    case SPARM_FIRE_RESISTANCE:
        return "it protects its wearer from fire.";
    case SPARM_COLD_RESISTANCE:
        return "it protects its wearer from cold.";
    case SPARM_POISON_RESISTANCE:
        return "it protects its wearer from poison.";
    case SPARM_SEE_INVISIBLE:
        return "it allows its wearer to see invisible things.";
    case SPARM_INVISIBILITY:
        return "when activated, it grants its wearer temporary "
               "invisibility, but also drains their maximum health.";
    case SPARM_STRENGTH:
        return "it increases the strength of its wearer (Str +3).";
    case SPARM_DEXTERITY:
        return "it increases the dexterity of its wearer (Dex +3).";
    case SPARM_INTELLIGENCE:
        return "it increases the intelligence of its wearer (Int +3).";
    case SPARM_PONDEROUSNESS:
        return "it is very cumbersome, slowing its wearer's movement.";
    case SPARM_FLYING:
        return "it grants its wearer flight.";
    case SPARM_WILLPOWER:
        return "it increases its wearer's willpower, protecting "
               "against certain magical effects.";
    case SPARM_PROTECTION:
        return "it protects its wearer from most sources of damage (AC +3).";
    case SPARM_STEALTH:
        return "it enhances the stealth of its wearer.";
    case SPARM_RESISTANCE:
        return "it protects its wearer from the effects of both fire and cold.";
    case SPARM_POSITIVE_ENERGY:
        return "it protects its wearer from the effects of negative energy.";
    case SPARM_ARCHMAGI:
        return "it increases the power of its wearer's magical spells.";
    case SPARM_PRESERVATION:
        return "it protects its wearer from the effects of acid and corrosion.";
    case SPARM_REFLECTION:
        return "it reflects blocked missile attacks back in the "
               "direction they came from.";
    case SPARM_SPIRIT_SHIELD:
        return "it causes incoming damage to be divided between "
               "the wearer's reserves of health and magic.";
    case SPARM_HURLING:
        return "it improves its wearer's accuracy and damage with "
               "thrown weapons, such as rocks and javelins (Slay +4).";
    case SPARM_REPULSION:
        return "it helps its wearer evade missiles.";
#if TAG_MAJOR_VERSION == 34
    case SPARM_CLOUD_IMMUNE:
        return "it does nothing special.";
#endif
    case SPARM_HARM:
        return "it increases damage dealt and taken.";
    case SPARM_SHADOWS:
        return "it reduces the distance the wearer can be seen at "
               "and can see.";
    case SPARM_RAMPAGING:
        return "its wearer takes one free step when moving towards enemies.";
    case SPARM_INFUSION:
        return "it empowers each of its wearer's blows with a small part of "
               "their magic.";
    case SPARM_LIGHT:
        return "it surrounds the wearer with a glowing halo, revealing "
               "invisible creatures, increasing accuracy against all within "
               "it other than the wearer, and reducing the wearer's stealth.";
    case SPARM_RAGE:
        return "it berserks the wearer when making melee attacks (20% chance).";
    case SPARM_MAYHEM:
        return "it causes witnesses of the wearer's kills to go into a frenzy,"
               " attacking everything nearby with great strength and speed.";
    case SPARM_GUILE:
        return "it weakens the willpower of the wielder and everyone they hex.";
    case SPARM_ENERGY:
        return "it may return the magic spent to cast spells, but lowers their "
               "success rate. It always returns the magic spent on miscasts.";
    default:
        return "it makes the wearer crave the taste of eggplant.";
    }
}

static string _describe_armour(const item_def &item, bool verbose, bool monster)
{
    string description;

    // orbs skip this entirely
    if (verbose && !(item.base_type == OBJ_ARMOUR && item.sub_type == ARM_ORB))
    {
        if (!monster && is_shield(item))
        {
            const int evp = -property(item, PARM_EVASION);
            const char* cumber_desc = evp < 100 ? "slightly " :
                                      evp > 100 ? "greatly " : "";
            description += make_stringf(
                "It is cumbersome to wear, and %simpedes the evasion, "
                "spellcasting ability, and attack speed of the wearer. "
                "These penalties are reduced by the wearer's Shields skill "
                "and Strength; mastering Shields eliminates penalties.",
                cumber_desc);
        }
        if (!monster)
            description += "\n\n";
        if (is_shield(item))
        {
            description += "Base shield rating: "
                        + to_string(property(item, PARM_AC));
            description += "     Encumbrance rating: "
                        + to_string(-property(item, PARM_EVASION) / 10);
            description += "     Max blocks/turn: "
                        + to_string(shield_block_limit(item));
            if (is_unrandom_artefact(item, UNRAND_WARLOCK_MIRROR))
                description += _warlock_mirror_reflect_desc();
        }
        else
        {
            const int evp = property(item, PARM_EVASION);
            description += "Base armour rating: "
                        + to_string(property(item, PARM_AC));
            if (get_armour_slot(item) == EQ_BODY_ARMOUR)
            {
                description += "       Encumbrance rating: "
                            + to_string(-evp / 10);
            }
            // Bardings reduce evasion by a fixed amount, and don't have any of
            // the other effects of encumbrance.
            else if (evp)
            {
                description += "       Evasion: "
                            + to_string(evp / 30);
            }
        }
    }

    const special_armour_type ego = get_armour_ego_type(item);

    const unrandart_entry *entry = nullptr;
    if (is_unrandom_artefact(item))
        entry = get_unrand_entry(item.unrand_idx);
    const bool skip_ego = is_unrandom_artefact(item)
                          && entry && (entry->flags & UNRAND_FLAG_SKIP_EGO);

    // Only give a description for armour with a known ego.
    if (ego != SPARM_NORMAL && item_type_known(item) && verbose && !skip_ego)
    {
        description += "\n\n";

        if (is_artefact(item))
        {
            // Make this match the formatting in _randart_descrip,
            // since instead of the item being named something like
            // 'cloak of invisiblity', it's 'the cloak of the Snail (+Inv, ...)'
            string name = string(armour_ego_name(item, true)) + ":";
            description += make_stringf("%-*s", MAX_ARTP_NAME_LEN + 1, name.c_str());
        }
        else
            description += "'Of " + string(armour_ego_name(item, false)) + "': ";

        string ego_desc = string(_item_ego_desc(ego));
        if (is_artefact(item))
            ego_desc = " " + uppercase_first(ego_desc);
        description += ego_desc;
    }

    string art_desc = _artefact_descrip(item);
    if (!art_desc.empty())
    {
        // Only add a section break if we didn't already add one before
        // printing an ego-based property.
        if (ego == SPARM_NORMAL || !verbose || skip_ego)
            description += "\n";
        description += "\n" + art_desc;
    }

    if (!is_artefact(item) && !monster)
    {
        const int max_ench = armour_max_enchant(item);
        if (max_ench > 0)
        {
            if (item.plus < max_ench || !item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                description += "\n\nIt can be maximally enchanted to +"
                               + to_string(max_ench) + ".";
            }
            else
                description += "\n\nIt cannot be enchanted further.";
        }

    }

    // Only displayed if the player exists (not for item lookup from the menu
    // or for morgues).
    if (verbose
        && crawl_state.need_save
        && can_wear_armour(item, false, true)
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        description += _equipment_ac_ev_sh_change(item);
    }

    const int DELAY_SCALE = 100;
    const int aevp = you.adjusted_body_armour_penalty(DELAY_SCALE);
    if (crawl_state.need_save
        && verbose
        && aevp
        && !is_shield(item)
        && _you_are_wearing_item(item)
        && is_slowed_by_armour(you.weapon()))
    {
        // TODO: why doesn't this show shield effect? Reconcile with
        // _display_attack_delay
        description += "\n\nYour current strength and Armour skill "
                       "slows attacks with missile weapons (like "
                        + you.weapon()->name(DESC_YOUR) + ") ";
        if (aevp >= DELAY_SCALE)
            description += make_stringf("by %.1f.", aevp / (10.0f * DELAY_SCALE));
        else
            description += "only slightly.";
    }

    return description;
}

static string _describe_lignify_ac()
{
    const Form* tree_form = get_form(transformation::tree);
    vector<const item_def *> treeform_items;

    for (auto item : you.get_armour_items())
        if (tree_form->slot_available(get_equip_slot(item)))
            treeform_items.push_back(item);

    const int treeform_ac =
        (you.base_ac_with_specific_items(100, treeform_items)
         - you.racial_ac(true) - you.ac_changes_from_mutations()
         - get_form()->get_ac_bonus() + tree_form->get_ac_bonus()) / 100;

    return make_stringf("If you quaff this potion your AC would be %d.",
                        treeform_ac);
}

string describe_item_rarity(const item_def &item)
{
    item_rarity_type rarity = consumable_rarity(item);

    switch (rarity)
    {
    case RARITY_VERY_RARE:
        return "very rare";
    case RARITY_RARE:
        return "rare";
    case RARITY_UNCOMMON:
        return "uncommon";
    case RARITY_COMMON:
        return "common";
    case RARITY_VERY_COMMON:
        return "very common";
    case RARITY_NONE:
    default:
        return "buggy";
    }
}

static string _int_with_plus(int i)
{
    if (i < 0)
        return make_stringf("%d", i);
    return make_stringf("+%d", i);
}

static string _maybe_desc_prop(const char* name, int val, int max = -1)
{
    if (val == 0 && max <= 0)
        return "";
    const int len_delta = strlen("Minimum skill") - strlen(name);
    const string padding = len_delta > 0 ? string(len_delta, ' ') : "";
    const string base = make_stringf("\n%s: %s%s",
                        name,
                        padding.c_str(),
                        _int_with_plus(val).c_str());
    if (max == val || max == -1)
        return base;
    return base + make_stringf(" (%s at max skill)",
                               _int_with_plus(max).c_str());
}

static string _describe_talisman_form(const item_def &item, bool monster)
{
    const transformation form_type = form_for_talisman(item);
    const Form* form = get_form(form_type);
    string description;
    description += make_stringf("Minimum skill: %d", form->min_skill);
    const bool below_target = _is_below_training_target(item, true);
    if (below_target)
        description += " (insufficient skill lowers this form's max HP)";
    description += make_stringf("\nMaximum skill: %d\n", form->max_skill);
    const int target_skill = _item_training_target(item);
    const bool can_set_target = below_target && in_inventory(item)
                                && !you.has_mutation(MUT_DISTRIBUTED_TRAINING);
    description += _your_skill_desc(SK_SHAPESHIFTING, can_set_target,
                                    target_skill, "   ");
    if (below_target)
        _append_skill_target_desc(description, SK_SHAPESHIFTING, target_skill);

    // defenses
    const int hp = form->mult_hp(100, true);
    const int ac = form->get_ac_bonus();
    const int ev = form->ev_bonus();
    const int body_ac_loss_percent = form->get_base_ac_penalty(100);
    const bool loses_body_ac = body_ac_loss_percent && you_can_wear(EQ_BODY_ARMOUR) != false;
    if (below_target || hp != 100 || ac || ev || loses_body_ac)
    {
        if (!monster)
            description += "\n\nDefence:";
        if (below_target || hp != 100)
        {
            description += make_stringf("\nHP:            %d%%", hp);
            if (below_target)
                description += " (reduced by your low skill)";
        }
        description += _maybe_desc_prop("Bonus AC", ac / 100,
                                        form->get_ac_bonus(true) / 100);
        description += _maybe_desc_prop("Bonus EV", ev, form->ev_bonus(true));

        if (body_ac_loss_percent)
        {
            const item_def *body_armour = you.slot_item(EQ_BODY_ARMOUR, false);
            const int base_ac = body_armour ? property(*body_armour, PARM_AC) : 0;
            const int ac_penalty = form->get_base_ac_penalty(base_ac);
            description += make_stringf("\nAC:           -%d (-%d%% of your body armour's %d base AC)",
                                        ac_penalty, body_ac_loss_percent, base_ac);
        }

        if (form->size != SIZE_CHARACTER)
            description += "\nSize:          " + uppercase_first(get_size_adj(form->size));
    }

    // offense
    if (!monster)
        description += "\n\nOffence:";
    const int uc = form->get_base_unarmed_damage(false); // TODO: compare to your base form?
                                                         // folks don't know nudists have 3
    const int max_uc = form->get_base_unarmed_damage(false, true);
    description += make_stringf("\nUC base dam.:  %d%s",
                                uc, max_uc == uc ? "" : make_stringf(" (max %d)", max_uc).c_str());
    description += _maybe_desc_prop("Slay", form->slay_bonus(false),
                                    form->slay_bonus(false, true));
    if (form_type == transformation::statue)
        description += "\nMelee damage:  +50%";
    if (form_type == transformation::flux)
    {
        description += "\nMelee damage:  -33%";
        const int contam_dam = form->contam_dam(false);
        const int max_contam_dam = form->contam_dam(false, true);
        description += make_stringf("\nContam Damage: %d", contam_dam);
        if (max_contam_dam != contam_dam)
            description += make_stringf(" (max %d)", max_contam_dam);
    }
    description += _maybe_desc_prop("Str", form->str_mod);
    description += _maybe_desc_prop("Dex", form->dex_mod);

    if (form_type == transformation::maw)
    {
        const int aux_dam = form->get_aux_damage(false);
        const int max_aux_dam = form->get_aux_damage(false, true);
        description += "\n\nMaw attack:" + aux_attack_desc(UNAT_MAW, aux_dam);
        if (max_aux_dam != aux_dam)
            description += make_stringf(" (max %d)", max_aux_dam);
    }
    else if (form_type == transformation::dragon)
    {
        // These are both dubious and that's fine.
        // Duplicates AuxBite:
        description += "\n\nBite:" + aux_attack_desc(UNAT_BITE, 1 + DRAGON_FANGS * 2);
        // Wrong if the player doesn't have muts melded and is e.g. an At:
        description += "\n\nTail slap:" + aux_attack_desc(UNAT_TAILSLAP);
    }

    // TODO: show resists (find an example of this elsewhere) (remember to include holiness)

    // misc (not covered):
    // uc brand, slots merged

    return description;
}

static string _describe_talisman(const item_def &item, bool verbose, bool monster)
{
    string description;

    if (verbose && !is_useless_item(item))
    {
        if (!monster)
        {
            description += "\n\nA period of sustained concentration is needed to "
                           "enter or leave forms. To leave this form, evoke the "
                           "talisman again.";
        }
        description += "\n\n" + _describe_talisman_form(item, monster);
    }

    // Artefact properties.
    string art_desc = _artefact_descrip(item);
    if (!art_desc.empty())
        description += "\n\n" + art_desc;

    return description;
}

static string _describe_jewellery(const item_def &item, bool verbose)
{
    string description;

    description.reserve(200);

    if (verbose && !is_artefact(item)
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        // Explicit description of ring or amulet power.
        if (item.sub_type == AMU_REFLECTION)
        {
            description += make_stringf("\n\nIt affects your shielding (%+d).",
                                        AMU_REFLECT_SH / 2);
        }
        else if (item.plus != 0)
        {
            switch (item.sub_type)
            {
            case RING_PROTECTION:
                description += make_stringf("\n\nIt affects your AC (%+d).",
                                            item.plus);
                break;

            case RING_EVASION:
                description += make_stringf("\n\nIt affects your evasion (%+d).",
                                            item.plus);
                break;

            case RING_STRENGTH:
                description += make_stringf("\n\nIt affects your strength (%+d).",
                                            item.plus);
                break;

            case RING_INTELLIGENCE:
                description += make_stringf("\n\nIt affects your intelligence (%+d).",
                                            item.plus);
                break;

            case RING_DEXTERITY:
                description += make_stringf("\n\nIt affects your dexterity (%+d).",
                                            item.plus);
                break;

            case RING_SLAYING:
                description += make_stringf("\n\nIt affects your accuracy and"
                      " damage with ranged weapons and melee (%+d).",
                      item.plus);
                break;

            default:
                break;
            }
        }
    }

    // Artefact properties.
    string art_desc = _artefact_descrip(item);
    if (!art_desc.empty())
        description += "\n\n" + art_desc;

    if (verbose
        && crawl_state.need_save
        && !you.has_mutation(MUT_NO_JEWELLERY)
        && item_ident(item, ISFLAG_KNOW_PROPERTIES)
        && (is_artefact(item)
            || item.sub_type != RING_PROTECTION
               && item.sub_type != RING_EVASION
               && item.sub_type != AMU_REFLECTION))
    {
        description += _equipment_ac_ev_sh_change(item);
    }

    return description;
}

static string _describe_item_curse(const item_def &item)
{
    if (!item.props.exists(CURSE_KNOWLEDGE_KEY))
        return "\nIt has a curse placed upon it.";

    const CrawlVector& curses = item.props[CURSE_KNOWLEDGE_KEY].get_vector();

    if (curses.empty())
        return "\nIt has a curse placed upon it.";

    ostringstream desc;

    desc << "\nIt has a curse which improves the following skills:\n";
    desc << comma_separated_fn(curses.begin(), curses.end(), desc_curse_skills,
                               ".\n", ".\n") << ".";

    return desc.str();
}

static string _describe_gizmo(const item_def &item)
{
    string ret = "\n\n";

    if (item.brand)
    {
        string name = string(gizmo_effect_name(item.brand)) + ":";
        ret += make_stringf("%-*s", MAX_ARTP_NAME_LEN + 2, name.c_str());
        switch (item.brand)
        {
            case SPGIZMO_MANAREV:
                ret += "Your spells cost less MP based on how Revved you are "
                       "(up to 3 less, but cannot reduce below 1).\n";
                break;

            case SPGIZMO_GADGETEER:
                ret += "Your evocable items recharge 30% faster and wands have "
                       "a 30% chance to not spend a charge.\n";
                break;

            case SPGIZMO_PARRYREV:
                ret += "Your AC increases as you Rev (up to +5) and while "
                       "fully Revved, your attacks may disarm enemies.\n";
                break;

            case SPGIZMO_AUTODAZZLE:
                ret += "It sometimes fires a blinding ray at enemies whose attacks "
                       "you dodge.\n";
                break;

            default:
                break;
        }
    }

    ret += _artefact_descrip(item);

    return ret;
}

bool is_dumpable_artefact(const item_def &item)
{
    return is_known_artefact(item) && item_ident(item, ISFLAG_KNOW_PROPERTIES);
}

static string &_trogsafe_lowercase(string &s)
{
    // hardcoding because of amnesia and brilliance msgs
    if (!starts_with(s, "Trog"))
        s = lowercase_first(s);
    return s;
}

static string _cannot_use_reason(const item_def &item, bool temp=true)
{
    // right now, description uselessness reasons only work for these four
    // item types..
    switch (item.base_type)
    {
    case OBJ_SCROLLS: return cannot_read_item_reason(&item, temp);
    case OBJ_POTIONS: return cannot_drink_item_reason(&item, temp);
    case OBJ_MISCELLANY:
    case OBJ_WANDS:   return cannot_evoke_item_reason(&item, temp);
    default: return "";
    }
}

static void _uselessness_desc(ostringstream &description, const item_def &item)
{
    if (!item_type_known(item))
        return;
    if (is_useless_item(item, true))
    {
        description << "\n\n"; // always needed for inv descriptions
        string r;
        if (is_useless_item(item, false))
        {
            description << "This " << base_type_string(item.base_type)
                        << " is completely useless to you";
            r = _cannot_use_reason(item, false);
        }
        else
        {
            switch (item.base_type)
            {
            case OBJ_SCROLLS:
                description << "Reading this right now";
                // this is somewhat heuristic:
                if (cannot_read_item_reason().size())
                    description << " isn't possible";
                else
                    description << " will have no effect";
                break;
            case OBJ_POTIONS:
                // XX code dup
                description << "Drinking this right now";
                if (cannot_drink_item_reason().size())
                    description << " isn't possible";
                else
                    description << " will have no effect";
                break;
            case OBJ_MISCELLANY:
            case OBJ_WANDS:
                description << "You can't evoke this right now";
                break;
            default:
                description << "This " << base_type_string(item.base_type)
                            << " is useless to you right now";
                break;
            }
            r = _cannot_use_reason(item, true);
        }
        if (!r.empty())
            description << ": " << _trogsafe_lowercase(r);
        else
            description << "."; // reasons always come with punctuation
    }
}

/**
 * Describe a specified item.
 *
 * @param item    The specified item.
 * @param mode    Controls various switches for the length of the description.
 * @return a string with the name, db desc, and some other data.
 */
string get_item_description(const item_def &item,
                            item_description_mode mode)
{
    ostringstream description;
    const bool verbose = mode == IDM_DEFAULT || mode == IDM_MONSTER;

#ifdef DEBUG_DIAGNOSTICS
    if (mode != IDM_MONSTER && mode != IDM_DUMP && !you.suppress_wizard)
    {
        description << setfill('0');
        description << "\n\n"
                    << "base: " << static_cast<int>(item.base_type)
                    << " sub: " << static_cast<int>(item.sub_type)
                    << " plus: " << item.plus << " plus2: " << item.plus2
                    << " special: " << item.special
                    << "\n"
                    << "quant: " << item.quantity
                    << " rnd?: " << static_cast<int>(item.rnd)
                    << " flags: " << hex << setw(8) << item.flags
                    << dec << "\n"
                    << "x: " << item.pos.x << " y: " << item.pos.y
                    << " link: " << item.link
                    << " slot: " << item.slot
                    << " ident_type: "
                    << get_ident_type(item)
                    << " value: " << item_value(item, true)
                    << "\nannotate: "
                    << stash_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
    }
#endif

    if (verbose || (item.base_type != OBJ_WEAPONS
                    && item.base_type != OBJ_ARMOUR
                    && item.base_type != OBJ_BOOKS))
    {
        if (mode != IDM_MONSTER)
            description << "\n\n";

        // Would be great to support DB descriptions in monster, but maybe tricky.
        bool need_base_desc = mode != IDM_MONSTER;
        if (mode == IDM_DUMP)
        {
            description << "["
                        << item.name(DESC_DBNAME, true, false, false)
                        << "]";
            need_base_desc = false;
        }
        else if (is_unrandom_artefact(item) && item_type_known(item))
        {
            const string desc = getLongDescription(get_artefact_name(item));
            if (!desc.empty())
            {
                description << desc;
                need_base_desc = false;
                description.seekp((streamoff)-1, ios_base::cur);
                description << " ";
            }
        }
        // Randart jewellery properties will be listed later,
        // just describe artefact status here.
        else if (is_artefact(item) && item_type_known(item)
                 && item.base_type == OBJ_JEWELLERY)
        {
            description << "It is an ancient artefact.";
            need_base_desc = false;
        }
        else if (item.base_type == OBJ_GIZMOS)
        {
            description << "It is a fabulous contraption, custom-made by your "
                           "own hands.";
            need_base_desc = false;
        }

        if (need_base_desc)
        {
            string db_name = item.name(DESC_DBNAME, true, false, false);
            string db_desc = getLongDescription(db_name);

            if (db_desc.empty())
            {
                if (item_type_removed(item.base_type, item.sub_type))
                    description << "This item has been removed.\n";
                else if (item_type_known(item))
                {
                    description << "[ERROR: no desc for item name '" << db_name
                                << "']. Perhaps this item has been removed?\n";
                }
                else
                {
                    description << uppercase_first(item.name(DESC_A, true,
                                                             false, false));
                    description << ".\n";
                }
            }
            else
                description << db_desc;

            // Get rid of newline at end of description; in most cases we
            // will be adding "\n\n" immediately, and we want only one,
            // not two, blank lines. This allow allows the "unpleasant"
            // message for chunks to appear on the same line.
            description.seekp((streamoff)-1, ios_base::cur);
            description << " ";
        }
    }

    bool need_extra_line = true;
    string desc;
    switch (item.base_type)
    {
    // Weapons, armour, jewellery, books might be artefacts.
    case OBJ_WEAPONS:
        desc = _describe_weapon(item, verbose, mode == IDM_MONSTER);
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_ARMOUR:
        desc = _describe_armour(item, verbose, mode == IDM_MONSTER);
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_JEWELLERY:
        desc = _describe_jewellery(item, verbose);
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_BOOKS:
        if (mode == IDM_MONSTER)
        {
            description << desc << terse_spell_list(item);
            need_extra_line = false;
        }
        else if (!verbose && is_random_artefact(item))
        {
            desc += describe_item_spells(item);
            if (desc.empty())
                need_extra_line = false;
            else
                description << desc;
        }
        break;

    case OBJ_MISSILES:
        description << _describe_ammo(item);
        break;

    case OBJ_CORPSES:
        break;

    case OBJ_STAVES:
        {
            string stats = mode == IDM_MONSTER ? "" : "\n\n";
            _append_weapon_stats(stats, item);
            description << stats;

            string art_desc = _artefact_descrip(item);
            if (!art_desc.empty())
                description << "\n\n" + art_desc;
        }
        description << "\n\nIt falls into the 'Staves' category. ";
        description << _handedness_string(item);
        break;

    case OBJ_MISCELLANY:
        if (item.sub_type == MISC_ZIGGURAT && you.zigs_completed)
        {
            const int zigs = you.zigs_completed;
            description << "\n\nIt is surrounded by a "
                        << (zigs >= 27 ? "blinding " : // just plain silly
                            zigs >=  9 ? "dazzling " :
                            zigs >=  3 ? "bright " :
                                         "gentle ")
                        << "glow.";
        }

        if (verbose)
        {
            _uselessness_desc(description, item);

            if (is_xp_evoker(item))
            {
                description << "\n\n";
                // slightly redundant with uselessness desc..
                const int charges = evoker_charges(item.sub_type);
                if (charges > 1)
                    description << "Charges: " << charges << ". Once all charges have been used";
                else
                    description << "Once activated";
                description << ", this device is rendered temporarily inert. "
                            << "However, it recharges as you gain experience.";

                const string damage_str = evoke_damage_string(item);
                if (damage_str != "")
                    description << "\nDamage: " << damage_str;

                const string noise_str = evoke_noise_string(item);
                if (noise_str != "")
                    description << "\nNoise: " << noise_str;
            }
        }

        break;

    case OBJ_POTIONS:
        if (item_type_known(item))
        {
            if (verbose)
            {
                if (is_useless_item(item, true))
                    _uselessness_desc(description, item);
                // anything past here is useful
                // TODO: more effect messages.
                // heal: print heal chance
                // curing: print heal chance and/or what would be cured
                else if (item.sub_type == POT_LIGNIFY)
                    description << "\n\n" + _describe_lignify_ac();
                else if (item.sub_type == POT_CANCELLATION)
                {
                    description << "\n\nIf you drink this now, you will no longer be " <<
                        describe_player_cancellation() << ".";
                }
            }
            description << "\n\nIt is "
                        << article_a(describe_item_rarity(item))
                        << " potion.";
            need_extra_line = false;
        }
        break;

    case OBJ_WANDS:
        if (item_type_known(item))
        {
            description << "\n";

            const string damage_str = evoke_damage_string(item);
            if (damage_str != "")
                description << "\nDamage: " << damage_str;

            const string noise_str = evoke_noise_string(item);
            if (noise_str != "")
                description << "\nNoise: " << noise_str;

            if (verbose)
                _uselessness_desc(description, item);
        }
        break;

    case OBJ_SCROLLS:
        if (item_type_known(item))
        {
            if (verbose)
                _uselessness_desc(description, item);

            description << "\n\nIt is "
                        << article_a(describe_item_rarity(item))
                        << " scroll.";
            need_extra_line = false;
        }
        break;

    case OBJ_TALISMANS:
        desc = _describe_talisman(item, verbose, mode == IDM_MONSTER);
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_GIZMOS:
        description << _describe_gizmo(item);
        break;

    case OBJ_ORBS:
    case OBJ_GOLD:
    case OBJ_RUNES:
    case OBJ_GEMS:

#if TAG_MAJOR_VERSION == 34
    case OBJ_FOOD:
    case OBJ_RODS:
#endif
        // No extra processing needed for these item types.
        break;

    default:
        die("Bad item class");
    }

    if (!verbose && item.cursed())
        description << _describe_item_curse(item);
    else if (verbose && mode != IDM_MONSTER)
    {
        if (need_extra_line)
            description << "\n";
        if (item.cursed())
            description << _describe_item_curse(item);

        if (is_artefact(item))
        {
            if (item.base_type == OBJ_ARMOUR
                || item.base_type == OBJ_WEAPONS)
            {
                if (!item_ident(item, ISFLAG_KNOW_PLUSES))
                    description << "\nIt is an ancient artefact.";
                else if (you.has_mutation(MUT_ARTEFACT_ENCHANTING))
                {
                    if (is_unrandom_artefact(item)
                        || (item.base_type == OBJ_ARMOUR
                            && item.plus >= armour_max_enchant(item))
                        || (item.base_type == OBJ_WEAPONS
                            && item.plus >= MAX_WPN_ENCHANT))
                    {
                        description << "\nEnchanting this artefact any further "
                            "is beyond even your skills.";
                    }
                }
                else
                {
                    description << "\nThis ancient artefact cannot be changed "
                        "by magic or mundane means.";
                }
            }
            // Randart jewellery has already displayed this line.
            // And gizmos really shouldn't (you just made them!)
            else if (item.base_type != OBJ_JEWELLERY && item.base_type != OBJ_GIZMOS
                     || (item_type_known(item) && is_unrandom_artefact(item)))
            {
                description << "\nIt is an ancient artefact.";
            }
        }
    }

    if (god_hates_item(item))
    {
        description << "\n\n" << uppercase_first(god_name(you.religion))
                    << " disapproves of the use of such an item.";
    }

    if (verbose && origin_describable(item))
        description << "\n" << origin_desc(item) << ".";

    // This information is obscure and differs per-item, so looking it up in
    // a docs file you don't know to exist is tedious.
    if (verbose && mode != IDM_MONSTER)
    {
        description << "\n\n" << "Stash search prefixes: "
                    << userdef_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
        string menu_prefix = item_prefix(item, false);
        if (!menu_prefix.empty())
            description << "\nMenu/colouring prefixes: " << menu_prefix;
    }

    return description.str();
}

string get_cloud_desc(cloud_type cloud, bool include_title)
{
    if (cloud == CLOUD_NONE)
        return "";
    const string cl_name = cloud_type_name(cloud);
    const string cl_desc = getLongDescription(cl_name + " cloud");

    string ret;
    if (include_title)
        ret = "A cloud of " + cl_name + (cl_desc.empty() ? "." : ".\n\n");
    ret += cl_desc + extra_cloud_info(cloud);
    return ret;
}

typedef struct {
    string title;
    string description;
    tile_def tile;
} extra_feature_desc;

static vector<extra_feature_desc> _get_feature_extra_descs(const coord_def &pos)
{
    vector<extra_feature_desc> ret;
    const dungeon_feature_type feat = env.map_knowledge(pos).feat();

    if (feat_is_tree(feat) && env.forest_awoken_until)
    {
        ret.push_back({
            "Awoken.",
            getLongDescription("awoken"),
            tile_def(TILE_AWOKEN_OVERLAY)
        });
    }
    if (feat_is_wall(feat) && env.map_knowledge(pos).flags & MAP_ICY)
    {
        ret.push_back({
            "A covering of ice.",
            getLongDescription("ice covered"),
            tile_def(TILE_FLOOR_ICY)
        });
    }
    else if (!feat_is_solid(feat))
    {
        if (haloed(pos) && !umbraed(pos))
        {
            ret.push_back({
                "A halo.",
                getLongDescription("haloed"),
                tile_def(TILE_HALO_RANGE)
            });
        }
        if (umbraed(pos) && !haloed(pos))
        {
            ret.push_back({
                "An umbra.",
                getLongDescription("umbraed"),
                tile_def(TILE_UMBRA)
            });
        }
        if (liquefied(pos, true))
        {
            ret.push_back({
                "Liquefied ground.",
                getLongDescription("liquefied"),
                tile_def(TILE_LIQUEFACTION)
            });
        }
        if (disjunction_haloed(pos))
        {
            ret.push_back({
                "Translocational energy.",
                getLongDescription("disjunction haloed"),
                tile_def(TILE_DISJUNCT)
            });
        }
    }
    if (const auto cloud = env.map_knowledge(pos).cloudinfo())
    {
        ret.push_back({
            "A cloud of " + cloud_type_name(cloud->type) + ".",
            get_cloud_desc(cloud->type, false),
            tile_def(tileidx_cloud(*cloud)),
        });
    }
    return ret;
}

static string _feat_action_desc(const vector<command_type>& actions,
                                                dungeon_feature_type feat)
{
    static const map<command_type, string> act_str =
    {
        { CMD_GO_UPSTAIRS,      "(<)go up" },
        { CMD_GO_DOWNSTAIRS,    "(>)go down" },
        { CMD_MAP_PREV_LEVEL,   "([)view destination" },
        { CMD_MAP_NEXT_LEVEL,   "(])view destination" },
        { CMD_OPEN_DOOR,        "(O)pen" },
        { CMD_CLOSE_DOOR,       "(C)lose" },

    };
    return comma_separated_fn(begin(actions), end(actions),
        [feat] (command_type cmd)
        {
            if (cmd == CMD_GO_DOWNSTAIRS && feat_is_altar(feat))
                return string("(>)pray");
            else if (cmd == CMD_GO_DOWNSTAIRS &&
                (feat == DNGN_ENTER_SHOP
                    || feat_is_portal(feat)
                    || feat_is_gate(feat)
                    || feat == DNGN_TRANSPORTER))
            {
                // XX disable for portals without item? The command still works.
                return string("(>)enter");
            }
            else if (cmd == CMD_GO_UPSTAIRS && feat_is_gate(feat))
                return string("(<)exit");
            else
                return act_str.at(cmd);
        },
        ", or ") + ".";
}

static vector<command_type> _allowed_feat_actions(const coord_def &pos)
{
    vector<command_type> actions;

    // ugh code duplication, some refactoring could be useful in all of this
    dungeon_feature_type feat = env.map_knowledge(pos).feat();
    // TODO: CMD_MAP_GOTO_TARGET
    const command_type dir = feat_stair_direction(feat);

    if (dir != CMD_NO_CMD && you.pos() == pos)
        actions.push_back(dir);

    if ((feat_is_staircase(feat) || feat_is_escape_hatch(feat))
        && !is_unknown_stair(pos)
        && feat != DNGN_EXIT_DUNGEON)
    {
        actions.push_back(dir == CMD_GO_UPSTAIRS
                            ? CMD_MAP_PREV_LEVEL : CMD_MAP_NEXT_LEVEL);
    }

    if (feat_is_door(feat) && feat != DNGN_BROKEN_DOOR && feat != DNGN_BROKEN_CLEAR_DOOR
        && (you.pos() - pos).rdist() == 1 // XX better handling of big gates
        && !feat_is_sealed(feat))
    {
        // XX maybe also a (G)o and traverse command?
        actions.push_back(feat_is_closed_door(feat)
            ? CMD_OPEN_DOOR : CMD_CLOSE_DOOR); // munge these later
    }
    return actions;
}

static string _esc_cmd_to_str(command_type cmd)
{
    string s = command_to_string(cmd);
    if (s == "<")
        return "<<";
    else
        return s;
}

void get_feature_desc(const coord_def &pos, describe_info &inf, bool include_extra)
{
    dungeon_feature_type feat = env.map_knowledge(pos).feat();

    string desc      = feature_description_at(pos, false, DESC_A);
    string desc_the  = feature_description_at(pos, false, DESC_THE);
    // remove " leading down", " leading back out of this place", etc.
    // XX maybe just dataify a short form, rather than do this awkward fixup?
    const vector<string> to_trim = {
        " leading ",
        " in the ",
        " through the ",
        " back to the ",
        " to a ",
        " to the "};
    for (const string &trim: to_trim)
        desc_the = desc_the.substr(0, desc_the.find(trim));

    string db_name   = feat == DNGN_ENTER_SHOP ? "a shop" : desc;
    strip_suffix(db_name, " (summoned)");
    string long_desc = getLongDescription(db_name);

    inf.title = uppercase_first(desc);
    if (!ends_with(desc, ".") && !ends_with(desc, "!")
        && !ends_with(desc, "?") && !desc.empty())
    {
        inf.title += ".";
    }
    // don't show "The floor." twice (maybe other things).
    if (trimmed_string(long_desc) == inf.title)
        long_desc = "";

    const string marker_desc =
        env.markers.property_at(pos, MAT_ANY, "feature_description_long");

    // suppress this if the feature changed out of view
    if (!marker_desc.empty() && env.grid(pos) == feat)
        long_desc += marker_desc;

    // Display branch descriptions on the entries to those branches.
    if (feat_is_stair(feat))
    {
        for (branch_iterator it; it; ++it)
        {
            if (it->entry_stairs == feat)
            {
                long_desc += "\n";
                long_desc += getLongDescription(it->shortname);
                break;
            }
        }
        const auto stair_dir = feat_stair_direction(feat);

        // it seems like there must be some way to refactor all this stuff so
        // it's cleaner...
        if (feat == DNGN_EXIT_DUNGEON)
        {
            // XX custom messages for other branch entrances/exits?
            long_desc += make_stringf(
                    "\nWhile standing here, you can exit the dungeon "
                    "with the <w>%s</w> key.",
                    _esc_cmd_to_str(stair_dir).c_str());
        }
        else if (feat_is_staircase(feat) || feat_is_escape_hatch(feat))
        {
            long_desc += make_stringf(
                    "\nWhile standing here, you can traverse %s "
                    "with the <w>%s</w> key.",
                    desc_the.c_str(),
                    _esc_cmd_to_str(stair_dir).c_str());

            if (is_unknown_stair(pos))
            {
                long_desc += " You have not yet explored it and cannot tell "
                             "where it leads.";
            }
            else
            {
                const command_type look_dir =
                    feat_stair_direction(feat) == CMD_GO_UPSTAIRS
                                         ? CMD_MAP_PREV_LEVEL
                                         : CMD_MAP_NEXT_LEVEL;
                long_desc +=
                    make_stringf(" You can view the location it leads to by "
                                 "examining it with <w>%s</w> and pressing "
                                 "<w>%s</w>.",
                                 command_to_string(CMD_DISPLAY_MAP).c_str(),
                                 command_to_string(look_dir).c_str());
            }
        }
        else if (feat_is_portal(feat)
            || feat == DNGN_ENTER_ZIGGURAT) // augh this is technically a gate
        {
            long_desc += make_stringf(
                    "\nWhile standing here, you can %senter %s "
                    "with the <w>%s</w> key; it will vanish after you do so.",
                    feat == DNGN_ENTER_TROVE ? "try to " : "",
                    desc_the.c_str(),
                    _esc_cmd_to_str(stair_dir).c_str());
        }
        else if (feat_is_gate(feat))
        {
            // it's just special case after special case with these descs
            const string how = feat == DNGN_ENTER_HELL
                ? (stair_dir == CMD_GO_UPSTAIRS ? "to Hell" : "Hell")
                : make_stringf("through %s", desc_the.c_str());
            long_desc += make_stringf(
                    "\nWhile standing here, you can %s %s "
                    "with the <w>%s</w> key%s.",
                    stair_dir == CMD_GO_DOWNSTAIRS ? "enter" : "exit",
                    how.c_str(),
                    _esc_cmd_to_str(stair_dir).c_str(),
                    (feat == DNGN_ENTER_ZOT || feat == DNGN_EXIT_VAULTS)
                        ? " if you have enough runes" : "");
        }
    }
    else if (feat_is_altar(feat))
    {
        long_desc +=
            make_stringf("\nPray here with <w>%s</w> to learn more.\n",
                         command_to_string(CMD_GO_DOWNSTAIRS).c_str());
    }
    else if (feat == DNGN_ENTER_SHOP)
    {
        long_desc += make_stringf(
                "\nWhile standing here, you can enter <w>%s</w> with "
                "the <w>%s</w> key.",
                desc.c_str(),
                command_to_string(CMD_GO_DOWNSTAIRS).c_str());
    }
    else if (feat == DNGN_TRANSPORTER)
    {
        long_desc += make_stringf(
                "\nWhile standing here, you can enter %s with "
                "with the <w>%s</w> key.",
                desc_the.c_str(),
                command_to_string(CMD_GO_DOWNSTAIRS).c_str());
    }

    // mention that permanent trees are usually flammable
    // (except for autumnal trees in Wucad Mu's Monastery)
    if (feat_is_flammable(feat) && !is_temp_terrain(pos) && in_bounds(pos)
        && env.markers.property_at(pos, MAT_ANY, "veto_destroy") != "veto")
    {
        if (feat == DNGN_TREE)
            long_desc += "\n" + getLongDescription("tree burning");
        else if (feat == DNGN_MANGROVE)
            long_desc += "\n" + getLongDescription("mangrove burning");
        else if (feat == DNGN_DEMONIC_TREE)
            long_desc += "\n" + getLongDescription("demonic tree burning");
    }

    if (feat_is_door(feat) && feat != DNGN_BROKEN_DOOR && feat != DNGN_BROKEN_CLEAR_DOOR)
    {
        const bool openable = feat_is_closed_door(feat);
        const bool sealed = feat_is_sealed(feat);

        long_desc += make_stringf(
            "\nWhile adjacent to it, you can %s%s it by pressing the "
            "<w>%s</w> key.",
            sealed ? "normally " : "",
            openable ? "open" : "close",
            command_to_string(openable ? CMD_OPEN_DOOR : CMD_CLOSE_DOOR).c_str());
        if (sealed)
            long_desc += " However, this will not work until the sealing runes wear off.";
    }

    // mention that diggable walls are
    if (feat_is_diggable(feat) && in_bounds(pos)
        && env.markers.property_at(pos, MAT_ANY, "veto_destroy") != "veto")
    {
        long_desc += "\nIt can be dug through.";
    }

    // Mention things which don't stop LOS_NO_TRANS or reaching weapons,
    // but block movement.
    if (feat_is_solid(feat) && !feat_is_opaque(feat) && !feat_is_wall(feat)
        && !feat_is_closed_door(feat) && !feat_is_endless(feat))
    {
        if (feat_is_reachable_past(feat))
            long_desc += "\nA suitable weapon or spell can reach past it.";
        else
            long_desc += "\nSome spells can be cast through it.";
    }

    if (pos == you.pos())
        long_desc += "\nYou are here.";

    inf.body << long_desc;

    if (include_extra)
    {
        const auto extra_descs = _get_feature_extra_descs(pos);
        for (const auto &d : extra_descs)
            inf.body << (d.title == extra_descs.back().title ? "" : "\n") << d.description;
    }

    inf.quote = getQuoteString(db_name);
}

static bool _do_feat_action(const coord_def &pos, const command_type action)
{
    if (action == CMD_NO_CMD)
        return true;

    switch (action)
    {
    case CMD_GO_UPSTAIRS:
    case CMD_GO_DOWNSTAIRS:
        ASSERT(pos == you.pos());
        process_command(action, crawl_state.prev_cmd);
        return false;
    case CMD_MAP_PREV_LEVEL:
    case CMD_MAP_NEXT_LEVEL:
    {
        level_pos dest = map_follow_stairs(action == CMD_MAP_PREV_LEVEL, pos);
        // XX this leaves view mode on the ui stack, any issues?
        if (dest.is_valid())
        {
            show_map(dest, true, true);
            return true;
        }
        break;
    }
    case CMD_OPEN_DOOR:
        open_door_action(pos - you.pos());
        return false;
    case CMD_CLOSE_DOOR:
        close_door_action(pos - you.pos());
        return false;
    default:
        break;
    }
    return true;
}

/*
 * @return: true if the game should stay in any outer mode (e.g. `xv`).
 */
bool describe_feature_wide(const coord_def& pos, bool do_actions)
{
    typedef struct {
        string title, body, quote;
        tile_def tile;
    } feat_info;

    vector<feat_info> feats;

    {
        describe_info inf;
        get_feature_desc(pos, inf, false);
        feat_info f = { "", "", "", tile_def(TILEG_TODO)};
        f.title = inf.title;
        f.body = trimmed_string(inf.body.str());
#ifdef USE_TILE
        tileidx_t tile = tileidx_feature(pos);
        apply_variations(tile_env.flv(pos), &tile, pos);
        f.tile = tile_def(tile);
#endif
        f.quote = trimmed_string(inf.quote);
        if (f.title.size() || f.body.size() || f.quote.size())
            feats.emplace_back(f);
    }
    auto extra_descs = _get_feature_extra_descs(pos);
    for (const auto &desc : extra_descs)
    {
        feat_info f = { "", "", "", tile_def(TILEG_TODO)};
        f.title = desc.title;
        f.body = trimmed_string(desc.description);
        f.tile = desc.tile;
        if (f.title.size() || f.body.size() || f.quote.size())
            feats.emplace_back(f);
    }
    if (crawl_state.game_is_hints())
    {
        string hint_text = trimmed_string(hints_describe_pos(pos.x, pos.y));
        if (!hint_text.empty())
        {
            feat_info f = { "", "", "", tile_def(TILEG_TODO)};
            f.body = hint_text;
            f.tile = tile_def(TILEG_STARTUP_HINTS);
            feats.emplace_back(f);
        }
    }
    if (feats.empty())
        return true;

    auto scroller = make_shared<Scroller>();
    auto vbox = make_shared<Box>(Widget::VERT);

    for (const auto &feat : feats)
    {
        auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
        auto icon = make_shared<Image>();
        icon->set_tile(feat.tile);
        title_hbox->add_child(std::move(icon));
#endif
        auto title = make_shared<Text>(feat.title);
        title->set_margin_for_sdl(0, 0, 0, 10);
        title_hbox->add_child(std::move(title));
        title_hbox->set_cross_alignment(Widget::CENTER);

        const bool has_desc = feat.body != feat.title && feat.body != "";

        if (has_desc || &feat != &feats.back())
        {
            title_hbox->set_margin_for_crt(0, 0, 1, 0);
            title_hbox->set_margin_for_sdl(0, 0, 20, 0);
        }
        vbox->add_child(std::move(title_hbox));

        if (has_desc)
        {
            formatted_string desc_text = formatted_string::parse_string(feat.body);
            if (!feat.quote.empty())
            {
                desc_text.cprintf("\n\n");
                desc_text += formatted_string::parse_string(feat.quote);
            }
            auto text = make_shared<Text>(desc_text);
            if (&feat != &feats.back())
            {
                text->set_margin_for_sdl(0, 0, 20, 0);
                text->set_margin_for_crt(0, 0, 1, 0);
            }
            text->set_wrap_text(true);
            vbox->add_child(text);
        }
    }

    vector<command_type> actions;
    if (do_actions)
        actions = _allowed_feat_actions(pos);
    formatted_string footer_text("", CYAN);
    if (!actions.empty())
    {
        const dungeon_feature_type feat = env.map_knowledge(pos).feat();
        footer_text += formatted_string(_feat_action_desc(actions, feat));
        auto footer = make_shared<Text>();
        footer->set_text(footer_text);
        footer->set_margin_for_crt(1, 0, 0, 0);
        footer->set_margin_for_sdl(20, 0, 0, 0);
        vbox->add_child(std::move(footer));
    }

#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif
    scroller->set_child(std::move(vbox));

    auto popup = make_shared<ui::Popup>(scroller);

    bool done = false;
    command_type action = CMD_NO_CMD;

    // use on_hotkey_event, not on_event, to preempt the scroller key handling
    popup->on_hotkey_event([&](const KeyEvent& ev) {
        int lastch = ev.key();
        action = _get_action(lastch, actions);
        done = action != CMD_NO_CMD || ui::key_exits_popup(lastch, true);
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_open_array("feats");
    for (const auto &feat : feats)
    {
        tiles.json_open_object();
        tiles.json_write_string("title", feat.title);
        tiles.json_write_string("body", trimmed_string(feat.body));
        tiles.json_write_string("quote", trimmed_string(feat.quote));
        tiles.json_open_object("tile");
        tiles.json_write_int("t", feat.tile.tile);
        tiles.json_write_int("tex", get_tile_texture(feat.tile.tile));
        if (feat.tile.ymax != TILE_Y)
            tiles.json_write_int("ymax", feat.tile.ymax);
        tiles.json_close_object();
        tiles.json_close_object();
    }
    tiles.json_close_array();
    tiles.json_write_string("actions", footer_text.tostring());
    tiles.push_ui_layout("describe-feature-wide", 0);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    ui::run_layout(std::move(popup), done);
    return _do_feat_action(pos, action);
}

void describe_feature_type(dungeon_feature_type feat)
{
    describe_info inf;
    string name = feature_description(feat, NUM_TRAPS, "", DESC_A);
    string title = uppercase_first(name);
    if (!ends_with(title, ".") && !ends_with(title, "!") && !ends_with(title, "?"))
        title += ".";
    inf.title = title;
    inf.body << getLongDescription(name);
#ifdef USE_TILE
    const tileidx_t idx = tileidx_feature_base(feat);
    tile_def tile = tile_def(idx);
    show_description(inf, &tile);
#else
    show_description(inf);
#endif
}

void get_item_desc(const item_def &item, describe_info &inf)
{
    // Don't use verbose descriptions if the item contains spells,
    // so we can actually output these spells if space is scarce.
    const item_description_mode mode = item.has_spells() ? IDM_PLAIN
                                                         : IDM_DEFAULT;
    string name = item.name(DESC_INVENTORY_EQUIP) + ".";
    if (!in_inventory(item))
        name = uppercase_first(name);
    inf.body << name << get_item_description(item, mode);
}

static vector<command_type> _allowed_actions(const item_def& item)
{
    vector<command_type> actions;

    if (!in_inventory(item) && item.pos != you.pos()
        && is_travelsafe_square(item.pos))
    {
        // XX might be nice to have a way for this to be visible but grayed
        // out?
        actions.push_back(CMD_MAP_GOTO_TARGET);
        return actions;
    }

    // this is a copy, we can't do anything with it. (Probably via stash
    // search.)
    if (!valid_item_index(item.index()) && !in_inventory(item))
        return actions;

    // XX CMD_ACTIVATE
    switch (item.base_type)
    {
    case OBJ_SCROLLS:
        if (cannot_read_item_reason(&item, true).empty())
            actions.push_back(CMD_READ);
        break;
    case OBJ_POTIONS:
        if (cannot_drink_item_reason(&item, true).empty())
            actions.push_back(CMD_QUAFF);
        break;
    default:
        break;
    }

    if (!in_inventory(item))
    {
        // guaranteed to be at the player's position
        if (!item_is_stationary(item))
            actions.push_back(CMD_PICKUP);
        return actions;
    }

    // evoking from inventory only
    if (cannot_evoke_item_reason(&item, true).empty())
        actions.push_back(CMD_EVOKE);

    if (quiver::slot_to_action(item.link)->is_valid())
        actions.push_back(CMD_QUIVER_ITEM);

    // what is this for?
    if (clua.callbooleanfn(false, "ch_item_wieldable", "i", &item))
        actions.push_back(CMD_WIELD_WEAPON);

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
        if (!item_is_equipped(item))
        {
            if (item_is_wieldable(item))
                actions.push_back(CMD_WIELD_WEAPON);
        }
        else if (item_equip_slot(item) == EQ_WEAPON)
            actions.push_back(CMD_UNWIELD_WEAPON);
        break;
    case OBJ_ARMOUR:
        if (item_is_equipped(item))
            actions.push_back(CMD_REMOVE_ARMOUR);
        else
            actions.push_back(CMD_WEAR_ARMOUR);
        break;
    case OBJ_JEWELLERY:
        if (item_is_equipped(item))
            actions.push_back(CMD_REMOVE_JEWELLERY);
        else
            actions.push_back(CMD_WEAR_JEWELLERY);
        break;
    default:
        break;
    }
    actions.push_back(CMD_DROP);
    actions.push_back(CMD_ADJUST_INVENTORY);
    if (!you.has_mutation(MUT_DISTRIBUTED_TRAINING)
        && _is_below_training_target(item, false))
    {
        actions.push_back(CMD_SET_SKILL_TARGET);
    }

    if (!crawl_state.game_is_tutorial())
        actions.push_back(CMD_INSCRIBE_ITEM);

    return actions;
}

static string _actions_desc(const vector<command_type>& actions)
{
    // XX code duplication
    static const map<command_type, string> act_str =
    {
        { CMD_WIELD_WEAPON, "(w)ield" },
        { CMD_UNWIELD_WEAPON, "(u)nwield" },
        { CMD_QUIVER_ITEM, "(q)uiver" }, // except for potions, see below
        { CMD_WEAR_ARMOUR, "(w)ear" },
        { CMD_REMOVE_ARMOUR, "(t)ake off" },
        { CMD_EVOKE, "e(v)oke" },
        { CMD_READ, "(r)ead" },
        { CMD_WEAR_JEWELLERY, "(p)ut on" },
        { CMD_REMOVE_JEWELLERY, "(r)emove" },
        { CMD_QUAFF, "(q)uaff" },
        { CMD_PICKUP, "(g)et" },
        { CMD_DROP, "(d)rop" },
        { CMD_INSCRIBE_ITEM, "(i)nscribe" },
        { CMD_ADJUST_INVENTORY, "(=)adjust" },
        { CMD_SET_SKILL_TARGET, "(s)kill target" },
        { CMD_MAP_GOTO_TARGET, "(g)o to location" },
    };
    bool push_quiver = false;
    return comma_separated_fn(begin(actions), end(actions),
                                [&push_quiver] (command_type cmd)
                                {
                                    if (cmd == CMD_QUAFF) // assumes quaff appears first
                                        push_quiver = true;
                                    else if (push_quiver && cmd == CMD_QUIVER_ITEM)
                                        return string("qui(v)er");
                                    return act_str.at(cmd);
                                },
                                ", or ") + ".";
}

// Take a key and a list of commands and return the command from the list
// that corresponds to the key. Note that some keys are overloaded (but with
// mutually-exclusive actions), so it's not just a simple lookup.
static command_type _get_action(int key, vector<command_type> actions)
{
    static const map<command_type, int> act_key =
    {
        { CMD_WIELD_WEAPON,     'w' },
        { CMD_UNWIELD_WEAPON,   'u' },
        { CMD_QUIVER_ITEM,      'q' }, // except for potions, see below
        { CMD_WEAR_ARMOUR,      'w' },
        { CMD_REMOVE_ARMOUR,    't' },
        { CMD_EVOKE,            'v' },
        { CMD_READ,             'r' },
        { CMD_WEAR_JEWELLERY,   'p' },
        { CMD_REMOVE_JEWELLERY, 'r' },
        { CMD_QUAFF,            'q' },
        { CMD_DROP,             'd' },
        { CMD_INSCRIBE_ITEM,    'i' },
        { CMD_PICKUP,           'g' },
        { CMD_ADJUST_INVENTORY, '=' },
        { CMD_SET_SKILL_TARGET, 's' },
        { CMD_MAP_GOTO_TARGET,  'g' },
        { CMD_GO_UPSTAIRS,      '<' },
        { CMD_GO_DOWNSTAIRS,    '>' },
        { CMD_MAP_PREV_LEVEL,   '[' },
        { CMD_MAP_NEXT_LEVEL,   ']' },
        { CMD_OPEN_DOOR,        'o' },
        { CMD_CLOSE_DOOR,       'c' },
    };

    key = tolower_safe(key);
    bool push_quiver = false;

    for (auto cmd : actions)
    {
        if (cmd == CMD_QUAFF) // quaff must come first
            push_quiver = true;
        if (push_quiver && cmd == CMD_QUIVER_ITEM && key == 'v' // ugly
            || key == act_key.at(cmd))
        {
            return cmd;
        }
    }

    return CMD_NO_CMD;
}

static level_id _item_level_id(const item_def &item)
{
    level_id loc;
    // set on stash search only
    if (!item.props.exists("level_id"))
        return loc;

    try
    {
        loc = level_id::parse_level_id(item.props["level_id"].get_string());
    }
    catch (const bad_level_id &err)
    {
        // die?
    }
    return loc;
}

/**
 * Do the specified action on the specified item.
 *
 * @param item    the item to have actions done on
 * @param action  the action to do
 * @return whether to stay in the inventory menu afterwards
 */
static bool _do_action(item_def &item, const command_type action)
{
    if (action == CMD_NO_CMD)
        return true;

    unwind_bool no_more(crawl_state.show_more_prompt, false);

    // ok in principle on floor items (though I didn't enable the last two)
    switch (action)
    {
    case CMD_MAP_GOTO_TARGET:
        if (in_bounds(item.pos))
        {
            level_id loc = _item_level_id(item); // stash search handling
            if (!loc.is_valid())
                loc = level_id::current(); // may still be during an excursion
            level_pos target(loc, item.pos);
            start_translevel_travel(target);
            return false;
        }
        break;
    case CMD_PICKUP:
        ASSERT(!in_inventory(item) && valid_item_index(item.index()));
        pickup_single_item(item.index(), item.quantity);
        return false;
    case CMD_READ:             read(&item);         return false;
    case CMD_QUAFF:            drink(&item);        return false;
    case CMD_INSCRIBE_ITEM:    inscribe_item(item); return false;
    case CMD_SET_SKILL_TARGET: target_item(item);   return false;
    default:
        break;
    }

    // inv item only
    const int slot = item.link;
    ASSERT_RANGE(slot, 0, ENDOFPACK);

    switch (action)
    {
    case CMD_WIELD_WEAPON:     wield_weapon(slot);            break;
    case CMD_UNWIELD_WEAPON:   wield_weapon(SLOT_BARE_HANDS); break;
    case CMD_QUIVER_ITEM:
        quiver::set_to_quiver(quiver::slot_to_action(slot), you.quiver_action); // ugh
        break;
    case CMD_WEAR_ARMOUR:      wear_armour(slot);             break;
    case CMD_REMOVE_ARMOUR:    takeoff_armour(slot);          break;
    case CMD_WEAR_JEWELLERY:   puton_ring(you.inv[slot]);     break;
    case CMD_REMOVE_JEWELLERY: remove_ring(slot, true);       break;
    case CMD_DROP:
        // TODO: it would be better if the inscription was checked before the
        // popup closes, but that is hard
        if (!check_warning_inscriptions(you.inv[slot], OPER_DROP))
            return true;
        drop_item(slot, item.quantity);
        break;
    case CMD_ADJUST_INVENTORY: adjust_item(slot);             break;
    case CMD_EVOKE:
        if (!check_warning_inscriptions(you.inv[slot], OPER_EVOKE))
            return true;
        evoke_item(you.inv[slot]);
        break;
    default:
        ui::error(make_stringf("illegal inventory cmd '%d'", action));
    }
    return false;
}

void target_item(item_def &item)
{
    const skill_type skill = _item_training_skill(item);
    if (skill == SK_NONE)
        return;

    const int target = _item_training_target(item);
    if (target == 0)
        return;

    you.set_training_target(skill, target, true);
    // ensure that the skill is at least enabled
    if (you.train[skill] == TRAINING_DISABLED)
        set_training_status(skill, TRAINING_ENABLED);
    you.train_alt[skill] = you.train[skill];
    reset_training();
}

/**
 *  Display a pop-up describe any item in the game.
 *
 *  @param item       the item to be described.
 *  @param fixup_desc a function (possibly null) to modify the
 *                    description before it's displayed.
 *  @param do_actions display interaction options
 *  @return an action to perform (if any was available or selected)
 *
 */
command_type describe_item_popup(const item_def &item,
                                 function<void (string&)> fixup_desc,
                                 bool do_actions)
{
    if (!item.defined())
        return CMD_NO_CMD;

    // Dead players use no items.
    do_actions = do_actions && !(you.pending_revival || crawl_state.updating_scores);

    string name = item.name(DESC_INVENTORY_EQUIP) + ".";
    if (!in_inventory(item))
        name = uppercase_first(name);

    string desc = "";
    level_id loc = _item_level_id(item);
    if (loc.is_valid() && loc != level_id::current())
    {
        // should be off-level stash search only
        desc += make_stringf("It can be found on %s.\n\n",
            loc.describe(true, true).c_str());
    }

    desc += get_item_description(item);

    string quote;
    if (is_unrandom_artefact(item) && item_type_known(item))
        quote = getQuoteString(get_artefact_name(item));
    else
        quote = getQuoteString(item.name(DESC_DBNAME, true, false, false));

    if (!(crawl_state.game_is_hints_tutorial()
          || quote.empty()))
    {
        desc += "\n\n" + quote;
    }

    if (crawl_state.game_is_hints())
        desc += "\n\n" + hints_describe_item(item);

    if (fixup_desc)
        fixup_desc(desc);

    formatted_string fs_desc = formatted_string::parse_string(desc);

    spellset spells = item_spellset(item);
    formatted_string spells_desc;
    describe_spellset(spells, &item, spells_desc, nullptr);
#ifdef USE_TILE_WEB
    string desc_without_spells = fs_desc.to_colour_string(LIGHTGRAY);
#endif
    fs_desc += spells_desc;

    vector<command_type> actions;
    if (do_actions)
        actions = _allowed_actions(item);

    auto vbox = make_shared<Box>(Widget::VERT);
    auto title_hbox = make_shared<Box>(Widget::HORZ);

#ifdef USE_TILE
    vector<tile_def> item_tiles;
    get_tiles_for_item(item, item_tiles, true);
    if (item_tiles.size() > 0)
    {
        auto tiles_stack = make_shared<Stack>();
        for (const auto &tile : item_tiles)
        {
            auto icon = make_shared<Image>();
            icon->set_tile(tile);
            tiles_stack->add_child(std::move(icon));
        }
        title_hbox->add_child(std::move(tiles_stack));
    }
#endif

    auto title = make_shared<Text>(name);
    title->set_margin_for_sdl(0, 0, 0, 10);
    title_hbox->add_child(std::move(title));

    title_hbox->set_cross_alignment(Widget::CENTER);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    vbox->add_child(std::move(title_hbox));

    auto scroller = make_shared<Scroller>();
    auto text = make_shared<Text>(fs_desc.trim());
    text->set_wrap_text(true);
    scroller->set_child(text);
    vbox->add_child(scroller);

    formatted_string footer_text("", CYAN);
    if (!actions.empty())
    {
        if (!spells.empty())
            footer_text.cprintf("Select a spell, or ");
        footer_text += formatted_string(_actions_desc(actions));
        auto footer = make_shared<Text>();
        footer->set_text(footer_text);
        footer->set_margin_for_crt(1, 0, 0, 0);
        footer->set_margin_for_sdl(20, 0, 0, 0);
        vbox->add_child(std::move(footer));
    }

#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif

    auto popup = make_shared<ui::Popup>(std::move(vbox));

    bool done = false;
    command_type action = CMD_NO_CMD;
    int lastch; // unused??
    popup->on_keydown_event([&](const KeyEvent& ev) {
        const auto key = ev.key() == '{' ? 'i' : ev.key();
        lastch = key;
        action = _get_action(key, actions);
        if (action != CMD_NO_CMD || ui::key_exits_popup(key, true))
            done = true;
        else if (scroller->on_event(ev))
            return true;

        const vector<pair<spell_type,char>> spell_map = map_chars_to_spells(spells);
        auto entry = find_if(spell_map.begin(), spell_map.end(),
                [key](const pair<spell_type,char>& e) { return e.second == key; });
        if (entry == spell_map.end())
            return false;
        describe_spell(entry->first, nullptr, &item);
        done = already_learning_spell();
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("title", name);
    desc_without_spells += "SPELLSET_PLACEHOLDER";
    trim_string(desc_without_spells);
    tiles.json_write_string("body", desc_without_spells);
    write_spellset(spells, &item, nullptr);

    tiles.json_write_string("actions", footer_text.tostring());
    tiles.json_open_array("tiles");
    for (const auto &tile : item_tiles)
    {
        tiles.json_open_object();
        tiles.json_write_int("t", tile.tile);
        tiles.json_write_int("tex", get_tile_texture(tile.tile));
        if (tile.ymax != TILE_Y)
            tiles.json_write_int("ymax", tile.ymax);
        tiles.json_close_object();
    }
    tiles.json_close_array();
    tiles.push_ui_layout("describe-item", 0);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    ui::run_layout(std::move(popup), done);

    return action;
}

/**
 *  Describe any item in the game and offer interactions if available.
 *
 *  This is split out from the popup because _do_action is necessarily non
 *  const but we would like to offer the description UI for items not in
 *  inventory in places where only a const item_def is available.
 *
 *  @param item       the item to be described.
 *  @param fixup_desc a function (possibly null) to modify the
 *                    description before it's displayed.
 *  @return whether to remain in the outer mode (inventory, `xv`) after the popup.
 *
 */
bool describe_item(item_def &item, function<void (string&)> fixup_desc, bool do_actions)
{
    command_type action = describe_item_popup(item, fixup_desc, do_actions);
    return _do_action(item, action);
}

void inscribe_item(item_def &item)
{
    mprf_nocap(MSGCH_EQUIPMENT, "%s", item.name(DESC_INVENTORY).c_str());

    const bool is_inscribed = !item.inscription.empty();
    string prompt = is_inscribed ? "Replace inscription with what? "
                                 : "Inscribe with what? ";

    char buf[79];
    int ret = msgwin_get_line(prompt, buf, sizeof buf, nullptr,
                              item.inscription);
    if (ret)
    {
        canned_msg(MSG_OK);
        return;
    }

    string new_inscrip = buf;
    trim_string_right(new_inscrip);

    if (item.inscription == new_inscrip)
    {
        canned_msg(MSG_OK);
        return;
    }

    item.inscription = new_inscrip;

    mprf_nocap(MSGCH_EQUIPMENT, "%s", item.name(DESC_INVENTORY).c_str());
    you.wield_change  = true;
    quiver::set_needs_redraw();
}

/**
 * List the simple calculated stats of a given spell, when cast by the player
 * in their current condition.
 *
 * @param spell     The spell in question.
 */
static string _player_spell_stats(const spell_type spell)
{
    string description;
    description += make_stringf("\nLevel: %d", spell_difficulty(spell));

    const string schools = spell_schools_string(spell);
    description +=
        make_stringf("        School%s: %s",
                     schools.find("/") != string::npos ? "s" : "",
                     schools.c_str());

    if (!crawl_state.need_save
        || (get_spell_flags(spell) & spflag::monster))
    {
        return description; // all other info is player-dependent
    }


    string failure;
    if (you.divine_exegesis)
        failure = "0%";
    else
        failure = failure_rate_to_string(raw_spell_fail(spell));
    description += make_stringf("        Fail: %s", failure.c_str());

    const string damage_string = spell_damage_string(spell);
    const string max_dam_string = spell_max_damage_string(spell);
    const int acc = spell_acc(spell);
    // TODO: generalize this pattern? It's very common in descriptions
    const int padding = (acc != -1) ? 8 : damage_string.size() ? 6 : 5;
    description += make_stringf("\n\n%*s: ", padding, "Power");
    description += spell_power_string(spell);

    if (damage_string != "")
    {
        description += make_stringf("\n%*s: ", padding, "Damage");
        description += damage_string;

        const string max_dam = spell_max_damage_string(spell);
        if (!max_dam.empty())
            description += " (max " + max_dam + ")";
    }
    if (acc != -1)
    {
        ostringstream acc_str;
        _print_bar(acc, 3, "", acc_str);
        description += make_stringf("\n%*s: %s", padding, "Accuracy",
                                                    acc_str.str().c_str());
    }

    description += make_stringf("\n%*s: ", padding, "Range");
    description += spell_range_string(spell);
    description += make_stringf("\n%*s: ", padding, "Noise");
    description += spell_noise_string(spell);
    description += "\n";
    return description;
}

string get_skill_description(skill_type skill, bool need_title)
{
    string lookup = skill_name(skill);
    string result = "";

    if (need_title)
    {
        result = lookup;
        result += "\n\n";
    }

    result += getLongDescription(lookup);

    if (skill == SK_INVOCATIONS)
    {
        if (you.has_mutation(MUT_FORLORN))
        {
            result += "\n";
            result += "How on earth did you manage to pick this up?";
        }
        else if (invo_skill(you.religion) != SK_INVOCATIONS)
        {
            result += "\n";
            result += uppercase_first(apostrophise(god_name(you.religion)))
                      + " powers are not affected by the Invocations skill.";
        }
    }
    else if (skill == invo_skill(you.religion))
    {
        result += "\n";
        result += uppercase_first(apostrophise(god_name(you.religion)))
                  + " powers are based on " + skill_name(skill) + " instead"
                    " of Invocations skill.";
    }

    if (is_harmful_skill(skill))
    {
        result += "\n";
        result += uppercase_first(god_name(you.religion))
                  + " strongly dislikes when you train this skill.";
    }

    return result;
}

/// How much power do we think the given monster casts this spell with?
static int _hex_pow(const spell_type spell, const int hd)
{
    const int cap = 200;
    const int pow = mons_power_for_hd(spell, hd) / ENCH_POW_FACTOR;
    return min(cap, pow);
}

/**
 * What are the odds of the given spell, cast by a monster with the given
 * spell_hd, affecting the player?
 */
int hex_chance(const spell_type spell, const monster_info* mi)
{
    const int capped_pow = _hex_pow(spell, mi->spell_hd());
    const bool guile = mi->inv[MSLOT_SHIELD]
                       && get_armour_ego_type(*mi->inv[MSLOT_SHIELD]) == SPARM_GUILE;
    const int will = guile ? guile_adjust_willpower(you.willpower())
                           : you.willpower();
    const int chance = hex_success_chance(will, capped_pow,
                                          100, true);
    if (spell == SPELL_STRIP_WILLPOWER)
        return chance + (100 - chance) / 3; // ignores wl 1/3rd of the time
    return chance;
}

/**
 * Describe miscast effects from a spell
 *
 * @param spell
 */
static string _miscast_damage_string(spell_type spell)
{
    const map <spschool, string> damage_flavor = {
        { spschool::conjuration, "irresistible" },
        { spschool::necromancy, "draining" },
        { spschool::fire, "fire" },
        { spschool::ice, "cold" },
        { spschool::air, "electric" },
        { spschool::earth, "fragmentation" },
    };

    const map <spschool, string> special_flavor = {
        { spschool::summoning, "summons a nameless horror" },
        { spschool::translocation, "anchors you in place" },
        { spschool::hexes, "slows you" },
        { spschool::alchemy, "poisons you" },
    };

    spschools_type disciplines = get_spell_disciplines(spell);
    vector <string> descs;

    for (const auto &flav : special_flavor)
        if (disciplines & flav.first)
            descs.push_back(flav.second);

    int dam = max_miscast_damage(spell);
    vector <string> dam_flavors;
    for (const auto &flav : damage_flavor)
        if (disciplines & flav.first)
            dam_flavors.push_back(flav.second);

    if (!dam_flavors.empty())
    {
        descs.push_back(make_stringf("deals up to %d %s damage", dam,
                                     comma_separated_line(dam_flavors.begin(),
                                                         dam_flavors.end(),
                                                         " or ").c_str()));
    }

    return (descs.size() > 1 ? "either " : "")
         + comma_separated_line(descs.begin(), descs.end(), " or ", "; ");
}

/**
 * Describe mostly non-numeric player-specific information about a spell.
 *
 * (E.g., your god's opinion of it, whether it's in a high-level book that
 * you can't memorise from, whether it's currently useless for whatever
 * reason...)
 *
 * @param spell     The spell in question.
 */
static string _player_spell_desc(spell_type spell)
{
    if (!crawl_state.need_save || (get_spell_flags(spell) & spflag::monster))
        return ""; // all info is player-dependent

    ostringstream description;

    description << "Miscasting this spell causes magic contamination"
                << (fail_severity(spell) ?
                    " and also " + _miscast_damage_string(spell) : "")
                << ".\n";

    if (spell == SPELL_BATTLESPHERE)
    {
        vector<spell_type> battlesphere_spells = player_battlesphere_spells();
        description << "Your battlesphere";
        if (battlesphere_spells.empty())
            description << " is not activated by any of your spells";
        else
        {
            description << " fires when you cast "
                        << comma_separated_fn(battlesphere_spells.begin(),
                                              battlesphere_spells.end(),
                                              spell_title,
                                              " or ");
        }
        description << ".\n";
    }

    if (spell == SPELL_SPELLFORGED_SERVITOR)
    {
        spell_type servitor_spell = player_servitor_spell();
        description << "Your servitor";
        if (servitor_spell == SPELL_NO_SPELL)
            description << " is unable to mimic any of your spells";
        else
            description << " casts " << spell_title(player_servitor_spell());
        description << ".\n";
    }
    else if (you.has_spell(SPELL_SPELLFORGED_SERVITOR) && spell_servitorable(spell))
    {
        if (failure_rate_to_int(raw_spell_fail(spell)) <= 20)
            description << "Your servitor can be imbued with this spell.\n";
        else
        {
            description << "Your servitor could be imbued with this spell if "
                           "your spell success rate were higher.\n";
        }
    }

    // Report summon cap
    const int limit = summons_limit(spell, true);
    if (limit)
    {
        description << "You can sustain at most " + number_in_words(limit)
                    // Attempt to clarify that flayed ghosts are NOT included in the cap
                    << (spell == SPELL_MARTYRS_KNELL ? " martyred shade" : " creature")
                    << (limit > 1 ? "s" : "")
                    << " summoned by this spell.\n";
    }

    if (god_hates_spell(spell, you.religion))
    {
        description << uppercase_first(god_name(you.religion))
                    << " frowns upon the use of this spell.\n";
    }
    else if (god_likes_spell(spell, you.religion))
    {
        description << uppercase_first(god_name(you.religion))
                    << " supports the use of this spell.\n";
    }

    if (!you_can_memorise(spell))
    {
        description << "\nYou cannot "
                    << (you.has_spell(spell) ? "cast" : "memorise")
                    << " this spell because "
                    << desc_cannot_memorise_reason(spell)
                    << "\n";
    }
    else if (casting_is_useless(spell, true))
    {
        // this preempts the more general uselessness call below, for the sake
        // of applying slightly different formatting.
        description << "\n<red>"
                    << uppercase_first(casting_uselessness_reason(spell, true))
                    << "</red>\n";
    }
    else if (spell_is_useless(spell, true, false))
    {
        description << "\nThis spell would have no effect right now because "
                    << spell_uselessness_reason(spell, true, false)
                    << "\n";
    }

    return description.str();
}


/**
 * Describe a spell, as cast by the player.
 *
 * @param spell     The spell in question.
 * @return          Information about the spell; does not include the title or
 *                  db description, but does include level, range, etc.
 */
string player_spell_desc(spell_type spell)
{
    return _player_spell_stats(spell) + _player_spell_desc(spell);
}

/**
 * Examine a given spell. Set the given string to its description, stats, &c.
 * If it's a book in a spell that the player is holding, mention the option to
 * memorise it.
 *
 * @param spell         The spell in question.
 * @param mon_owner     If this spell is being examined from a monster's
 *                      description, 'spell' is that monster. Else, null.
 * @param description   Set to the description & details of the spell.
 */
static void _get_spell_description(const spell_type spell,
                                  const monster_info *mon_owner,
                                  string &description)
{
    description.reserve(500);

    const string long_descrip = getLongDescription(string(spell_title(spell))
                                                   + " spell");

    if (!long_descrip.empty())
        description += long_descrip;
    else
    {
        description += "This spell has no description. "
                       "Casting it may therefore be unwise. "
#ifdef DEBUG
                       "Instead, go fix it. ";
#else
                       "Please file a bug report.";
#endif
    }

    if (mon_owner)
    {
        if (spell == SPELL_CONJURE_LIVING_SPELLS)
        {
            const spell_type living_spell = living_spell_type_for(mon_owner->type);
            description += make_stringf("\n%s creates living %s spells.\n",
                                        uppercase_first(mon_owner->full_name(DESC_A)).c_str(),
                                        spell_title(living_spell));
        }
        else if (spell == SPELL_QUICKSILVER_BOLT)
        {
            if (player_is_debuffable())
            {
                description += make_stringf("\nIf you are struck by this,"
                                            " you will no longer be %s.\n",
                                            describe_player_cancellation(true).c_str());
            }
            else
                description += "\nYou currently have no enchantments that could be"
                               " removed by this.\n";

        }

        const int hd = mon_owner->spell_hd();
        const int range = mons_spell_range_for_hd(spell, hd);
        description += "\nRange : ";
        if (spell == SPELL_CALL_DOWN_LIGHTNING)
            description += stringize_glyph(mons_char(mon_owner->type)) + "..---->";
        else if (spell == SPELL_FLASHING_BALESTRA)
            description += stringize_glyph(mons_char(mon_owner->type)) + "..-->";
        else
            description += range_string(range, range, mons_char(mon_owner->type));

        if (crawl_state.need_save && you_worship(GOD_DITHMENOS))
        {
            if (!valid_marionette_spell(spell))
            {
                description += "\n\n<magenta>This spell cannot be performed via "
                               "Aphotic Marionette.</magenta>\n";
            }
            else if (spell_has_marionette_override(spell))
            {
                description += "\n\n<magenta>When cast via Aphotic Marionette, "
                               "this spell will affect the player instead.";
            }
        }

        description += "\n";

        // Report summon cap
        const int limit = summons_limit(spell, false);
        if (limit)
        {
            description += make_stringf("%s can sustain at most %s creature%s "
                               "summoned by this spell.\n",
                               uppercase_first(mon_owner->full_name(DESC_THE)).c_str(),
                               number_in_words(limit).c_str(),
                               limit > 1 ? "s" : "");
        }

        // only display this if the player exists (not in the main menu)
        if (crawl_state.need_save && (get_spell_flags(spell) & spflag::WL_check)
#ifndef DEBUG_DIAGNOSTICS
            && mon_owner->attitude != ATT_FRIENDLY
#endif
            )
        {
            string wiz_info;
#ifdef WIZARD
            if (you.wizard)
                wiz_info += make_stringf(" (pow %d)", _hex_pow(spell, hd));
#endif
            description += you.immune_to_hex(spell)
                ? make_stringf("You cannot be affected by this "
                               "spell right now. %s\n",
                               wiz_info.c_str())
                : make_stringf("Chance to defeat your Will: %d%%%s\n",
                               hex_chance(spell, mon_owner),
                               wiz_info.c_str());
        }

    }
    else
        description += player_spell_desc(spell);

    const string quote = getQuoteString(string(spell_title(spell)) + " spell");
    if (!quote.empty())
        description += "\n" + quote;
}

/**
 * Provide the text description of a given spell.
 *
 * @param spell     The spell in question.
 * @param inf[out]  The spell's description is concatenated onto the end of
 *                  inf.body.
 */
#ifdef USE_TILE_LOCAL
void get_spell_desc(const spell_type spell, describe_info &inf)
{
    string desc;
    _get_spell_description(spell, nullptr, desc);
    inf.body << desc;
}
#endif

/**
 * Examine a given spell. List its description and details, and handle
 * memorising the spell in question, if the player is able & chooses to do so.
 *
 * @param spelled   The spell in question.
 * @param mon_owner If this spell is being examined from a monster's
 *                  description, 'mon_owner' is that monster. Else, null.
 */
void describe_spell(spell_type spell, const monster_info *mon_owner,
                    const item_def* item)
{
    UNUSED(item);

    string desc;
    _get_spell_description(spell, mon_owner, desc);

    auto vbox = make_shared<Box>(Widget::VERT);
#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
    auto spell_icon = make_shared<Image>();
    spell_icon->set_tile(tile_def(tileidx_spell(spell)));
    title_hbox->add_child(std::move(spell_icon));
#endif

    string spl_title = spell_title(spell);
    trim_string(desc);

    auto title = make_shared<Text>();
    title->set_text(spl_title);
    title->set_margin_for_sdl(0, 0, 0, 10);
    title_hbox->add_child(std::move(title));

    title_hbox->set_cross_alignment(Widget::CENTER);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    vbox->add_child(std::move(title_hbox));

    auto scroller = make_shared<Scroller>();
    auto text = make_shared<Text>();
    text->set_text(formatted_string::parse_string(desc));
    text->set_wrap_text(true);
    scroller->set_child(std::move(text));
    vbox->add_child(scroller);

    auto popup = make_shared<ui::Popup>(std::move(vbox));

    bool done = false;
    int lastch;
    popup->on_keydown_event([&](const KeyEvent& ev) {
        lastch = ev.key();
        done = ui::key_exits_popup(lastch, true);
        if (scroller->on_event(ev))
            return true;
        return done;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    auto tile = tile_def(tileidx_spell(spell));
    tiles.json_open_object("tile");
    tiles.json_write_int("t", tile.tile);
    tiles.json_write_int("tex", get_tile_texture(tile.tile));
    if (tile.ymax != TILE_Y)
        tiles.json_write_int("ymax", tile.ymax);
    tiles.json_close_object();
    tiles.json_write_string("title", spl_title);
    tiles.json_write_string("desc", desc);
    tiles.push_ui_layout("describe-spell", 0);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    ui::run_layout(std::move(popup), done);
}

/**
 * Examine a given ability. List its description and details.
 *
 * @param ability   The ability in question.
 */
void describe_ability(ability_type ability)
{
    describe_info inf;
    inf.title = ability_name(ability);
    inf.body << get_ability_desc(ability, false);
    tile_def tile = tile_def(tileidx_ability(ability));
    show_description(inf, &tile);
}

/**
 * Examine a given deck.
 */
void describe_deck(deck_type deck)
{
    describe_info inf;

    if (deck == DECK_STACK)
        inf.title = "A stacked deck";
    else
        inf.title = "The " + deck_name(deck);

    inf.body << deck_description(deck);

    show_description(inf);
}

/**
 * Describe a given mutation. List its description and details.
 */
void describe_mutation(mutation_type mut)
{
    describe_info inf;
    // TODO: mutation_name was not designed for use as a title, sometimes has
    // the wrong casing, is often cryptic
    inf.title = uppercase_first(mutation_name(mut)).c_str();

    const bool has_mutation = you.has_mutation(mut);
    if (has_mutation && mutation_max_levels(mut) > 1)
    {
        inf.title += make_stringf(" (level %d/%d)",
                                  you.get_mutation_level(mut),
                                  mutation_max_levels(mut));
    }
    inf.body << get_mutation_desc(mut);

    const tileidx_t base_tile_idx = get_mutation_tile(mut);
    if (base_tile_idx)
    {
        const int offset = (has_mutation
                            ? you.get_mutation_level(mut, false)
                            : mutation_max_levels(mut))
                           - 1;
        const tile_def tile = tile_def(base_tile_idx + offset);
        show_description(inf, &tile);
    }
    else
        show_description(inf);
}

static string _describe_draconian(const monster_info& mi)
{
    string description;
    const int subsp = mi.draconian_subspecies();

    if (subsp != mi.type)
    {
        description += "It has ";

        switch (subsp)
        {
        case MONS_BLACK_DRACONIAN:      description += "black ";   break;
        case MONS_YELLOW_DRACONIAN:     description += "yellow ";  break;
        case MONS_GREEN_DRACONIAN:      description += "green ";   break;
        case MONS_PURPLE_DRACONIAN:     description += "purple ";  break;
        case MONS_RED_DRACONIAN:        description += "red ";     break;
        case MONS_WHITE_DRACONIAN:      description += "white ";   break;
        case MONS_GREY_DRACONIAN:       description += "grey ";    break;
        case MONS_PALE_DRACONIAN:       description += "pale ";    break;
        default:
            break;
        }

        description += "scales. ";
    }

    switch (subsp)
    {
    case MONS_BLACK_DRACONIAN:
        description += "Sparks flare out of its mouth and nostrils.";
        break;
    case MONS_YELLOW_DRACONIAN:
        description += "Acidic fumes swirl around it.";
        break;
    case MONS_GREEN_DRACONIAN:
        description += "Venom drips from its jaws.";
        break;
    case MONS_PURPLE_DRACONIAN:
        description += "Its outline shimmers with magical energy.";
        break;
    case MONS_RED_DRACONIAN:
        description += "Smoke pours from its nostrils.";
        break;
    case MONS_WHITE_DRACONIAN:
        description += "Frost pours from its nostrils.";
        break;
    case MONS_GREY_DRACONIAN:
        description += "Its scales and tail are adapted to the water.";
        break;
    case MONS_PALE_DRACONIAN:
        description += "It is cloaked in a pall of superheated steam.";
        break;
    default:
        break;
    }

    return description;
}

static const char* _get_resist_name(mon_resist_flags res_type)
{
    switch (res_type)
    {
    case MR_RES_ELEC:
        return "electricity";
    case MR_RES_POISON:
        return "poison";
    case MR_RES_FIRE:
        return "fire";
    case MR_RES_STEAM:
        return "steam";
    case MR_RES_COLD:
        return "cold";
    case MR_RES_ACID:
        return "acid";
    case MR_RES_MIASMA:
        return "miasma";
    case MR_RES_NEG:
        return "negative energy";
    case MR_RES_DAMNATION:
        return "damnation";
    case MR_RES_TORMENT:
        return "torment";
    default:
        return "buggy resistance";
    }
}

static const char* _get_threat_desc(mon_threat_level_type threat)
{
    switch (threat)
    {
    case MTHRT_UNDEF: // ?
    case MTHRT_TRIVIAL: return "Minor";
    case MTHRT_EASY:    return "Low";
    case MTHRT_TOUGH:   return "High";
    case MTHRT_NASTY:   return "Lethal";
    default:            return "Eggstreme";
    }
}

static const char* _get_int_desc(mon_intel_type intel)
{
    switch (intel)
    {
    case I_BRAINLESS:   return "Mindless";
    case I_ANIMAL:      return "Animal";
    case I_HUMAN:       return "Human";
    default:            return "Eggplantelligent";
    }
}

static string _flavour_base_desc(attack_flavour flavour)
{
    static const map<attack_flavour, string> base_descs = {
        { AF_ACID,              "acid damage"},
        { AF_REACH_TONGUE,      "acid damage" },
        { AF_BLINK,             "blink self" },
        { AF_BLINK_WITH,        "blink together with the defender" },
        { AF_COLD,              "cold damage" },
        { AF_CONFUSE,           "cause confusion" },
        { AF_DRAIN_STR,         "drain strength" },
        { AF_DRAIN_INT,         "drain intelligence" },
        { AF_DRAIN_DEX,         "drain dexterity" },
        { AF_DRAIN_STAT,        "drain strength, intelligence or dexterity" },
        { AF_DRAIN,             "drain life" },
        { AF_VAMPIRIC,          "drain health from the living" },
        { AF_DRAIN_SPEED,       "drain speed" },
        { AF_ANTIMAGIC,         "drain magic" },
        { AF_SCARAB,            "drain speed and health" },
        { AF_ELEC,              "electric damage" },
        { AF_FIRE,              "fire damage" },
        { AF_SEAR,              "remove fire resistance" },
        { AF_MINIPARA,          "poison and momentary paralysis" },
        { AF_POISON_PARALYSE,   "poison and paralysis/slowing" },
        { AF_POISON,            "poison" },
        { AF_REACH_STING,       "poison" },
        { AF_POISON_STRONG,     "strong poison" },
        { AF_DISTORT,           "distortion" },
        { AF_RIFT,              "distortion" },
        { AF_RAGE,              "drive defenders berserk" },
        { AF_CHAOTIC,           "chaos" },
        { AF_STEAL,             "steal items" },
        { AF_CRUSH,             "begin ongoing constriction" },
        { AF_REACH,             "" },
        { AF_HOLY,              "extra damage to undead/demons" },
        { AF_PAIN,              "extra pain damage to the living" },
        { AF_ENSNARE,           "ensnare with webbing" },
        { AF_ENGULF,            "engulf" },
        { AF_PURE_FIRE,         "" },
        { AF_VULN,              "reduce willpower" },
        { AF_SHADOWSTAB,        "increased damage when unseen" },
        { AF_DROWN,             "drowning damage" },
        { AF_CORRODE,           "cause corrosion" },
        { AF_TRAMPLE,           "knock back the defender" },
        { AF_WEAKNESS,          "cause weakness" },
        { AF_BARBS,             "embed barbs" },
        { AF_SPIDER,            "summon a spider" },
        { AF_BLOODZERK,         "become enraged on drawing blood" },
        { AF_SLEEP,             "induce sleep" },
        { AF_SWOOP,             "swoops behind the defender beforehand" },
        { AF_FLANK,             "slips behind the defender beforehand" },
        { AF_DRAG,              "drag the defender backwards"},
        { AF_FOUL_FLAME,        "extra damage, especially to the good" },
        { AF_HELL_HUNT,         "summon demonic beasts" },
        { AF_SWARM,             "summon more of itself" },
        { AF_PLAIN,             "" },
    };

    const string* desc = map_find(base_descs, flavour);
    ASSERT(desc);
    return *desc;
}

struct mon_attack_info
{
    mon_attack_def definition;
    const item_def* weapon;
    bool operator < (const mon_attack_info &other) const
    {
        return std::tie(definition.type, definition.flavour,
                        definition.damage, weapon)
             < std::tie(other.definition.type, other.definition.flavour,
                        other.definition.damage, other.weapon);
    }
};

/**
 * What weapon is the given monster using for the given attack, if any?
 *
 * @param mi        The monster in question.
 * @param atk       The attack number. (E.g. 0, 1, 2...)
 * @return          The melee weapon being used by the monster for the given
 *                  attack, if any.
 */
static const item_def* _weapon_for_attack(const monster_info& mi, int atk)
{
    // XXX: duplicates monster::weapon()
    if ((atk % 2) && mi.wields_two_weapons())
    {
        item_def *alt_weap = mi.inv[MSLOT_ALT_WEAPON].get();
        if (alt_weap && is_weapon(*alt_weap))
            return alt_weap;
    }
    item_def* weapon = mi.inv[MSLOT_WEAPON].get();
    if (weapon && is_weapon(*weapon))
        return weapon;
    return nullptr;
}

static mon_attack_info _atk_info(const monster_info& mi, int i)
{
    const mon_attack_def &attack = mi.attack[i];
    const item_def* weapon = nullptr;
    // XXX: duplicates monster::weapon()
    if (attack.type == AT_HIT || attack.type == AT_WEAP_ONLY)
        weapon = _weapon_for_attack(mi, i);
    mon_attack_info attack_info = { attack, weapon };
    return attack_info;
}

// Return a string describing the maximum damage from a monster's weapon brand
static string _brand_damage_string(const monster_info &mi, brand_type brand,
                                   int dam)
{
    const char * name = brand_type_name(brand, true);
    int brand_dam;
    // Only include damaging brands
    // Heavy is included in base damage calculations instead
    switch (brand)
    {
        case SPWPN_FLAMING:
        case SPWPN_FREEZING:
        case SPWPN_DRAINING:
            brand_dam = dam / 2;
            break;
        case SPWPN_ELECTROCUTION:
            brand_dam = 20;
            break;
        case SPWPN_DISTORTION:
            brand_dam = 26;
            break;
        case SPWPN_HOLY_WRATH:
            // Hopefully this isn't too confusing for non-holy-vuln players
            brand_dam = dam * 15 / 10;
            break;
        case SPWPN_FOUL_FLAME:
            brand_dam = dam * 0.75;
            break;
        case SPWPN_PAIN:
            brand_dam = mi.has_necromancy_spell() ? mi.hd : mi.hd / 2;
            break;
        case SPWPN_VENOM:
        case SPWPN_ANTIMAGIC:
        case SPWPN_CHAOS:
            return make_stringf(" + %s", name);
        default:
            return "";
    }

    return make_stringf(" + %d (%s)", brand_dam, name);
}

// Return a monster's slaying bonus (not including weapon enchantment)
static int _monster_slaying(const monster_info& mi)
{
    int slaying = 0;
    const artefact_prop_type artp = ARTP_SLAYING;

    // Largely a duplication of monster::scan_artefacts,
    // but there's no equivalent for monster_info :(
    const item_def *weapon       = mi.inv[MSLOT_WEAPON].get();
    const item_def *other_weapon = mi.inv[MSLOT_ALT_WEAPON].get();
    const item_def *armour       = mi.inv[MSLOT_ARMOUR].get();
    const item_def *shield       = mi.inv[MSLOT_SHIELD].get();
    const item_def *jewellery    = mi.inv[MSLOT_JEWELLERY].get();

    if (jewellery && jewellery->base_type == OBJ_JEWELLERY)
    {
        if (jewellery->is_type(OBJ_JEWELLERY, RING_SLAYING))
            slaying += jewellery->plus;
        if (is_artefact(*jewellery))
            slaying += artefact_property(*jewellery, artp);
    }

    if (weapon && weapon->base_type == OBJ_WEAPONS && is_artefact(*weapon))
        slaying += artefact_property(*weapon, artp);

    if (other_weapon && other_weapon->base_type == OBJ_WEAPONS
        && is_artefact(*other_weapon) && mi.wields_two_weapons())
    {
        slaying += artefact_property(*other_weapon, artp);
    }

    if (armour && armour->base_type == OBJ_ARMOUR && is_artefact(*armour))
        slaying += artefact_property(*armour, artp);

    if (shield && shield->base_type == OBJ_ARMOUR && is_artefact(*shield))
        slaying += artefact_property(*shield, artp);

    return slaying;
}

// Max damage from a magical staff with a given amount of staff & evo skill
static int _staff_max_damage(stave_type staff, int staff_skill, int evo_skill)
{
    return (2 * staff_skill + evo_skill) * staff_damage_mult(staff) / 80 - 1;
}

// Describe the damage from a monster's magical staff
static string _monster_staff_damage_string(const monster_info &mi,
                                           stave_type staff)
{
    // From monster::skill
    const int evo_skill = mi.hd;
    int staff_skill;
    if (staff == STAFF_DEATH)
        staff_skill = mi.has_necromancy_spell() ? mi.hd : mi.hd / 2;
    else
        staff_skill = mi.is_actual_spellcaster() ? mi.hd : mi.hd / 3;

    // "earth" tries to communicate the damage reduction when flying
    // XXX "conj" isn't a damage type, but we want to communicate
    // that the damage is flat staff bonus damage somehow.
    string dam_type_string = staff == STAFF_FIRE          ? "fire"
                           : staff == STAFF_COLD          ? "cold"
                           : staff == STAFF_AIR           ? "elec"
                           : staff == STAFF_EARTH         ? "earth"
                           : staff == STAFF_DEATH         ? "drain"
                           : staff == STAFF_ALCHEMY       ? "poison"
                           /*staff == STAFF_CONJURATION*/ : "conj";

    return make_stringf(" + %d (%s)",
                        _staff_max_damage(staff, staff_skill, evo_skill),
                        dam_type_string.c_str());
}

static string _monster_attacks_description(const monster_info& mi)
{
    // Spectral weapons use the wielder's stats to attack, so displaying
    // their 'monster' damage here is just misleading.
    // TODO: display the right number without an awful hack
    if (mi.type == MONS_SPECTRAL_WEAPON)
        return "";

    ostringstream result;
    map<mon_attack_info, int> attack_counts;
    brand_type special_flavour = SPWPN_NORMAL;

    if (mi.props.exists(SPECIAL_WEAPON_KEY))
    {
        ASSERT(mi.type == MONS_PANDEMONIUM_LORD || mons_is_pghost(mi.type));
        special_flavour = (brand_type) mi.props[SPECIAL_WEAPON_KEY].get_int();
    }

    bool has_any_flavour = special_flavour != SPWPN_NORMAL;
    bool flavour_without_dam = special_flavour != SPWPN_NORMAL;
    bool plural = false;
    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
    {
        mon_attack_info attack_info = _atk_info(mi, i);
        const mon_attack_def &attack = attack_info.definition;
        if (attack.type == AT_NONE)
            break; // assumes there are no gaps in attack arrays

        // Multi-headed monsters must always have their multi-attack in the
        // first slot.
        if ((mons_genus(mi.base_type) == MONS_HYDRA
             || mons_species(mi.base_type) == MONS_SERPENT_OF_HELL)
            && i == 0)
        {
            attack_counts[attack_info] = mi.num_heads;
        }
        else
            ++attack_counts[attack_info];

        if (i > 0)
            plural = true;

        const item_def *quiv = mi.inv[MSLOT_MISSILE].get();
        if (quiv && quiv->base_type == OBJ_MISSILES)
        {
            plural = true;
            if (quiv->sub_type == MI_DART || quiv->sub_type == MI_THROWING_NET)
            {
                has_any_flavour = true;
                flavour_without_dam = true;
            }
        }

        if (attack.flavour == AF_PLAIN || attack.flavour == AF_PURE_FIRE)
            continue;

        has_any_flavour = true;
        const bool needs_dam = !flavour_triggers_damageless(attack.flavour)
                            && !flavour_has_mobility(attack.flavour)
                            && !flavour_has_reach(attack.flavour);
        if (!needs_dam && attack.flavour != AF_REACH_TONGUE)
            flavour_without_dam = true;
    }

    if (attack_counts.empty())
        return "";

    _describe_mons_to_hit(mi, result);

    result << "\n";

    size_t attk_desc_width = 7;                    // length of "Attacks"
    size_t damage_width   = 10;                    // length of "Max Damage"
    size_t bonus_width = !has_any_flavour ? 0      // no bonus column
                       : flavour_without_dam ? 5   // length of "Bonus"
                                             : 19; //  of "After Damaging Hits"
    vector<string> attack_descriptions;
    vector<string> damage_descriptions;
    vector<string> bonus_descriptions;

    // Get all the info that will form the table of attacks
    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
    {
        const mon_attack_info info = _atk_info(mi, i);
        const mon_attack_def &attack = info.definition;

        int attk_mult = attack_counts[info];
        if (!attk_mult) // we're done
            break;
        attack_counts[info] = 0;

        // Part 1: The "Attacks" column
        // Display the name of the attack, along with any weapon used with it

        if (weapon_multihits(info.weapon))
            attk_mult *= weapon_hits_per_swing(*info.weapon);
        string attk_name = uppercase_first(mon_attack_name_short(attack.type));
        if (info.weapon && is_range_weapon(*info.weapon))
            attk_name = "Shoot";
        string weapon_descriptor = "";
        if (info.weapon)
            weapon_descriptor = ": " + info.weapon->name(DESC_PLAIN, true, true, false);
        string attk_desc = make_stringf("%s%s", attk_name.c_str(),
                                        weapon_descriptor.c_str());
        if (attk_mult > 1)
        {
            attk_desc = make_stringf("%dx %s%s", attk_mult, attk_name.c_str(),
                                     weapon_descriptor.c_str());
        }
        attack_descriptions.emplace_back(attk_desc);
        attk_desc_width = max(attk_desc_width, attk_desc.length());

        // Part 2: The "Max Damage" column
        // Display the max damage from the attack (including any weapon)
        // and additionally display max brand damage separately

        const int flav_dam = flavour_damage(attack.flavour, mi.hd, false);

        int dam = attack.damage;
        int slaying = _monster_slaying(mi);

        if (attack.flavour == AF_PURE_FIRE)
            dam = flav_dam;
        else if (attack.flavour == AF_CRUSH)
            dam = 0;
        else if (info.weapon)
        {
            // From attack::calc_damage
            // damage = 1 + random2(monster attack damage)
            //          + random2(weapon damage) + random2(1 + enchant + slay)
            const int base_dam = property(*info.weapon, PWPN_DAMAGE);
            dam += brand_adjust_weapon_damage(base_dam, get_weapon_brand(*info.weapon), false) - 1;
            if (is_range_weapon(*info.weapon) && mons_class_flag(mi.type, M_ARCHER))
                dam += archer_bonus_damage(mi.hd);
            slaying += info.weapon->plus;
        }
        dam += max(slaying, 0);

        // Show damage modified by effects, if applicable
        int real_dam = dam;
        if (mi.is(MB_STRONG) || mi.is(MB_BERSERK))
            real_dam = real_dam * 3 / 2;
        if (mi.is(MB_IDEALISED))
            real_dam = real_dam * 2;
        if (mi.is(MB_WEAK))
            real_dam = real_dam * 2 / 3;
        if (mi.is(MB_TOUCH_OF_BEOGH))
            real_dam = real_dam * 4 / 3;

        string dam_str;
        if (dam != real_dam)
            dam_str = make_stringf("%d (base %d)", real_dam, dam);
        else
            dam_str = make_stringf("%d", dam);

        if (attack.flavour == AF_PURE_FIRE)
            dam_str += " fire";

        string brand_str;
        if (info.weapon)
        {
            if (info.weapon->base_type == OBJ_WEAPONS)
            {
                brand_str = _brand_damage_string(mi,
                                                 get_weapon_brand(*info.weapon),
                                                 dam);
            }
            else if (info.weapon->base_type == OBJ_STAVES)
                brand_str = _monster_staff_damage_string(mi,
                                static_cast<stave_type>(info.weapon->sub_type));
        }

        string final_dam_str = make_stringf("%s%s%s", dam_str.c_str(),
                                            brand_str.c_str(),
                                            attk_mult > 1 ? " each" : "");
        damage_descriptions.emplace_back(final_dam_str);
        damage_width = max(damage_width, final_dam_str.size());

        // Part 3: The "Bonus" column
        // Describe any additional effects from a monster's attack flavour

        if (special_flavour != SPWPN_NORMAL)
        {
            // TODO Merge this with weapon brand handling above
            bonus_descriptions.emplace_back(_desc_ghost_brand(special_flavour));
            bonus_width = max(bonus_width, _desc_ghost_brand(special_flavour).size());
            continue;
        }

        string bonus_desc = uppercase_first(_flavour_base_desc(attack.flavour));
        if (flav_dam && attack.flavour != AF_PURE_FIRE)
        {
            bonus_desc += make_stringf(" (max %d%s)",
                                       flav_dam,
                                       attk_mult > 1 ? " each" : "");
        }
        else if (attack.flavour == AF_DRAIN)
            bonus_desc += make_stringf(" (max %d damage)", real_dam / 2);
        else if (attack.flavour == AF_CRUSH)
        {
            bonus_desc += make_stringf(" (%d-%d dam)", attack.damage,
                                       attack.damage*2);
        }

        if (flavour_without_dam
            && !bonus_desc.empty()
            && !flavour_triggers_damageless(attack.flavour)
            && !flavour_has_mobility(attack.flavour))
        {
            bonus_desc += " (if damage dealt)";
        }

        // Nessos' ranged weapon attacks apply venom as a special effect
        if (mi.type == MONS_NESSOS
            && info.weapon && is_range_weapon(*info.weapon))
        {
            bonus_desc += make_stringf("Poison");
            has_any_flavour = true;
            flavour_without_dam = true;
        }

        if (flavour_has_reach(attack.flavour))
        {
            bonus_desc += (bonus_desc.empty() ? "Reaches" : "; reaches");
            bonus_desc += (attack.flavour == AF_RIFT ? " very far" : " from afar");
        }

        bonus_descriptions.emplace_back(bonus_desc);
        bonus_width = max(bonus_width, bonus_desc.size());
    }

    // Check for throwing weapons
    item_def *quiv = mi.inv[MSLOT_MISSILE].get();
    if (quiv && quiv->base_type == OBJ_MISSILES)
    {
        string throw_str = "Throw: ";
        if (quiv->is_type(OBJ_MISSILES, MI_THROWING_NET))
            throw_str += quiv->name(DESC_A, false, false, true, false);
        else
            throw_str += pluralise(quiv->name(DESC_PLAIN, false, false,
                                              true, false));
        attack_descriptions.emplace_back(throw_str);
        attk_desc_width = max(attk_desc_width, throw_str.size());

        string dam_desc = "0";
        string bonus_desc = "";
        if (quiv->sub_type == MI_THROWING_NET)
        {
            bonus_desc = "Ensnare in a net";
            has_any_flavour = true;
            flavour_without_dam = true;
        }
        else if (quiv->sub_type == MI_DART)
        {
            has_any_flavour = true;
            flavour_without_dam = true;
            switch (quiv->brand)
            {
            case SPMSL_CURARE:
                dam_desc = "12 (curare)"; // direct curare damage is 2d6
                bonus_desc = "Poison and slowing";
                break;
            case SPMSL_POISONED:
                bonus_desc = "Poison";
                break;
            case SPMSL_BLINDING:
                bonus_desc = "Blinding and confusion";
                break;
            case SPMSL_FRENZY:
                bonus_desc = "Drive defenders into a frenzy";
                break;
            case SPMSL_DISPERSAL:
                bonus_desc = "Blink the defender away";
                break;
            default:
                break;
            }
        }
        else // ordinary missiles
        {
            // Missile attacks use the damage number of a monster's first attack
            const mon_attack_info info = _atk_info(mi, 0);
            const mon_attack_def &attack = info.definition;
            int dam = attack.damage;
            dam += property(*quiv, PWPN_DAMAGE) - 1;
            dam += max(_monster_slaying(mi), 0);
            if (mons_class_flag(mi.type, M_ARCHER))
                dam += archer_bonus_damage(mi.hd);
            string silver_str;
            if (quiv->brand == SPMSL_SILVER)
            {
                string dmg_msg;
                int silver_dam = max(dam / 3,
                                     silver_damages_victim(&you, dam, dmg_msg));
                silver_str = make_stringf(" + %d (silver)", silver_dam);
            }
            dam_desc = make_stringf("%d%s", dam, silver_str.c_str());
        }

        damage_descriptions.emplace_back(dam_desc);
        bonus_descriptions.emplace_back(bonus_desc);
        damage_width = max(damage_width, dam_desc.size());
        bonus_width = max(bonus_width, bonus_desc.size());
    }

    // Hopefully enough for every possibility
    damage_width    = min(damage_width, (size_t) 31);
    bonus_width     = min(bonus_width, 69 - damage_width);

    // Table lines can't be longer than 80 chars wide (incl 4 spaces)
    // so cut off the attack description if it's too long.
    // Note: minimum 7 (length of "Attacks")
    attk_desc_width = min(attk_desc_width, 76 - damage_width - bonus_width);

    // Now we can actually build the table of attacks
    // Note: columns are separated by (a minimum of) 2 spaces

    // First, the table header
    result << padded_str(plural ? "Attacks" : "Attack", attk_desc_width + 2)
           << padded_str("Max Damage", damage_width + 2);
    if (has_any_flavour)
    {
        result << padded_str(flavour_without_dam ? "Bonus"
                                                 : "After Damaging Hits",
                             bonus_width);
    }
    result << "\n";
    // Table body
    for (unsigned int i = 0; i < attack_descriptions.size(); ++i)
    {
        if (attack_descriptions[i] == "")
            break;

        result << chop_string(attack_descriptions[i], attk_desc_width)
               << "  "
               << chop_string(damage_descriptions[i], damage_width)
               << "  "
               << chop_string(bonus_descriptions[i], bonus_width)
               << "\n";
    }

    result << "\n";
    return result.str();
}

#define SPELL_LIST_BEGIN "[SPELL_LIST_BEGIN]"
#define SPELL_LIST_END "[SPELL_LIST_END]"

static string _monster_spells_description(const monster_info& mi, bool mark_spells)
{
    // Show monster spells and spell-like abilities.
    if (!mi.has_spells())
        return "";

    formatted_string description;

    // hacky, refactor so this isn't necessary
    if (mark_spells)
        description += SPELL_LIST_BEGIN;
    describe_spellset(monster_spellset(mi), nullptr, description, &mi);
    if (mark_spells)
        description += SPELL_LIST_END;

    description.cprintf("\nTo read a description, press the key listed above. "
        "(AdB) indicates damage (the sum of A B-sided dice), "
        "(x%%) indicates the chance to defeat your Will, "
        "and (y) indicates the spell range");
    description.cprintf(crawl_state.need_save
        ? "; shown in red if you are in range.\n"
        : ".\n");

    return description.to_colour_string();
}

/**
 * Display the % chance of a player hitting the given monster.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
void describe_to_hit(const monster_info& mi, ostringstream &result,
                     const item_def* weapon)
{
    if (weapon != nullptr && !is_weapon(*weapon))
        return; // breadwielding

    const bool melee = weapon == nullptr || !is_range_weapon(*weapon);
    int acc_pct;
    if (melee)
    {
        melee_attack attk(&you, nullptr);
        acc_pct = to_hit_pct(mi, attk, true);
    }
    else
    {
        // TODO: handle throwing to-hit somehow?
        item_def fake_proj;
        populate_fake_projectile(*weapon, fake_proj);
        ranged_attack attk(&you, nullptr, weapon, &fake_proj, false);
        acc_pct = to_hit_pct(mi, attk, false);
    }

    result << "about " << (100 - acc_pct) << "% to evade ";
    if (weapon == nullptr)
        result << "your " << you.hand_name(true);
    else
        result << weapon->name(DESC_YOUR, false, false, false);
}

static bool _visible_to(const monster_info& mi)
{
    // XXX: this duplicates player::visible_to
    const bool invis_to = you.invisible() && !mi.can_see_invisible()
                          && !you.in_water();
    return mi.attitude == ATT_FRIENDLY || (!mi.is(MB_BLIND) && !invis_to);
}

/**
 * Display the % chance of a the given monster hitting the player.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_mons_to_hit(const monster_info& mi, ostringstream &result)
{
    if (crawl_state.game_is_arena() || !crawl_state.need_save)
        return;

    const item_def* weapon = mi.inv[MSLOT_WEAPON].get();
    const bool melee = weapon == nullptr || !is_range_weapon(*weapon);
    const bool skilled = mons_class_flag(mi.type, melee ? M_FIGHTER : M_ARCHER);
    const int base_to_hit = mon_to_hit_base(mi.hd, skilled);
    const int weapon_to_hit = weapon ? weapon->plus + property(*weapon, PWPN_HIT) : 0;
    const int total_base_hit = base_to_hit + weapon_to_hit;

    int post_roll_modifiers = 0;
    if (mi.is(MB_CONFUSED))
        post_roll_modifiers += CONFUSION_TO_HIT_MALUS;

    const bool invisible = !_visible_to(mi);
    if (invisible)
        post_roll_modifiers -= total_base_hit * 35 / 100;
    else
    {
        post_roll_modifiers += TRANSLUCENT_SKIN_TO_HIT_MALUS
                               * you.get_mutation_level(MUT_TRANSLUCENT_SKIN);
        if (you.backlit(false))
            post_roll_modifiers += BACKLIGHT_TO_HIT_BONUS;
        if (you.umbra() && !mi.nightvision())
            post_roll_modifiers += UMBRA_TO_HIT_MALUS;
    }
    // We ignore pproj because monsters never have it passively.

    // We ignore the EV penalty for not being able to see an enemy because, if you
    // can't see an enemy, you can't get a monster description for them. (Except through
    // ?/M, but let's neglect that for now.)
    const int scaled_ev = you.evasion_scaled(100);

    const int to_land = weapon && is_unrandom_artefact(*weapon, UNRAND_SNIPER) ? AUTOMATIC_HIT :
                                                                total_base_hit + post_roll_modifiers;
    const int beat_ev_chance = mon_to_hit_pct(to_land, scaled_ev);

    const int scaled_sh = player_shield_class(100, false);
    const int shield_bypass = mon_shield_bypass(mi.hd);
    // ignore penalty for unseen attacker, as with EV above
    const int beat_sh_chance = mon_beat_sh_pct(shield_bypass, scaled_sh);

    const int hit_chance = beat_ev_chance * beat_sh_chance / 100;
    result << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE)) << " "
           << conjugate_verb("have", mi.pronoun_plurality())
           << " about " << hit_chance << "% to hit you.\n";
}

/**
 * Print a bar of +s representing a given stat to a provided stream.
 *
 * @param value[in]         The current value represented by the bar. Clamped
 *                          to zero.
 * @param scale[in]         The value that each + represents.
 * @param name              The name of the bar.
 * @param result[in,out]    The stringstream to append to.
 * @param base_value[in]    The 'base' value represented by the bar, clamped to
 *                          zero. If INT_MAX, is ignored.
 */
static void _print_bar(int value, int scale, const string &name,
                       ostringstream &result, int base_value)
{
    value = max(0, value);
    base_value = max(0, base_value);

    if (base_value == INT_MAX)
        base_value = value;

    if (!name.empty())
        result << name << " ";

    const int display_max = value ? value : base_value;
    const bool currently_disabled = !value && base_value;

    if (currently_disabled)
      result << "none (normally ";

    if (display_max == 0)
        result << "none";
    else
    {
        for (int i = 0; i * scale < display_max; i++)
        {
            result << "+";
            if (i % 5 == 4)
                result << " ";
        }
    }

    if (currently_disabled)
        result << ")";

#ifdef DEBUG_DIAGNOSTICS
    if (!you.suppress_wizard)
    {
        result << " (" << value << ")";
        if (currently_disabled)
            result << " (base: " << base_value << ")";
    }
#endif
}

static string _build_bar(int value, int scale)
{
    // Round up.
    const int pips = (value + scale - 1) / scale;
    if (pips <= 0)
        return "none";
    if (pips > 8) // too many..
        return make_stringf("~%d", pips * scale);

    string bar = "";
    bar.append(min(pips, 4), '+');
    // Add a space in the middle of long bars,
    // for readability.
    if (pips > 4) {
        bar += " ";
        bar.append(min(pips - 4, 4), '+');
    }
    return bar;
}

/**
 * Returns a description of a given monster's WL.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 */
static string _describe_monster_wl(const monster_info& mi)
{
    const int will = mi.willpower();
    if (will == WILL_INVULN)
    {
        if (Options.char_set == CSET_ASCII)
            return "inf";
        return "∞";
    }
    return _build_bar(will, WL_PIP);
}

/**
 * Returns a string describing a given monster's habitat.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @return                  The habitat description.
 */
string _monster_habitat_description(const monster_info& mi)
{
    const monster_type type = mons_is_draconian_job(mi.type)
                              ? mi.draconian_subspecies()
                              : mi.type;
    if (mons_class_is_stationary(type))
        return "";

    switch (mons_habitat_type(type, mi.base_type))
    {
    case HT_AMPHIBIOUS:
        return uppercase_first(make_stringf("%s can %s water.\n",
                               mi.pronoun(PRONOUN_SUBJECTIVE),
                               mi.type == MONS_ORC_APOSTLE
                                ? "walk on" : "travel through"));
    case HT_AMPHIBIOUS_LAVA:
        return uppercase_first(make_stringf("%s can travel through lava.\n",
                               mi.pronoun(PRONOUN_SUBJECTIVE)));
    default:
        return "";
    }
}

// Size adjectives
const char* const size_adj[] =
{
    "tiny",
    "very small",
    "small",
    "medium",
    "large",
    "giant",
};
COMPILE_CHECK(ARRAYSZ(size_adj) == NUM_SIZE_LEVELS);

// This is used in monster description and on '%' screen for player size
const char* get_size_adj(const size_type size, bool ignore_medium)
{
    ASSERT_RANGE(size, 0, ARRAYSZ(size_adj));
    if (ignore_medium && size == SIZE_MEDIUM)
        return nullptr; // don't mention medium size
    return size_adj[size];
}

static string _monster_current_target_description(const monster_info &mi)
{
    // is it morally wrong to use pos to get the actual monster? Possibly...
    if (!in_bounds(mi.pos) || !monster_at(mi.pos))
        return "";
    const monster *m = monster_at(mi.pos);

    const char* pronoun = mi.pronoun(PRONOUN_SUBJECTIVE);
    const bool plural = mi.pronoun_plurality();

    ostringstream result;
    if (mi.is(MB_ALLY_TARGET))
    {
        auto allies = find_allies_targeting(*m);
        if (allies.size() == 1)
        {
            result << uppercase_first(pronoun) << " "
                   << conjugate_verb("are", plural)
                   << " currently targeted by "
                   << allies[0]->name(DESC_YOUR) << ".\n";
        }
        else
        {
            result << uppercase_first(pronoun) << " "
                   << conjugate_verb("are", plural)
                   << " currently targeted by:\n";
            for (auto *a : allies)
                result << "  " << a->name(DESC_YOUR) << "\n";
        }
    }

    // TODO: this might be ambiguous, give a relative position?
    if (mi.attitude == ATT_FRIENDLY && m->get_foe())
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural)
               << " currently targeting "
               << m->get_foe()->name(DESC_THE) << ".\n";
    }

    return result.str();
}

struct TableCell
{
    string   label;
    string   value;
    colour_t colour;
};

// TODO: This is similar to column_composer. Deduplicate?
class TablePrinter
{
private:
    vector<vector<TableCell>> rows;

public:
    void AddRow()
    {
        rows.push_back({});
    }

    void AddCell(string label = "", string value = "", colour_t colour = LIGHTGREY)
    {
        rows[rows.size() - 1].push_back({label, value, colour});
    }

    void Print(ostringstream &result)
    {
        vector<int> labels_lengths_by_col;
        for (size_t row = 0; row < rows.size(); ++row)
        {
            for (size_t col = 0; col < rows[row].size(); ++col)
            {
                const int label_len = codepoints(rows[row][col].label);
                if (col == labels_lengths_by_col.size())
                    labels_lengths_by_col.push_back(label_len);
                else
                    labels_lengths_by_col[col] = max(labels_lengths_by_col[col], label_len);
            }
        }
        const int cell_len = 80 / labels_lengths_by_col.size();

        for (const auto &row : rows)
        {
            for (size_t col = 0; col < row.size(); ++col)
            {
                const TableCell &cell = row[col];
                if (cell.label.empty())  // padding
                {
                    result << string(cell_len, ' ');
                    continue;
                }

                const int label_len = labels_lengths_by_col[col];
                const string label = padded_str(cell.label, label_len, true);
                const string body = make_stringf("%s: %s",
                                                 label.c_str(),
                                                 cell.value.c_str());
                result << colourize_str(padded_str(body, cell_len), cell.colour);
            }
            result << "\n";
        }
    }
};

// Converts a numeric resistance to its symbolic counterpart.
// Can handle any maximum level. The default is for single level resistances
// (the most common case). Negative resistances are allowed.
// Resistances with a maximum of up to 4 are spaced (arbitrary choice), and
// starting at 5 levels, they are continuous.
// params:
//  level : actual resistance level
//  max : maximum number of levels of the resistance
//  immune : overwrites normal pip display for full immunity
string desc_resist(int level, int max, bool immune, bool allow_spacing)
{
    if (max < 1)
        return "";

    if (immune)
        return Options.char_set == CSET_ASCII ? "inf" : "\u221e"; //"∞"

    string sym;
    const bool spacing = allow_spacing && max < 5;

    while (max > 0)
    {
        if (level == 0)
            sym += ".";
        else if (level > 0)
        {
            sym += "+";
            --level;
        }
        else // negative resistance
        {
            sym += "x";
            ++level;
        }
        sym += (spacing) ? " " : "";
        --max;
    }
    return sym;
}

static string _res_name(mon_resist_flags res)
{
    switch (res)
    {
    case MR_RES_FIRE:   return "rF";
    case MR_RES_COLD:   return "rC";
    case MR_RES_POISON: return "rPois";
    case MR_RES_ELEC:   return "rElec";
    case MR_RES_NEG:    return "rNeg";
    default:            return "rEggplant";
    }
}

static void _desc_mon_resist(TablePrinter &pr,
                             resists_t resist_set, mon_resist_flags res)
{
    const int level = get_resist(resist_set, res);
    const int max = (res == MR_RES_POISON || res == MR_RES_ELEC) ? 1 : 3; // lies
    const string desc = desc_resist(level, max, level == 3, false);
    pr.AddCell(_res_name(res), desc, level ? LIGHTGREY : DARKGREY);
}

static void _add_resist_desc(TablePrinter &pr, resists_t resist_set)
{
    const mon_resist_flags common_resists[] =
        { MR_RES_FIRE, MR_RES_COLD, MR_RES_POISON, MR_RES_NEG, MR_RES_ELEC };

    bool found = false;
    for (mon_resist_flags rflags : common_resists)
        if (get_resist(resist_set, rflags))
            found = true;
    if (!found)
        return;

    pr.AddRow();
    for (mon_resist_flags rflags : common_resists)
        _desc_mon_resist(pr, resist_set, rflags);
}

static void _desc_mon_death_explosion(ostringstream &result,
                                      const monster_info &mi)
{
    if (mi.type == MONS_LURKING_HORROR)
        return; // no damage number
    const dice_def dam = mon_explode_dam(mi.type, mi.hd);
    result << "Explosion damage: " << dam.num << "d" << dam.size << "\n";
}

// Describe a monster's (intrinsic) resistances, speed and a few other
// attributes.
static string _monster_stat_description(const monster_info& mi, bool mark_spells)
{
    if (mons_is_sensed(mi.type) || mons_is_projectile(mi.type))
        return "";

    ostringstream result;

    TablePrinter pr;

    pr.AddRow();
    pr.AddCell("Max HP", mi.get_max_hp_desc());
    pr.AddCell("Will", _describe_monster_wl(mi));
    pr.AddCell("AC", _build_bar(mi.ac, 5));
    pr.AddCell("EV", _build_bar(mi.base_ev, 5));
    if (mi.sh / 2 > 0)  // rescale to match player SH
        pr.AddCell("SH", _build_bar(mi.sh / 2, 5));
    else
        pr.AddCell(); // ensure alignment

    const resists_t resist = mi.resists();
    _add_resist_desc(pr, resist);

    // Less important common properties. Arguably should be lower down.
    const size_type sz = mi.body_size();
    const string size_desc = sz == SIZE_LITTLE ? "V. Small" : uppercase_first(get_size_adj(sz));
    const auto holiness = mons_class_holiness(mi.type);
    const string holi = holiness == MH_NONLIVING ? "Nonliv."
                                                 : single_holiness_description(holiness);
    pr.AddRow();
    if (mi.threat != MTHRT_UNDEF && !mons_is_conjured(mi.type))
        pr.AddCell("Threat", _get_threat_desc(mi.threat));
    else // ?/m
        pr.AddCell(); // ensure alignment
    pr.AddCell("Class", uppercase_first(holi).c_str());
    pr.AddCell("Size", size_desc.c_str());
    pr.AddCell("Int", _get_int_desc(mi.intel()));
    if (mi.is(MB_SICK) || mi.is(MB_NO_REGEN))
        pr.AddCell("Regen", "None");
    else if (mons_class_fast_regen(mi.type) || mi.is(MB_REGENERATION))
        pr.AddCell("Regen", make_stringf("%d/turn", mi.regen_rate(1)));
                                        // (Wait, what's a 'turn'?)

    pr.Print(result);

    result << mi.speed_description() << "\n\n";

    if (crawl_state.game_started)
    {
        result << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE)) << " "
               << conjugate_verb("have", mi.pronoun_plurality()) << " ";
        describe_to_hit(mi, result, you.weapon());
        if (mi.base_ev != mi.ev)
        {
            if (!mi.ev)
                result << " (while incapacitated)";
            else
                result << " (at present)";
        }
        result << ".\n";
    }
    result << _monster_attacks_description(mi);

    const mon_resist_flags special_resists[] =
    {
        MR_RES_STEAM,     MR_RES_ACID,
        MR_RES_DAMNATION, MR_RES_MIASMA,  MR_RES_TORMENT,
    };

    vector<string> extreme_resists;
    vector<string> high_resists;
    vector<string> base_resists;
    vector<string> suscept;

    for (mon_resist_flags rflags : special_resists)
    {
        int level = get_resist(resist, rflags);

        if (level != 0)
        {
            const char* attackname = _get_resist_name(rflags);
            if (rflags == MR_RES_DAMNATION || rflags == MR_RES_TORMENT)
                level = 3; // one level is immunity
            level = max(level, -1);
            level = min(level,  3);
            switch (level)
            {
                case -1:
                    suscept.emplace_back(attackname);
                    break;
                case 1:
                    base_resists.emplace_back(attackname);
                    break;
                case 2:
                    high_resists.emplace_back(attackname);
                    break;
                case 3:
                    extreme_resists.emplace_back(attackname);
                    break;
            }
        }
    }

    if (mi.is(MB_UNBLINDABLE))
        extreme_resists.emplace_back("blinding");
    // Resists engulfing/waterlogging but still dies on falling into deep water.
    if (mi.is(MB_RES_DROWN))
        base_resists.emplace_back("drowning");

    if (mi.props.exists(CLOUD_IMMUNE_MB_KEY) && mi.props[CLOUD_IMMUNE_MB_KEY])
        extreme_resists.emplace_back("clouds of all kinds");

    vector<string> resist_descriptions;
    if (!extreme_resists.empty())
    {
        const string tmp = "immune to "
            + comma_separated_line(extreme_resists.begin(),
                                   extreme_resists.end());
        resist_descriptions.push_back(tmp);
    }
    if (!high_resists.empty())
    {
        const string tmp = "very resistant to "
            + comma_separated_line(high_resists.begin(), high_resists.end());
        resist_descriptions.push_back(tmp);
    }
    if (!base_resists.empty())
    {
        const string tmp = "resistant to "
            + comma_separated_line(base_resists.begin(), base_resists.end());
        resist_descriptions.push_back(tmp);
    }

    const char* pronoun = mi.pronoun(PRONOUN_SUBJECTIVE);
    const bool plural = mi.pronoun_plurality();

    if (mi.has_unusual_items())
    {
        const vector<string> unusual_items = mi.get_unusual_items();

        result << uppercase_first(pronoun) << " ";
        (!mons_class_is_animated_weapon(mi.type) ?
            result << conjugate_verb("have", plural)
                   << " an unusual item: "
                   << comma_separated_line(unusual_items.begin(),
                                           unusual_items.end()) :
            result << conjugate_verb("are", plural)
                   << " an unusual item")
               << ".\n";
    }

    if (!resist_descriptions.empty())
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural) << " "
               << comma_separated_line(resist_descriptions.begin(),
                                       resist_descriptions.end(),
                                       "; and ", "; ")
               << ".\n";
    }

    // Is monster susceptible to anything? (On a new line.)
    if (!suscept.empty())
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural) << " susceptible to "
               << comma_separated_line(suscept.begin(), suscept.end())
               << ".\n";
    }

    if (mi.is(MB_CHAOTIC))
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural)
               << " vulnerable to silver and hated by Zin.\n";
    }

    if (mons_class_flag(mi.type, M_STATIONARY)
        && !mons_is_tentacle_or_tentacle_segment(mi.type))
    {
        result << uppercase_first(pronoun) << " cannot move.\n";
    }

    if (mons_class_flag(mi.type, M_COLD_BLOOD)
        && get_resist(resist, MR_RES_COLD) <= 0)
    {
        result << uppercase_first(pronoun)
               << " " << conjugate_verb("are", plural)
               << " cold-blooded and may be slowed by cold attacks.\n";
    }

    if (mi.can_see_invisible())
        result << uppercase_first(pronoun) << " can see invisible.\n";

    if (mons_class_flag(mi.type, M_INSUBSTANTIAL))
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural)
               << " insubstantial and immune to ensnarement.\n";
    }

    if (mons_genus(mi.type) == MONS_JELLY)
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural)
               << " amorphous and immune to ensnarement.\n";
    }

    // XXX: could mention "immune to dazzling" here, but that's spammy, since
    // it's true of such a huge number of monsters. (undead, statues, plants).
    // Might be better to have some place where players can see holiness &
    // information about holiness.......?

    if (mi.type == MONS_SHADOWGHAST)
    {
        // Cf. monster::action_energy() in monster.cc.
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("cover", plural)
               << " ground more quickly when invisible.\n";
    }

    if (mi.type == MONS_ROYAL_JELLY)
    {
        result << "It will release varied jellies when damaged or killed, with"
            " the number of jellies proportional to the amount of damage.\n";
        result << "It will release all of its jellies when polymorphed.\n";
    }

    if (mi.airborne())
        result << uppercase_first(pronoun) << " can fly.\n";

    if (in_good_standing(GOD_ZIN, 0) && !mi.pos.origin() && monster_at(mi.pos))
    {
        recite_counts retval;
        monster *m = monster_at(mi.pos);
        auto eligibility = zin_check_recite_to_single_monster(m, retval);
        if (eligibility == RE_INELIGIBLE)
        {
            result << uppercase_first(pronoun) <<
                    " cannot be affected by reciting Zin's laws.";
        }
        else if (eligibility == RE_TOO_STRONG)
        {
            result << uppercase_first(pronoun) << " "
                   << conjugate_verb("are", plural)
                   << " too strong to be affected by reciting Zin's laws.";
        }
        else // RE_ELIGIBLE || RE_RECITE_TIMER
        {
            result << uppercase_first(pronoun) <<
                            " can be affected by reciting Zin's laws.";
        }

        if (you.wizard)
        {
            result << " (Recite power:" << zin_recite_power()
                   << ", Hit dice:" << mi.hd << ")";
        }
        result << "\n";
    }

    if (mon_explodes_on_death(mi.type))
        _desc_mon_death_explosion(result, mi);

    result << _monster_habitat_description(mi);
    result << _monster_spells_description(mi, mark_spells);

    return result.str();
}

branch_type serpent_of_hell_branch(monster_type m)
{
    switch (m)
    {
    case MONS_SERPENT_OF_HELL_COCYTUS:
        return BRANCH_COCYTUS;
    case MONS_SERPENT_OF_HELL_DIS:
        return BRANCH_DIS;
    case MONS_SERPENT_OF_HELL_TARTARUS:
        return BRANCH_TARTARUS;
    case MONS_SERPENT_OF_HELL:
        return BRANCH_GEHENNA;
    default:
        die("bad serpent of hell monster_type");
    }
}

string serpent_of_hell_flavour(monster_type m)
{
    return lowercase_string(branches[serpent_of_hell_branch(m)].shortname);
}

static string _desc_foxfire_dam(const monster_info &mi)
{
    bolt beam;
    zappy(ZAP_FOXFIRE, mi.hd, mi.attitude != ATT_FRIENDLY, beam);
    return make_stringf("%dd%d", beam.damage.num, beam.damage.size);
}

// Fetches the monster's database description and reads it into inf.
void get_monster_db_desc(const monster_info& mi, describe_info &inf,
                         bool &has_stat_desc, bool mark_spells)
{
    if (inf.title.empty())
        inf.title = getMiscString(mi.common_name(DESC_DBNAME) + " title");
    if (inf.title.empty())
        inf.title = uppercase_first(mi.full_name(DESC_A)) + ".";

    string db_name;

    if (mi.props.exists(DBNAME_KEY))
        db_name = mi.props[DBNAME_KEY].get_string();
    else if (mi.mname.empty())
        db_name = mi.db_name();
    else
        db_name = mi.full_name(DESC_PLAIN);

    if (mons_species(mi.type) == MONS_SERPENT_OF_HELL)
        db_name += " " + serpent_of_hell_flavour(mi.type);
    if (mi.type == MONS_ORC_APOSTLE && mi.attitude == ATT_FRIENDLY)
        db_name = "orc apostle follower";

    // This is somewhat hackish, but it's a good way of over-riding monsters'
    // descriptions in Lua vaults by using MonPropsMarker. This is also the
    // method used by set_feature_desc_long, etc. {due}
    if (!mi.description.empty())
        inf.body << mi.description;
    // Don't get description for player ghosts.
    else if (mi.type != MONS_PLAYER_GHOST
             && mi.type != MONS_PLAYER_ILLUSION)
    {
        inf.body << getLongDescription(db_name);
    }

    // And quotes {due}
    if (!mi.quote.empty())
        inf.quote = mi.quote;
    else
        inf.quote = getQuoteString(db_name);

    string symbol;
    symbol += get_monster_data(mi.type)->basechar;
    if (isaupper(symbol[0]))
        symbol = "cap-" + symbol;

    string quote2;
    if (!mons_is_unique(mi.type))
    {
        string symbol_prefix = "__" + symbol + "_prefix";
        inf.prefix = getLongDescription(symbol_prefix);

        string symbol_suffix = "__" + symbol + "_suffix";
        quote2 = getQuoteString(symbol_suffix);
    }

    if (!inf.quote.empty() && !quote2.empty())
        inf.quote += "\n";
    inf.quote += quote2;

    const string it = mi.pronoun(PRONOUN_SUBJECTIVE);
    const string it_o = mi.pronoun(PRONOUN_OBJECTIVE);
    const string It = uppercase_first(it);
    const string is = conjugate_verb("are", mi.pronoun_plurality());

    switch (mi.type)
    {
    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_PALE_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_GREY_DRACONIAN:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_STORMCALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_KNIGHT:
    {
        inf.body << "\n" << _describe_draconian(mi) << "\n";
        break;
    }

    case MONS_PLAYER_GHOST:
        inf.body << "The apparition of " << get_ghost_description(mi) << ".\n";
        if (mi.props.exists(MIRRORED_GHOST_KEY))
            inf.body << "It looks just like you...spooky!\n";
        break;

    case MONS_PLAYER_ILLUSION:
        inf.body << "An illusion of " << get_ghost_description(mi) << ".\n";
        break;

    case MONS_PANDEMONIUM_LORD:
        inf.body << _describe_demon(mi.mname, mi.airborne(), mi.ghost_colour) << "\n";
        break;

    case MONS_MUTANT_BEAST:
        // vault renames get their own descriptions
        if (mi.mname.empty() || !mi.is(MB_NAME_REPLACE))
            inf.body << _describe_mutant_beast(mi) << "\n";
        break;

    case MONS_BLOCK_OF_ICE:
        if (mi.is(MB_SLOWLY_DYING))
            inf.body << "\nIt is quickly melting away.\n";
        break;

    case MONS_BRIAR_PATCH: // death msg uses "crumbling"
    case MONS_PILLAR_OF_SALT:
        // XX why are these "quick" here but "slow" elsewhere??
        if (mi.is(MB_SLOWLY_DYING))
            inf.body << "\nIt is quickly crumbling away.\n";
        break;

    case MONS_FOXFIRE:
        inf.body << "\nIt deals " << _desc_foxfire_dam(mi) << " fire damage.\n";
        break;

    case MONS_PROGRAM_BUG:
        inf.body << "If this monster is a \"program bug\", then it's "
                "recommended that you save your game and reload. Please report "
                "monsters who masquerade as program bugs or run around the "
                "dungeon without a proper description to the authorities.\n";
        break;

    default:
        break;
    }

    if (mons_class_is_fragile(mi.type))
    {
        if (mi.is(MB_CRUMBLING))
            inf.body << "\nIt is quickly crumbling away.\n";
        else if (mi.is(MB_WITHERING))
            inf.body << "\nIt is quickly withering away.\n";
        else if (mi.holi & (MH_NONLIVING))
            inf.body << "\nIf struck, it will crumble away soon after.\n";
        else
            inf.body << "\nIf struck, it will die soon after.\n";
    }

    if (!mons_is_unique(mi.type))
    {
        string symbol_suffix = "__";
        symbol_suffix += symbol;
        symbol_suffix += "_suffix";

        string suffix = getLongDescription(symbol_suffix)
                      + getLongDescription(symbol_suffix + "_examine");

        if (!suffix.empty())
            inf.body << "\n" << suffix;
    }

    const int curse_power = mummy_curse_power(mi.type);
    if (curse_power && !mi.is(MB_SUMMONED))
    {
        inf.body << "\n" << It << " will inflict a ";
        if (curse_power > 10)
            inf.body << "powerful ";
        inf.body << "necromantic curse on "
                 << mi.pronoun(PRONOUN_POSSESSIVE) << " foe when destroyed.\n";
    }

    // Get information on resistances, speed, etc.
    string result = _monster_stat_description(mi, mark_spells);
    if (!result.empty())
    {
        inf.body << "\n" << result;
        has_stat_desc = true;
    }

    bool did_stair_use = false;
    if (!mons_class_can_use_stairs(mi.type))
    {
        inf.body << It << " " << is << " incapable of using stairs.\n";
        did_stair_use = true;
    }

    result = _monster_current_target_description(mi);
    if (!result.empty())
        inf.body << "\n" << result;

    if (mi.is(MB_SUMMONED) || mi.is(MB_PERM_SUMMON))
    {
        inf.body << "\nThis monster has been summoned"
                 << (mi.is(MB_SUMMONED) ? ", and is thus only temporary. "
                                        : " in a durable way. ");
        // TODO: hacks; convert angered_by_attacks to a monster_info check
        // (but on the other hand, it is really limiting to not have access
        // to the monster...)
        if (!mi.pos.origin() && monster_at(mi.pos)
                                && monster_at(mi.pos)->angered_by_attacks()
                                && mi.attitude == ATT_FRIENDLY)
        {
            inf.body << "If angered " << it
                                      << " will immediately vanish, yielding ";
        }
        else
            inf.body << "Killing " << it_o << " yields ";
        inf.body << "no experience or items";

        if (!did_stair_use && !mi.is(MB_PERM_SUMMON))
            inf.body << "; " << it << " " << is << " incapable of using stairs";

        if (mi.is(MB_PERM_SUMMON))
            inf.body << " and " << it << " cannot be abjured";

        inf.body << ".\n";
    }
    else if (mi.is(MB_NO_REWARD))
        inf.body << "\nKilling this monster yields no experience or items.";
    else if (mons_class_leaves_hide(mi.type))
    {
        inf.body << "\nIf " << it << " " << is <<
                    " slain, it may be possible to recover "
                 << mi.pronoun(PRONOUN_POSSESSIVE)
                 << " hide, which can be used as armour.\n";
    }

    if (mi.is(MB_SUMMONED_CAPPED))
    {
        inf.body << "\nYou have summoned too many monsters of this kind to "
                    "sustain them all, and thus this one will shortly "
                    "expire.\n";
    }

    if (!inf.quote.empty())
        inf.quote += "\n";

#ifdef DEBUG_DIAGNOSTICS
    if (you.suppress_wizard)
        return;
    if (mi.pos.origin() || !monster_at(mi.pos))
        return; // not a real monster
    monster& mons = *monster_at(mi.pos);

    if (mons.has_originating_map())
    {
        inf.body << make_stringf("\nPlaced by map: %s",
                                 mons.originating_map().c_str());
    }

    inf.body << "\nMonster health: "
             << mons.hit_points << "/" << mons.max_hit_points << "\n";

    const actor *mfoe = mons.get_foe();
    inf.body << "Monster foe: "
             << (mfoe? mfoe->name(DESC_PLAIN, true)
                 : "(none)");

    vector<string> attitude;
    if (mons.friendly())
        attitude.emplace_back("friendly");
    if (mons.neutral())
        attitude.emplace_back("neutral");
    if (mons.good_neutral())
        attitude.emplace_back("good_neutral");
    if (mons.pacified())
        attitude.emplace_back("pacified");
    if (mons.wont_attack())
        attitude.emplace_back("wont_attack");
    if (!attitude.empty())
    {
        string att = comma_separated_line(attitude.begin(), attitude.end(),
                                          "; ", "; ");
        if (mons.has_ench(ENCH_FRENZIED))
            inf.body << "; frenzied and wild (otherwise " << att << ")";
        else
            inf.body << "; " << att;
    }
    else if (mons.has_ench(ENCH_FRENZIED))
        inf.body << "; frenzied and wild";

    inf.body << "\n\nHas holiness: ";
    inf.body << holiness_description(mi.holi);
    inf.body << ".";

    const monster_spells &hspell_pass = mons.spells;
    bool found_spell = false;

    for (unsigned int i = 0; i < hspell_pass.size(); ++i)
    {
        if (!found_spell)
        {
            inf.body << "\n\nMonster Spells:\n";
            found_spell = true;
        }

        inf.body << "    " << i << ": "
                 << spell_title(hspell_pass[i].spell)
                 << " (";
        if (hspell_pass[i].flags & MON_SPELL_EMERGENCY)
            inf.body << "emergency, ";
        if (hspell_pass[i].flags & MON_SPELL_NATURAL)
            inf.body << "natural, ";
        if (hspell_pass[i].flags & MON_SPELL_MAGICAL)
            inf.body << "magical, ";
        if (hspell_pass[i].flags & MON_SPELL_WIZARD)
            inf.body << "wizard, ";
        if (hspell_pass[i].flags & MON_SPELL_PRIEST)
            inf.body << "priest, ";
        if (hspell_pass[i].flags & MON_SPELL_VOCAL)
            inf.body << "vocal, ";
        if (hspell_pass[i].flags & MON_SPELL_BREATH)
            inf.body << "breath, ";
        inf.body << (int) hspell_pass[i].freq << ")";
    }

    bool has_item = false;
    for (mon_inv_iterator ii(mons); ii; ++ii)
    {
        if (!has_item)
        {
            inf.body << "\n\nMonster Inventory:\n";
            has_item = true;
        }
        inf.body << "    " << ii.slot() << ": "
                 << ii->name(DESC_A, false, true);
    }

    if (mons.props.exists(BLAME_KEY))
    {
        inf.body << "\n\nMonster blame chain:\n";

        const CrawlVector& blame = mons.props[BLAME_KEY].get_vector();

        for (const auto &entry : blame)
            inf.body << "    " << entry.get_string() << "\n";
    }
    inf.body << "\n\n" << debug_constriction_string(&mons);
#endif
}

int describe_monsters(const monster_info &mi, const string& /*footer*/)
{
    bool has_stat_desc = false;
    describe_info inf;
    formatted_string desc;

#ifdef USE_TILE_WEB
    // mark the spellset (XX, separate mi field?)
    get_monster_db_desc(mi, inf, has_stat_desc, true);
#else
    get_monster_db_desc(mi, inf, has_stat_desc);
#endif

    spellset spells = monster_spellset(mi);

    auto vbox = make_shared<Box>(Widget::VERT);
    auto title_hbox = make_shared<Box>(Widget::HORZ);

#ifdef USE_TILE_LOCAL
    auto dgn = make_shared<Dungeon>();
    dgn->width = dgn->height = 1;
    dgn->buf().add_monster(mi, 0, 0);
    title_hbox->add_child(std::move(dgn));
#endif

    auto title = make_shared<Text>();
    title->set_text(inf.title);
    title->set_margin_for_sdl(0, 0, 0, 10);
    title_hbox->add_child(std::move(title));

    title_hbox->set_cross_alignment(Widget::CENTER);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    vbox->add_child(std::move(title_hbox));

    desc += inf.body.str();
    if (crawl_state.game_is_hints())
        desc += formatted_string(hints_describe_monster(mi, has_stat_desc));
    desc += inf.footer;
    string raw_desc = trimmed_string(desc); // forces us back to a colour string

#ifdef USE_TILE_WEB
    // the spell list is delimited so that we can later replace this substring
    // with a placeholder marker; remove the delimiters for console display
    desc = formatted_string::parse_string(
        replace_all(
            replace_all(raw_desc, SPELL_LIST_BEGIN, ""),
            SPELL_LIST_END, ""));
#else
    desc = formatted_string::parse_string(raw_desc);
#endif

    const formatted_string quote = formatted_string(trimmed_string(inf.quote));

    auto desc_sw = make_shared<Switcher>();
    auto more_sw = make_shared<Switcher>();
    desc_sw->current() = 0;
    more_sw->current() = 0;

    const char* mores[2] = {
        "[<w>!</w>]: <w>Description</w>|Quote",
        "[<w>!</w>]: Description|<w>Quote</w>",
    };

    for (int i = 0; i < (inf.quote.empty() ? 1 : 2); i++)
    {
        const formatted_string *content[2] = { &desc, &quote };
        auto scroller = make_shared<Scroller>();
        auto text = make_shared<Text>(content[i]->trim());
        text->set_wrap_text(true);
        scroller->set_child(text);
        desc_sw->add_child(std::move(scroller));

        more_sw->add_child(make_shared<Text>(
                formatted_string::parse_string(mores[i])));
    }

    more_sw->set_margin_for_sdl(20, 0, 0, 0);
    more_sw->set_margin_for_crt(1, 0, 0, 0);
    desc_sw->expand_h = false;
    desc_sw->align_x = Widget::STRETCH;
    vbox->add_child(desc_sw);
    if (!inf.quote.empty())
        vbox->add_child(more_sw);

#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif

    auto popup = make_shared<ui::Popup>(std::move(vbox));

    bool done = false;
    int lastch;
    popup->on_keydown_event([&](const KeyEvent& ev) {
        const auto key = ev.key();
        lastch = key;
        done = ui::key_exits_popup(key, true);
        if (!inf.quote.empty() && key == '!')
        {
            int n = (desc_sw->current() + 1) % 2;
            desc_sw->current() = more_sw->current() = n;
#ifdef USE_TILE_WEB
            tiles.json_open_object();
            tiles.json_write_int("pane", n);
            tiles.ui_state_change("describe-monster", 0);
#endif
        }
        if (desc_sw->current_widget()->on_event(ev))
            return true;
        const vector<pair<spell_type,char>> spell_map = map_chars_to_spells(spells);
        auto entry = find_if(spell_map.begin(), spell_map.end(),
                [key](const pair<spell_type,char>& e) { return e.second == key; });
        if (entry == spell_map.end())
            return false;
        describe_spell(entry->first, &mi, nullptr);
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("title", inf.title);

    string desc_without_spells = raw_desc;
    if (!spells.empty())
    {
        // TODO: webtiles could could just look for these delimiters? But I'm
        // fixing a bug very close to release so I'm not going to change this
        // now -advil
        auto start = desc_without_spells.find(SPELL_LIST_BEGIN);
        auto end = desc_without_spells.find(SPELL_LIST_END);
        if (start == string::npos || end == string::npos || start >= end)
            desc_without_spells += "\n\nBUGGY SPELLSET\n\nSPELLSET_PLACEHOLDER";
        else
        {
            const size_t len = end + (string(SPELL_LIST_END) + "\n").size() - start;
            desc_without_spells.replace(start, len, "SPELLSET_PLACEHOLDER");
        }
    }
    tiles.json_write_string("body", desc_without_spells);
    tiles.json_write_string("quote", quote);
    write_spellset(spells, nullptr, &mi);

    {
        tileidx_t t    = tileidx_monster(mi);
        tileidx_t t0   = t & TILE_FLAG_MASK;
        tileidx_t flag = t & (~TILE_FLAG_MASK);

        if (!mons_class_is_stationary(mi.type) || mi.type == MONS_TRAINING_DUMMY)
        {
            tileidx_t mcache_idx = mcache.register_monster(mi);
            t = flag | (mcache_idx ? mcache_idx : t0);
            t0 = t & TILE_FLAG_MASK;
        }

        tiles.json_write_int("fg_idx", t0);
        tiles.json_write_name("flag");
        tiles.write_tileidx(flag);
        tiles.json_write_icons(status_icons_for(mi));

        if (t0 >= TILEP_MCACHE_START)
        {
            mcache_entry *entry = mcache.get(t0);
            if (entry)
                tiles.send_mcache(entry, false);
            else
            {
                tiles.json_write_comma();
                tiles.write_message("\"doll\":[[%d,%d]]", TILEP_MONS_UNKNOWN, TILE_Y);
                tiles.json_write_null("mcache");
            }
        }
        else if (get_tile_texture(t0) == TEX_PLAYER)
        {
            tiles.json_write_comma();
            tiles.write_message("\"doll\":[[%u,%d]]", (unsigned int) t0, TILE_Y);
            tiles.json_write_null("mcache");
        }
    }
    tiles.push_ui_layout("describe-monster", 1);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    ui::run_layout(std::move(popup), done);

    return lastch;
}

static const char* xl_rank_names[] =
{
    "weakling",
    "amateur",
    "novice",
    "journeyman",
    "adept",
    "veteran",
    "master",
    "legendary"
};

static string _xl_rank_name(const int xl_rank)
{
    const string rank = xl_rank_names[xl_rank];

    return article_a(rank);
}

string short_ghost_description(const monster *mon, bool abbrev)
{
    ASSERT(mons_is_pghost(mon->type));

    const ghost_demon &ghost = *(mon->ghost);
    const char* rank = xl_rank_names[ghost_level_to_rank(ghost.xl)];

    string desc = make_stringf("%s %s %s", rank,
                               species::name(ghost.species).c_str(),
                               get_job_name(ghost.job));

    if (abbrev || strwidth(desc) > 40)
    {
        desc = make_stringf("%s %s%s",
                            rank,
                            species::get_abbrev(ghost.species),
                            get_job_abbrev(ghost.job));
    }

    return desc;
}

// Describes the current ghost's previous owner. The caller must
// prepend "The apparition of" or whatever and append any trailing
// punctuation that's wanted.
string get_ghost_description(const monster_info &mi, bool concise)
{
    ostringstream gstr;

    const species_type gspecies = mi.i_ghost.species;

    gstr << mi.mname << " the "
         << skill_title_by_rank(mi.i_ghost.best_skill,
                        mi.i_ghost.best_skill_rank,
                        gspecies,
                        species::has_low_str(gspecies), mi.i_ghost.religion)
         << ", " << _xl_rank_name(mi.i_ghost.xl_rank) << " ";

    if (concise)
    {
        gstr << species::get_abbrev(gspecies)
             << get_job_abbrev(mi.i_ghost.job);
    }
    else
    {
        gstr << species::name(gspecies)
             << " "
             << get_job_name(mi.i_ghost.job);
    }

    if (mi.i_ghost.religion != GOD_NO_GOD)
    {
        gstr << " of "
             << god_name(mi.i_ghost.religion);
    }

    return gstr.str();
}

void describe_skill(skill_type skill)
{
    describe_info inf;
    inf.title = skill_name(skill);
    inf.body << get_skill_description(skill, false);
    tile_def tile = tile_def(tileidx_skill(skill, TRAINING_ENABLED));
    show_description(inf, &tile);
}

#ifdef USE_TILE_LOCAL // only used in tiles
string get_command_description(const command_type cmd, bool terse)
{
    string lookup = command_to_name(cmd);

    if (!terse)
        lookup += " verbose";

    string result = getLongDescription(lookup);
    if (result.empty())
    {
        if (!terse)
        {
            // Try for the terse description.
            result = get_command_description(cmd, true);
            if (!result.empty())
                return result + ".";
        }
        return command_to_name(cmd);
    }

    return result.substr(0, result.length() - 1);
}
#endif

/**
 * Provide auto-generated information about the given cloud type. Describe
 * opacity & related factors.
 *
 * @param cloud_type        The cloud_type in question.
 * @return e.g. "\nThis cloud is opaque; one tile will not block vision, but
 *      multiple will. \n\nClouds of this kind the player makes will vanish very
 *      quickly once outside the player's sight."
 */
string extra_cloud_info(cloud_type cloud_type)
{
    const bool opaque = is_opaque_cloud(cloud_type);
    const string opacity_info = !opaque ? "" :
        "\nThis cloud is opaque; one tile will not block vision, but "
        "multiple will.\n";
    const string vanish_info
        = make_stringf("\nClouds of this kind an adventurer makes will vanish"
                       " %s once outside their sight.\n",
                       opaque ? "quickly" : "almost instantly");
    return opacity_info + vanish_info;
}

string player_species_name()
{
    if (you_worship(GOD_BEOGH))
        return species::orc_name(you.species);
    return species::name(you.species);
}
