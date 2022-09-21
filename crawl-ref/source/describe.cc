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
#include "mon-tentacle.h"
#include "movement.h"
#include "mutation.h" // mutation_name, get_mutation_desc
#include "output.h"
#include "potion.h"
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
#include "throw.h" // is_pproj_active for describe_to_hit
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
            title_hbox->add_child(move(icon));
        }
#else
        UNUSED(tile);
#endif

        auto title = make_shared<Text>(inf.title);
        title_hbox->add_child(move(title));

        title_hbox->set_cross_alignment(Widget::CENTER);
        title_hbox->set_margin_for_sdl(0, 0, 20, 0);
        title_hbox->set_margin_for_crt(0, 0, 1, 0);
        vbox->add_child(move(title_hbox));
    }

    auto desc_sw = make_shared<Switcher>();
    auto more_sw = make_shared<Switcher>();
    desc_sw->current() = 0;
    more_sw->current() = 0;

    const string descs[2] =  {
        trimmed_string(process_description(inf, false)),
        trimmed_string(inf.quote),
    };

#ifdef USE_TILE_LOCAL
# define MORE_PREFIX "[<w>!</w>" "|<w>Right-click</w>" "]: "
#else
# define MORE_PREFIX "[<w>!</w>" "]: "
#endif

    const char* mores[2] = {
        MORE_PREFIX "<w>Description</w>|Quote",
        MORE_PREFIX "Description|<w>Quote</w>",
    };

    for (int i = 0; i < (inf.quote.empty() ? 1 : 2); i++)
    {
        const auto &desc = descs[static_cast<int>(i)];
        auto scroller = make_shared<Scroller>();
        auto fs = formatted_string::parse_string(trimmed_string(desc));
        auto text = make_shared<Text>(fs);
        text->set_wrap_text(true);
        scroller->set_child(text);
        desc_sw->add_child(move(scroller));
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
        if (!inf.quote.empty() && (lastch == '!' || lastch == CK_MOUSE_CMD || lastch == '^'))
            desc_sw->current() = more_sw->current() = 1 - desc_sw->current();
        else
            done = !desc_sw->current_widget()->on_event(ev);
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

    ui::run_layout(move(popup), done);

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
    case RING_WIZARDRY:           return "Wiz";
    case RING_FIRE:               return "Fire";
    case RING_ICE:                return "Ice";
#if TAG_MAJOR_VERSION == 34
    case RING_TELEPORTATION:      return "*Tele";
    case RING_TELEPORT_CONTROL:   return "+cTele";
    case AMU_HARM:                return "Harm";
    case AMU_THE_GOURMAND:        return "Gourm";
#endif
    case AMU_MANA_REGENERATION:   return "RegenMP";
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

struct property_annotators
{
    artefact_prop_type prop;
    prop_note spell_out;
};

static vector<string> _randart_propnames(const item_def& item,
                                         bool no_comma = false)
{
    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_desc_properties(item, proprt, known);

    vector<string> propnames;

    // list the following in rough order of importance
    const property_annotators propanns[] =
    {
        // (Generally) negative attributes
        // These come first, so they don't get chopped off!
        { ARTP_PREVENT_SPELLCASTING,  prop_note::plain },
        { ARTP_PREVENT_TELEPORTATION, prop_note::plain },
        { ARTP_CONTAM,                prop_note::plain },
        { ARTP_ANGRY,                 prop_note::plain },
        { ARTP_CAUSE_TELEPORTATION,   prop_note::plain },
        { ARTP_NOISE,                 prop_note::plain },
        { ARTP_HARM,                  prop_note::plain },
        { ARTP_RAMPAGING,             prop_note::plain },
        { ARTP_CORRODE,               prop_note::plain },
        { ARTP_DRAIN,                 prop_note::plain },
        { ARTP_SLOW,                  prop_note::plain },
        { ARTP_FRAGILE,               prop_note::plain },

        // Evokable abilities come second
        { ARTP_BLINK,                 prop_note::plain },
        { ARTP_INVISIBLE,             prop_note::plain },
        { ARTP_FLY,                   prop_note::plain },

        // Resists, also really important
        { ARTP_ELECTRICITY,           prop_note::plain },
        { ARTP_POISON,                prop_note::plain },
        { ARTP_FIRE,                  prop_note::symbolic },
        { ARTP_COLD,                  prop_note::symbolic },
        { ARTP_NEGATIVE_ENERGY,       prop_note::symbolic },
        { ARTP_WILLPOWER,             prop_note::symbolic },
        { ARTP_REGENERATION,          prop_note::symbolic },
        { ARTP_RMUT,                  prop_note::plain },
        { ARTP_RCORR,                 prop_note::plain },

        // Quantitative attributes
        { ARTP_HP,                    prop_note::numeral },
        { ARTP_MAGICAL_POWER,         prop_note::numeral },
        { ARTP_AC,                    prop_note::numeral },
        { ARTP_EVASION,               prop_note::numeral },
        { ARTP_STRENGTH,              prop_note::numeral },
        { ARTP_INTELLIGENCE,          prop_note::numeral },
        { ARTP_DEXTERITY,             prop_note::numeral },
        { ARTP_SLAYING,               prop_note::numeral },
        { ARTP_SHIELDING,             prop_note::numeral },

        // Qualitative attributes (and Stealth)
        { ARTP_SEE_INVISIBLE,         prop_note::plain },
        { ARTP_STEALTH,               prop_note::symbolic },
        { ARTP_CLARITY,               prop_note::plain },
        { ARTP_RMSL,                  prop_note::plain },
        { ARTP_ARCHMAGI,              prop_note::plain },
    };

    const unrandart_entry *entry = nullptr;
    if (is_unrandom_artefact(item))
        entry = get_unrand_entry(item.unrand_idx);
    const bool skip_ego = is_unrandom_artefact(item)
                          && entry && entry->flags & UNRAND_FLAG_SKIP_EGO;

    // For randart jewellery, note the base jewellery type if it's not
    // covered by artefact_desc_properties()
    if (item.base_type == OBJ_JEWELLERY
        && (item_ident(item, ISFLAG_KNOW_TYPE)) && !skip_ego)
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
        if (!ego.empty())
        {
            // XXX: Ugly hack for adding a comma if needed.
            bool extra_props = false;
            for (const property_annotators &ann : propanns)
                if (known_proprt(ann.prop) && ann.prop != ARTP_BRAND)
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

    for (const property_annotators &ann : propanns)
    {
        if (known_proprt(ann.prop))
        {
            const int val = proprt[ann.prop];

            // Don't show rF+/rC- for =Fire, or vice versa for =Ice.
            if (item.base_type == OBJ_JEWELLERY)
            {
                if (item.sub_type == RING_FIRE
                    && (ann.prop == ARTP_FIRE && val == 1
                        || ann.prop == ARTP_COLD && val == -1))
                {
                    continue;
                }
                if (item.sub_type == RING_ICE
                    && (ann.prop == ARTP_COLD && val == 1
                        || ann.prop == ARTP_FIRE && val == -1))
                {
                    continue;
                }
            }

            ostringstream work;
            switch (ann.spell_out)
            {
            case prop_note::numeral: // e.g. AC+4
                work << showpos << artp_name(ann.prop) << val;
                break;
            case prop_note::symbolic: // e.g. F++
            {
                work << artp_name(ann.prop);

                char symbol = val > 0 ? '+' : '-';
                const int sval = abs(val);
                if (sval > 4)
                    work << symbol << sval;
                else
                    work << string(sval, symbol);

                break;
            }
            case prop_note::plain: // e.g. rPois or SInv
                work << artp_name(ann.prop);
                break;
            }
            propnames.push_back(work.str());
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

struct property_descriptor
{
    artefact_prop_type property;
    const char* desc;           // If it contains %d, will be replaced by value.
    bool is_graded_resist;
};

static const int MAX_ARTP_NAME_LEN = 10;

static string _padded_artp_name(artefact_prop_type prop)
{
    string name = artp_name(prop);
    name = chop_string(name, MAX_ARTP_NAME_LEN - 1, false) + ":";
    name.append(MAX_ARTP_NAME_LEN - name.length(), ' ');
    return name;
}

static string _randart_descrip(const item_def &item)
{
    string description;

    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_desc_properties(item, proprt, known);

    const property_descriptor propdescs[] =
    {
        { ARTP_AC, "It affects your AC (%d).", false },
        { ARTP_EVASION, "It affects your evasion (%d).", false},
        { ARTP_STRENGTH, "It affects your strength (%d).", false},
        { ARTP_INTELLIGENCE, "It affects your intelligence (%d).", false},
        { ARTP_DEXTERITY, "It affects your dexterity (%d).", false},
        { ARTP_SLAYING, "It affects your accuracy & damage with ranged "
                        "weapons and melee (%d).", false},
        { ARTP_FIRE, "fire", true},
        { ARTP_COLD, "cold", true},
        { ARTP_ELECTRICITY, "It insulates you from electricity.", false},
        { ARTP_POISON, "poison", true},
        { ARTP_NEGATIVE_ENERGY, "negative energy", true},
        { ARTP_HP, "It affects your health (%d).", false},
        { ARTP_MAGICAL_POWER, "It affects your magic capacity (%d).", false},
        { ARTP_SEE_INVISIBLE, "It lets you see invisible.", false},
        { ARTP_INVISIBLE, "It lets you turn invisible.", false},
        { ARTP_FLY, "It grants you flight.", false},
        { ARTP_BLINK, "It lets you blink.", false},
        { ARTP_NOISE, "It may make noises in combat.", false},
        { ARTP_PREVENT_SPELLCASTING, "It prevents spellcasting.", false},
        { ARTP_CAUSE_TELEPORTATION, "It may teleport you next to monsters.", false},
        { ARTP_PREVENT_TELEPORTATION, "It prevents most forms of teleportation.",
          false},
        { ARTP_ANGRY,  "It berserks you when you make melee attacks (%d% chance).", false},
        { ARTP_CLARITY, "It protects you against confusion.", false},
        { ARTP_CONTAM, "It causes magical contamination when unequipped.", false},
        { ARTP_RMSL, "It protects you from missiles.", false},
        { ARTP_REGENERATION, "It increases your rate of health regeneration.",
          false},
        { ARTP_RCORR, "It protects you from acid and corrosion.",
          false},
        { ARTP_RMUT, "It protects you from mutation.", false},
        { ARTP_CORRODE, "It may corrode you when you take damage.", false},
        { ARTP_DRAIN, "It drains your maximum health when unequipped.", false},
        { ARTP_SLOW, "It may slow you when you take damage.", false},
        { ARTP_FRAGILE, "It will be destroyed if unequipped.", false },
        { ARTP_SHIELDING, "It affects your SH (%d).", false},
        { ARTP_HARM, "It increases damage dealt and taken.", false},
        { ARTP_RAMPAGING, "It bestows one free step when moving towards enemies.",
          false},
        { ARTP_ARCHMAGI, "It increases the power of your magical spells.", false},
    };

    bool need_newline = false;
    // Give a short description of the base type, for base types with no
    // corresponding ARTP.
    if (item.base_type == OBJ_JEWELLERY
        && (item_ident(item, ISFLAG_KNOW_TYPE)))
    {
        const char* type = _jewellery_base_ability_description(item.sub_type);
        if (*type)
        {
            description += type;
            need_newline = true;
        }
    }

    for (const property_descriptor &desc : propdescs)
    {
        if (!known_proprt(desc.property)) // can this ever happen..?
            continue;

        string sdesc = desc.desc;

        // FIXME Not the nicest hack.
        char buf[80];
        snprintf(buf, sizeof buf, "%+d", proprt[desc.property]);
        sdesc = replace_all(sdesc, "%d", buf);

        if (desc.is_graded_resist)
        {
            int idx = proprt[desc.property] + 3;
            idx = min(idx, 6);
            idx = max(idx, 0);

            const char* prefixes[] =
            {
                "It makes you extremely vulnerable to ",
                "It makes you very vulnerable to ",
                "It makes you vulnerable to ",
                "Buggy descriptor!",
                "It protects you from ",
                "It greatly protects you from ",
                "It renders you almost immune to "
            };
            sdesc = prefixes[idx] + sdesc + '.';
        }

        if (need_newline)
            description += '\n';
        description += make_stringf("%s %s",
                                    _padded_artp_name(desc.property).c_str(),
                                    sdesc.c_str());
        need_newline = true;
    }

    if (known_proprt(ARTP_WILLPOWER))
    {
        const int stval = proprt[ARTP_WILLPOWER];
        char buf[80];
        snprintf(buf, sizeof buf, "%s%s It %s%s your willpower.",
                 need_newline ? "\n" : "",
                 _padded_artp_name(ARTP_WILLPOWER).c_str(),
                 (stval < -1 || stval > 1) ? "greatly " : "",
                 (stval < 0) ? "decreases" : "increases");
        description += buf;
        need_newline = true;
    }

    if (known_proprt(ARTP_STEALTH))
    {
        const int stval = proprt[ARTP_STEALTH];
        char buf[80];
        snprintf(buf, sizeof buf, "%s%s It makes you %s%s stealthy.",
                 need_newline ? "\n" : "",
                 _padded_artp_name(ARTP_STEALTH).c_str(),
                 (stval < -1 || stval > 1) ? "much " : "",
                 (stval < 0) ? "less" : "more");
        description += buf;
        need_newline = true;
    }

    return description;
}
#undef known_proprt

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
        if (entry->dbrand)
        {
            out << entry->dbrand;
            need_newline = true;
        }
        if (entry->descrip)
        {
            out << (need_newline ? "\n\n" : "") << entry->descrip;
            need_newline = true;
        }
        if (!_randart_descrip(item).empty())
            out << (need_newline ? "\n\n" : "") << _randart_descrip(item);
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
static string _describe_demon(const string& name, bool flying)
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
    else if (item.base_type == OBJ_MISSILES && is_throwable(&you, item))
        return (((10 + throw_dam / 2) - FASTEST_PLAYER_THROWING_SPEED) * 2) * 10;
    else
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
    else if (is_shield(item))
        return SK_SHIELDS; // shields are armour, so do shields before armour
    else if (item.base_type == OBJ_ARMOUR)
        return SK_ARMOUR;
    else if (item.base_type == OBJ_MISSILES && is_throwable(&you, item))
        return SK_THROWING;
    else if (item_is_evokable(item)) // not very accurate
        return SK_EVOCATIONS;
    else
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
static string _your_skill_desc(skill_type skill, bool show_target_button, int scaled_target)
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

    return make_stringf("Your %sskill: %d.%d%s",
                            (you_skill_temp != you_skill ? "(base) " : ""),
                            you_skill / 10, you_skill % 10,
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
    const int cur_delay = _get_delay(item);
    if (base_delay == cur_delay)
        return "";
    return make_stringf("\n    Current attack delay: %.1f.", (float)cur_delay / 10);
}

static string _describe_brand(brand_type brand)
{
    switch (brand) {
    case SPWPN_ACID:
    case SPWPN_CHAOS:
    case SPWPN_DISTORTION:
    case SPWPN_DRAINING:
    case SPWPN_ELECTROCUTION:
    case SPWPN_FLAMING:
    case SPWPN_FREEZING:
    case SPWPN_PAIN:
    case SPWPN_VORPAL:
    case SPWPN_VENOM:
    {
        const string brand_name = uppercase_first(brand_type_name(brand, true));
        return make_stringf(" + %s", brand_name.c_str());
    }
    default:
        return "";
    }
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

string damage_rating(const item_def *item)
{
    if (item && is_unrandom_artefact(*item, UNRAND_WOE))
        return "your enemies will bleed and die for Makhleb.";

    const bool thrown = item && item->base_type == OBJ_MISSILES;

    // Would be great to have a breakdown of UC damage by skill, form, claws etc.
    const int base_dam = item ? property(*item, PWPN_DAMAGE)
                              : unarmed_base_damage();
    const int extra_base_dam = thrown ? throwing_base_damage_bonus(*item) :
                                !item ? unarmed_base_damage_bonus(false) :
                                        0;
    const skill_type skill = item ? _item_training_skill(*item) : SK_UNARMED_COMBAT;
    const int stat_mult = stat_modify_damage(100, skill, true);
    const bool use_str = weapon_uses_strength(skill, true);
    const int skill_mult = apply_fighting_skill(apply_weapon_skill(100, skill, false), false, false);

    const int slaying = slaying_bonus(false);
    const int ench = item && item_ident(*item, ISFLAG_KNOW_PLUSES) ? item->plus : 0;
    const int plusses = slaying + ench;

    brand_type brand = SPWPN_NORMAL;
    if (!item)
        brand = get_form()->get_uc_brand();
    else if (item_type_known(*item) && !thrown)
        brand = get_weapon_brand(*item);

    const int DAM_RATE_SCALE = 100;
    int rating = (base_dam + extra_base_dam) * DAM_RATE_SCALE;
    rating = stat_modify_damage(rating, skill, true);
    rating = apply_weapon_skill(rating, skill, false);
    rating = apply_fighting_skill(rating, false, false);
    rating /= DAM_RATE_SCALE;
    rating += plusses;

    const string base_dam_desc = thrown ? make_stringf("[%d + %d (Thrw)]",
                                                       base_dam, extra_base_dam) :
                                  !item ? make_stringf("[%d + %d (UC)]",
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

    const string brand_desc
        = item && is_unrandom_artefact(*item, UNRAND_DAMNATION) ? " + Damn"
          : thrown ? _describe_missile_brand(*item)
                   : _describe_brand(brand);

    return make_stringf(
        "%d (Base %s x %d%% (%s) x %d%% (Skill)%s)%s.",
        rating,
        base_dam_desc.c_str(),
        stat_mult,
        use_str ? "Str" : "Dex",
        skill_mult,
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

static string _category_string(const item_def &item)
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

    const int spec_ench = (is_artefact(item) || verbose)
                          ? get_weapon_brand(item) : SPWPN_NORMAL;
    const int damtype = get_vorpal_type(item);

    if (verbose && !is_unrandom_artefact(item, UNRAND_LOCHABER_AXE))
    {
        switch (item_attack_skill(item))
        {
        case SK_POLEARMS:
            description += "\n\nIt can be evoked to extend its reach.";
            break;
        case SK_AXES:
            description += "\n\nIt hits all enemies adjacent to the wielder";
            if (!is_unrandom_artefact(item, UNRAND_WOE))
                description += ", dealing less damage to those not targeted";
            description += ".";
            break;
        case SK_SHORT_BLADES:
            {
                string adj = (item.sub_type == WPN_DAGGER) ? "extremely"
                                                           : "particularly";
                description += "\n\nIt is " + adj + " good for stabbing"
                               " helpless or unaware enemies.";
            }
            break;
        default:
            break;
        }
    }

    // ident known & no brand but still glowing
    // TODO: deduplicate this with the code in item-name.cc
    const bool enchanted = get_equip_desc(item) && spec_ench == SPWPN_NORMAL
                           && !item_ident(item, ISFLAG_KNOW_PLUSES);

    const unrandart_entry *entry = nullptr;
    if (is_unrandom_artefact(item))
        entry = get_unrand_entry(item.unrand_idx);
    const bool skip_ego = is_unrandom_artefact(item)
                          && entry && entry->flags & UNRAND_FLAG_SKIP_EGO;

    // special weapon descrip
    if (item_type_known(item)
        && (spec_ench != SPWPN_NORMAL || enchanted)
        && !skip_ego)
    {
        description += "\n\n";

        switch (spec_ench)
        {
        case SPWPN_FLAMING:
            if (is_range_weapon(item))
                description += "Any ammunition fired from it";
            else
                description += "It";
            description += " burns those it strikes, dealing additional fire "
                "damage.";
            if (!is_range_weapon(item) &&
                (damtype == DVORP_SLICING || damtype == DVORP_CHOPPING))
            {
                description += " Big, fiery blades are also staple "
                    "armaments of hydra-hunters.";
            }
            break;
        case SPWPN_FREEZING:
            if (is_range_weapon(item))
                description += "Any ammunition fired from it";
            else
                description += "It";
            description += " freezes those it strikes, dealing additional cold "
                "damage. It can also slow down cold-blooded creatures.";
            break;
        case SPWPN_HOLY_WRATH:
            description += "It has been blessed by the Shining One";
            if (is_range_weapon(item))
                description += ", and any ammunition fired from it causes";
            else
                description += " to cause";
            description += " great damage to the undead and demons.";
            break;
        case SPWPN_ELECTROCUTION:
            if (is_range_weapon(item))
                description += "Any ammunition fired from it";
            else
                description += "It";
            description += " occasionally discharges a powerful burst of "
                "electricity upon striking a foe.";
            break;
        case SPWPN_VENOM:
            if (is_range_weapon(item))
                description += "Any ammunition fired from it";
            else
                description += "It";
            description += " poisons the flesh of those it strikes.";
            break;
        case SPWPN_PROTECTION:
            description += "It grants its wielder temporary protection when "
                "it strikes (+7 to AC).";
            break;
        case SPWPN_DRAINING:
            description += "A truly terrible weapon, it drains the life of "
                "any living foe it strikes.";
            break;
        case SPWPN_SPEED:
            description += "Attacks with this weapon are significantly faster.";
            break;
        case SPWPN_VORPAL:
            if (is_range_weapon(item))
                description += "Any ammunition fired from it";
            else
                description += "It";
            description += " inflicts extra damage upon your enemies.";
            break;
        case SPWPN_CHAOS:
            if (is_range_weapon(item))
            {
                description += "Each projectile launched from it has a "
                    "different, random effect.";
            }
            else
            {
                description += "Each time it hits an enemy it has a "
                    "different, random effect.";
            }
            break;
        case SPWPN_VAMPIRISM:
            description += "It occasionally heals its wielder for a portion "
                "of the damage dealt when it wounds a living foe.";
            break;
        case SPWPN_PAIN:
            description += "In the hands of one skilled in necromantic "
                "magic, it inflicts extra damage on living creatures.";
            if (is_useless_skill(SK_NECROMANCY))
            {
                description += " Your inability to study Necromancy prevents "
                               "you from drawing on the full power of this "
                               "weapon.";
            }
            break;
        case SPWPN_DISTORTION:
            description += "It warps and distorts space around it, and may "
                "blink, banish, or inflict extra damage upon those it strikes. "
                "Unwielding it can cause banishment or high damage.";
            break;
        case SPWPN_PENETRATION:
            description += "Any ammunition fired by it passes through the "
                "targets it hits, potentially hitting all targets in "
                "its path until it reaches maximum range.";
            break;
        case SPWPN_REAPING:
            description += "Any living foe damaged by it may be reanimated "
                "upon death as a zombie friendly to the wielder, with an "
                "increasing chance as more damage is dealt.";
            break;
        case SPWPN_ANTIMAGIC:
            description += "It reduces the magical energy of the wielder, "
                "and disrupts the spells and magical abilities of those it "
                "strikes. Natural abilities and divine invocations are not "
                "affected.";
            break;
        case SPWPN_SPECTRAL:
            description += "When its wielder attacks, the weapon's spirit "
                "leaps out and strikes again. The spirit shares a part of "
                "any damage it takes with its wielder.";
            break;
        case SPWPN_ACID:
             if (is_range_weapon(item))
                description += "Any ammunition fired from it";
            else
                description += "It";
            description += " is coated in acid, damaging and corroding those "
                "it strikes.";
            break;
        case SPWPN_NORMAL:
            ASSERT(enchanted);
            description += "It has no special brand (it is not flaming, "
                "freezing, etc), but is still enchanted in some way - "
                "positive or negative.";
            break;
        }
    }

    string art_desc = _artefact_descrip(item);
    if (!art_desc.empty())
        description += "\n\n" + art_desc;

    if (verbose)
    {
        description += "\n\n" + _category_string(item);

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
                           "hits into a mindless rage, attacking friend and "
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

        if (!is_useless_item(item))
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
    const int SH = crawl_state.need_save ? player_shield_class() : 0;
    const int reflect_chance = 100 * SH / omnireflect_chance_denom(SH);
    return "\n\nWith your current SH, it has a " + to_string(reflect_chance) +
           "% chance to reflect attacks against your willpower and other "
           "normally unblockable effects.";
}

static string _describe_point_change(int points)
{
    string point_diff_description;

    point_diff_description += make_stringf("%s by %d",
                                           points > 0 ? "increase" : "decrease",
                                           abs(points));

    return point_diff_description;
}

static string _describe_point_diff(int original,
                                   int changed)
{
    string description;

    int difference = changed - original;

    if (difference == 0)
        return "remain unchanged.";

    description += _describe_point_change(difference);
    description += " (";
    description += to_string(original);
    description += " -> ";
    description += to_string(changed);
    description += ").";

    return description;
}

static string _armour_ac_sub_change_description(const item_def &item)
{
    string description;

    description.reserve(100);


    description += "\n\nIf you switch to wearing this armour,"
                        " your AC would ";

    int you_ac_with_this_item =
                 you.armour_class_with_one_sub(item);

    description += _describe_point_diff(you.armour_class(),
                                        you_ac_with_this_item);

    return description;
}

static string _armour_ac_remove_change_description(const item_def &item)
{
    string description;

    description += "\n\nIf you remove this armour,"
                        " your AC would ";

    int you_ac_without_item =
                 you.armour_class_with_one_removal(item);

    description += _describe_point_diff(you.armour_class(),
                                        you_ac_without_item);

    return description;
}

static bool _you_are_wearing_item(const item_def &item)
{
    return get_equip_slot(&item) != EQ_NONE;
}

static string _armour_ac_change(const item_def &item)
{
    string description;

    if (!_you_are_wearing_item(item))
        description = _armour_ac_sub_change_description(item);
    else
        description = _armour_ac_remove_change_description(item);

    return description;
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
        return "it protects its wearer by repelling missiles.";
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
               "invisible creatures and increasing accuracy against all within "
               "it other than the wearer.";
    case SPARM_RAGE:
        return "it berserks the wearer when making melee attacks (20% chance).";
    case SPARM_MAYHEM:
        return "it causes witnesses of the wearer's kills to go into a frenzy,"
               " attacking everything nearby with great strength and speed.";
    case SPARM_GUILE:
        return "it weakens the willpower of the wielder and everyone they hex.";
    case SPARM_ENERGY:
        return "it occasionally powers its wielder's spells, but with a chance"
               " of causing confusion or draining the wielder's intelligence."
               " It becomes more likely to activate and less likely to backfire"
               " with Evocations skill.";
    default:
        return "it makes the wearer crave the taste of eggplant.";
    }
}

static string _describe_armour(const item_def &item, bool verbose, bool monster)
{
    string description;

    description.reserve(300);

    if (verbose)
    {
        if (!monster)
            description += "\n\n";
        if (is_shield(item))
        {
            description += "Base shield rating: "
                        + to_string(property(item, PARM_AC));
            description += "       Encumbrance rating: "
                        + to_string(-property(item, PARM_EVASION) / 10);
            if (is_unrandom_artefact(item, UNRAND_WARLOCK_MIRROR))
                description += _warlock_mirror_reflect_desc();
        }
        else if (item.base_type == OBJ_ARMOUR && item.sub_type == ARM_ORB)
            ;
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
                          && entry && entry->flags & UNRAND_FLAG_SKIP_EGO;

    // Only give a description for armour with a known ego.
    if (ego != SPARM_NORMAL && item_type_known(item) && verbose && !skip_ego)
    {
        description += "\n\n";

        if (is_artefact(item))
        {
            // Make this match the formatting in _randart_descrip,
            // since instead of the item being named something like
            // 'cloak of invisiblity', it's 'the cloak of the Snail (+Inv, ...)'
            string name = string(armour_ego_name(item, true));
            name = chop_string(name, MAX_ARTP_NAME_LEN - 1, false) + ":";
            name.append(MAX_ARTP_NAME_LEN - name.length(), ' ');
            description += name;
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

    // Only displayed if the player exists (not for item lookup from the menu).
    if (crawl_state.need_save
        && can_wear_armour(item, false, true)
        && item_ident(item, ISFLAG_KNOW_PLUSES)
        && !is_offhand(item))
    {
        description += _armour_ac_change(item);
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

bool is_dumpable_artefact(const item_def &item)
{
    return is_known_artefact(item) && item_ident(item, ISFLAG_KNOW_PROPERTIES);
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
        if (is_xp_evoker(item))
        {
            description << "\n\nOnce "
                        << (item.sub_type == MISC_LIGHTNING_ROD
                            ? "all charges have been used"
                            : "activated")
                        << ", this device "
                        << (!item_is_horn_of_geryon(item) ?
                           "and all other devices of its kind are " : "is ")
                        << "rendered temporarily inert. However, "
                        << (!item_is_horn_of_geryon(item) ? "they recharge " : "it recharges ")
                        << "as you gain experience."
                        << (!evoker_charges(item.sub_type) ?
                           " The device is presently inert." : "");
        }
        break;

    case OBJ_POTIONS:
        if (item_type_known(item))
        {
            if (verbose)
            {
                if (item.sub_type == POT_LIGNIFY)
                    description << "\n\n" + _describe_lignify_ac();
                else if (item.sub_type == POT_CANCELLATION)
                {
                    if (player_is_cancellable())
                    {
                        description << "\n\nIf you drink this now, you will no longer be " <<
                            describe_player_cancellation() << ".";
                    }
                    else
                        description << "\n\nDrinking this now will have no effect.";
                }
            }
            description << "\n\nIt is "
                        << article_a(describe_item_rarity(item))
                        << " potion.";
        }
        break;

    case OBJ_WANDS:
        if (item_type_known(item))
        {
            description << "\n";

            const spell_type spell = spell_in_wand(static_cast<wand_type>(item.sub_type));

            const string damage_str = spell_damage_string(spell, true);
            if (damage_str != "")
                description << "\nDamage: " << damage_str;

            description << "\nNoise: " << spell_noise_string(spell);
        }
        break;

    case OBJ_SCROLLS:
        if (item_type_known(item))
        {
            description << "\n\nIt is "
                        << article_a(describe_item_rarity(item))
                        << " scroll.";
        }
        break;

    case OBJ_ORBS:
    case OBJ_GOLD:
    case OBJ_RUNES:

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
                description << "\nThis ancient artefact cannot be changed "
                    "by magic or mundane means.";
            }
            // Randart jewellery has already displayed this line.
            else if (item.base_type != OBJ_JEWELLERY
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
        if (liquefied(pos))
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
                    // TODO should probably derive this from `runes_for_branch`:
                    (feat == DNGN_ENTER_ZOT || feat == DNGN_ENTER_VAULTS)
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
        title_hbox->add_child(move(icon));
#endif
        auto title = make_shared<Text>(feat.title);
        title->set_margin_for_sdl(0, 0, 0, 10);
        title_hbox->add_child(move(title));
        title_hbox->set_cross_alignment(Widget::CENTER);

        const bool has_desc = feat.body != feat.title && feat.body != "";

        if (has_desc || &feat != &feats.back())
        {
            title_hbox->set_margin_for_crt(0, 0, 1, 0);
            title_hbox->set_margin_for_sdl(0, 0, 20, 0);
        }
        vbox->add_child(move(title_hbox));

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
        vbox->add_child(move(footer));
    }

#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif
    scroller->set_child(move(vbox));

    auto popup = make_shared<ui::Popup>(scroller);

    bool done = false;
    command_type action = CMD_NO_CMD;

    // use on_hotkey_event, not on_event, to preempt the scroller key handling
    popup->on_hotkey_event([&](const KeyEvent& ev) {
        action = _get_action(ev.key(), actions);
        if (action != CMD_NO_CMD)
            done = true;
        else
            done = !scroller->on_event(ev);
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

    ui::run_layout(move(popup), done);
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
        actions.push_back(CMD_READ);
        break;
    case OBJ_POTIONS:
        if (you.can_drink()) // mummies and lich form forbidden
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

    if (item_is_evokable(item))
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
    case CMD_WIELD_WEAPON:     wield_weapon(true, slot);            break;
    case CMD_UNWIELD_WEAPON:   wield_weapon(true, SLOT_BARE_HANDS); break;
    case CMD_QUIVER_ITEM:
        quiver::set_to_quiver(quiver::slot_to_action(slot), you.quiver_action); // ugh
        break;
    case CMD_WEAR_ARMOUR:      wear_armour(slot);                   break;
    case CMD_REMOVE_ARMOUR:    takeoff_armour(slot);                break;
    case CMD_WEAR_JEWELLERY:   puton_ring(slot);                    break;
    case CMD_REMOVE_JEWELLERY: remove_ring(slot, true);             break;
    case CMD_DROP:
        // TODO: it would be better if the inscription was checked before the
        // popup closes, but that is hard
        if (!check_warning_inscriptions(you.inv[slot], OPER_DROP))
            return true;
        drop_item(slot, item.quantity);
        break;
    case CMD_ADJUST_INVENTORY: adjust_item(slot);                   break;
    case CMD_EVOKE:            evoke_item(slot);                    break;
    default:
        die("illegal inventory cmd %d", action);
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
        you.train[skill] = TRAINING_ENABLED;
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
            tiles_stack->add_child(move(icon));
        }
        title_hbox->add_child(move(tiles_stack));
    }
#endif

    auto title = make_shared<Text>(name);
    title->set_margin_for_sdl(0, 0, 0, 10);
    title_hbox->add_child(move(title));

    title_hbox->set_cross_alignment(Widget::CENTER);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    vbox->add_child(move(title_hbox));

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
        vbox->add_child(move(footer));
    }

#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif

    auto popup = make_shared<ui::Popup>(move(vbox));

    bool done = false;
    command_type action = CMD_NO_CMD;
    int lastch; // unused??
    popup->on_keydown_event([&](const KeyEvent& ev) {
        const auto key = ev.key() == '{' ? 'i' : ev.key();
        lastch = key;
        action = _get_action(key, actions);
        if (action != CMD_NO_CMD)
            done = true;
        else if (key == ' ' || key == CK_ESCAPE)
            done = true;
        else if (scroller->on_event(ev))
            return true;
        const vector<pair<spell_type,char>> spell_map = map_chars_to_spells(spells, &item);
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

    ui::run_layout(move(popup), done);

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
    const int acc = spell_acc(spell);
    // TODO: generalize this pattern? It's very common in descriptions
    const int padding = (acc != -1) ? 8 : damage_string.size() ? 6 : 5;
    description += make_stringf("\n\n%*s: ", padding, "Power");
    description += spell_power_string(spell);

    if (damage_string != "")
    {
        description += make_stringf("\n%*s: ", padding, "Damage");
        description += damage_string;
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
        { spschool::poison, "poison" },
    };

    const map <spschool, string> special_flavor = {
        { spschool::summoning, "summons a nameless horror" },
        { spschool::transmutation, "further contaminates you" },
        { spschool::translocation, "anchors you in place" },
        { spschool::hexes, "slows you" },
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

    if (spell == SPELL_SPELLFORGED_SERVITOR)
    {
        spell_type servitor_spell = player_servitor_spell();
        description << "Your servitor";
        if (servitor_spell == SPELL_NO_SPELL)
            description << " would be unable to mimic any of your spells";
        else
        {
            description << " casts "
                        << spell_title(player_servitor_spell());
        }
        description << ".\n";
    }

    // Report summon cap
    const int limit = summons_limit(spell, true);
    if (limit)
    {
        description << "You can sustain at most " + number_in_words(limit)
                    << " creature" << (limit > 1 ? "s" : "")
                    << " summoned by this spell.\n";
    }

    if (god_hates_spell(spell, you.religion))
    {
        description << uppercase_first(god_name(you.religion))
                    << " frowns upon the use of this spell.\n";
        if (god_loathes_spell(spell, you.religion))
            description << "You'd be excommunicated if you dared to cast it!\n";
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
        else
            description += range_string(range, range, mons_char(mon_owner->type));
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
    title_hbox->add_child(move(spell_icon));
#endif

    string spl_title = spell_title(spell);
    trim_string(desc);

    auto title = make_shared<Text>();
    title->set_text(spl_title);
    title->set_margin_for_sdl(0, 0, 0, 10);
    title_hbox->add_child(move(title));

    title_hbox->set_cross_alignment(Widget::CENTER);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    vbox->add_child(move(title_hbox));

    auto scroller = make_shared<Scroller>();
    auto text = make_shared<Text>();
    text->set_text(formatted_string::parse_string(desc));
    text->set_wrap_text(true);
    scroller->set_child(move(text));
    vbox->add_child(scroller);

    auto popup = make_shared<ui::Popup>(move(vbox));

    bool done = false;
    int lastch;
    popup->on_keydown_event([&](const KeyEvent& ev) {
        lastch = ev.key();
        done = (lastch == CK_ESCAPE || lastch == CK_ENTER || lastch == ' ');
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

    ui::run_layout(move(popup), done);
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
    if (you.has_mutation(mut) && mutation_max_levels(mut) > 1)
    {
        inf.title += make_stringf(" (level %d/%d)",
                                  you.get_mutation_level(mut),
                                  mutation_max_levels(mut));
    }
    inf.body << get_mutation_desc(mut);

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
    case MR_RES_VORTEX:
        return "polar vortices";
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
    case MTHRT_TRIVIAL: return "harmless";
    case MTHRT_EASY:    return "easy";
    case MTHRT_TOUGH:   return "dangerous";
    case MTHRT_NASTY:   return "extremely dangerous";
    case MTHRT_UNDEF:
    default:            return "buggily threatening";
    }
}

/**
 * Describe monster attack 'flavours' that trigger before the attack.
 *
 * @param flavour   The flavour in question; e.g. AF_SWOOP.
 * @return          A description of anything that happens 'before' an attack
 *                  with the given flavour;
 *                  e.g. "swoop behind its target and ".
 */
static const char* _special_flavour_prefix(attack_flavour flavour)
{
    return flavour == AF_SWOOP ? "swoop behind its foe and " : "";
}

/**
 * Describe monster attack 'flavours' that have extra range.
 *
 * @param flavour   The flavour in question; e.g. AF_REACH_STING.
 * @return          If the flavour has extra-long range, say so. E.g.,
 *                  " from a distance". (Else "").
 */
static const char* _flavour_range_desc(attack_flavour flavour)
{
    if (flavour == AF_RIFT)
        return " from a great distance";
    else if (flavour_has_reach(flavour))
        return " from a distance";
    return "";
}

static string _flavour_base_desc(attack_flavour flavour)
{
    static const map<attack_flavour, string> base_descs = {
        { AF_ACID,              "deal extra acid damage"},
        { AF_BLINK,             "blink self" },
        { AF_BLINK_WITH,        "blink together with the defender" },
        { AF_COLD,              "deal up to %d extra cold damage" },
        { AF_CONFUSE,           "cause confusion" },
        { AF_DRAIN_STR,         "drain strength" },
        { AF_DRAIN_INT,         "drain intelligence" },
        { AF_DRAIN_DEX,         "drain dexterity" },
        { AF_DRAIN_STAT,        "drain strength, intelligence or dexterity" },
        { AF_DRAIN,             "drain life" },
        { AF_ELEC,              "deal up to %d extra electric damage" },
        { AF_FIRE,              "deal up to %d extra fire damage" },
        { AF_SEAR,              "remove fire resistance" },
        { AF_MUTATE,            "cause mutations" },
        { AF_POISON_PARALYSE,   "poison and cause paralysis or slowing" },
        { AF_POISON,            "cause poisoning" },
        { AF_POISON_STRONG,     "cause strong poisoning" },
        { AF_VAMPIRIC,          "drain health from the living" },
        { AF_DISTORT,           "cause wild translocation effects" },
        { AF_RAGE,              "cause berserking" },
        { AF_STICKY_FLAME,      "apply sticky flame" },
        { AF_CHAOTIC,           "cause unpredictable effects" },
        { AF_STEAL,             "steal items" },
        { AF_CRUSH,             "begin ongoing constriction" },
        { AF_REACH,             "" },
        { AF_HOLY,              "deal extra damage to undead and demons" },
        { AF_ANTIMAGIC,         "drain magic" },
        { AF_PAIN,              "cause pain to the living" },
        { AF_ENSNARE,           "ensnare with webbing" },
        { AF_ENGULF,            "engulf" },
        { AF_PURE_FIRE,         "" },
        { AF_DRAIN_SPEED,       "drain speed" },
        { AF_VULN,              "reduce willpower" },
        { AF_SHADOWSTAB,        "deal increased damage when unseen" },
        { AF_DROWN,             "deal drowning damage" },
        { AF_CORRODE,           "cause corrosion" },
        { AF_SCARAB,            "drain speed and drain health" },
        { AF_TRAMPLE,           "knock back the defender" },
        { AF_REACH_STING,       "cause poisoning" },
        { AF_REACH_TONGUE,      "deal extra acid damage" },
        { AF_RIFT,              "cause wild translocation effects" },
        { AF_WEAKNESS,          "cause weakness" },
        { AF_BARBS,             "embed barbs" },
        { AF_SPIDER,            "summon a spider" },
        { AF_SWOOP,             "" },
        { AF_PLAIN,             "" },
    };

    const string* desc = map_find(base_descs, flavour);
    ASSERT(desc);
    return *desc;
}

/**
 * Provide a short, and-prefixed flavour description of the given attack
 * flavour, if any.
 *
 * @param flavour  E.g. AF_COLD, AF_PLAIN.
 * @param HD       The hit dice of the monster using the flavour.
 * @return         "" if AF_PLAIN; else " <desc>", e.g.
 *                 " to deal up to 27 extra cold damage if any damage is dealt".
 */
static string _flavour_effect(attack_flavour flavour, int HD)
{
    const string base_desc = _flavour_base_desc(flavour);
    if (base_desc.empty())
        return base_desc;

    const int flavour_dam = flavour_damage(flavour, HD, false);
    const string flavour_desc = make_stringf(base_desc.c_str(), flavour_dam);

    if (!flavour_triggers_damageless(flavour) && flavour != AF_SWOOP)
        return " to " + flavour_desc + " if any damage is dealt";

    return " to " + flavour_desc;
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
    const item_def* weapon
       = atk == 0 ? mi.inv[MSLOT_WEAPON].get() :
         atk == 1 && mi.wields_two_weapons() ? mi.inv[MSLOT_ALT_WEAPON].get() :
         nullptr;

    if (weapon && is_weapon(*weapon))
        return weapon;
    return nullptr;
}

static string _monster_attacks_description(const monster_info& mi)
{
    ostringstream result;
    map<mon_attack_info, int> attack_counts;
    brand_type special_flavour = SPWPN_NORMAL;

    if (mi.props.exists(SPECIAL_WEAPON_KEY))
    {
        ASSERT(mi.type == MONS_PANDEMONIUM_LORD || mons_is_pghost(mi.type));
        special_flavour = (brand_type) mi.props[SPECIAL_WEAPON_KEY].get_int();
    }

    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
    {
        const mon_attack_def &attack = mi.attack[i];
        if (attack.type == AT_NONE)
            break; // assumes there are no gaps in attack arrays

        const item_def* weapon = _weapon_for_attack(mi, i);
        mon_attack_info attack_info = { attack, weapon };

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

    }

    vector<string> attack_descs;
    for (const auto &attack_count : attack_counts)
    {
        const mon_attack_info &info = attack_count.first;
        const mon_attack_def &attack = info.definition;

        const string weapon_note = info.weapon ?
            make_stringf(" with %s weapon", mi.pronoun(PRONOUN_POSSESSIVE))
            : "";

        const string count_desc =
              attack_count.second == 1 ? "" :
              attack_count.second == 2 ? " twice" :
              " " + number_in_words(attack_count.second) + " times";

        // XXX: hack alert
        if (attack.flavour == AF_PURE_FIRE)
        {
            attack_descs.push_back(
                make_stringf("%s for up to %d fire damage",
                             mon_attack_name(attack.type, false).c_str(),
                             flavour_damage(attack.flavour, mi.hd, false)));
            continue;
        }

        int dam = attack.damage;
        if (info.weapon)
        {
            dam += property(*info.weapon, PWPN_DAMAGE);
            // Enchant is rolled separately, so doesn't change the max.
            if (info.weapon->plus > 0)
                dam += info.weapon->plus;
        }

        const brand_type brand = info.weapon ? get_weapon_brand(*info.weapon)
                                             : special_flavour;
        const string brand_desc = _describe_brand(brand);

        // Damage is listed in parentheses for attacks with a flavour
        // description, but not for plain attacks.
        bool has_flavour = !_flavour_base_desc(attack.flavour).empty();
        const string damage_desc =
            make_stringf("%sfor up to %d damage%s%s%s%s",
                         has_flavour ? "(" : "",
                         dam,
                         brand_desc.c_str(),
                         attack_count.second > 1 ? " each" : "",
                         weapon_note.c_str(),
                         has_flavour ? ")" : "");

        attack_descs.push_back(
            make_stringf("%s%s%s%s %s%s",
                         _special_flavour_prefix(attack.flavour),
                         mon_attack_name(attack.type, false).c_str(),
                         _flavour_range_desc(attack.flavour),
                         count_desc.c_str(),
                         damage_desc.c_str(),
                         _flavour_effect(attack.flavour, mi.hd).c_str()));
    }

    if (!attack_descs.empty())
    {
        result << uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE));
        result << " can " << comma_separated_line(attack_descs.begin(),
                                                  attack_descs.end(),
                                                  "; and ", "; ");
        _describe_mons_to_hit(mi, result);
        result << ".\n";
    }

    if (mons_class_flag(mi.type, M_ARCHER))
    {
        result << make_stringf("It can deal up to %d extra damage when attacking with ranged weaponry.\n",
                                archer_bonus_damage(mi.hd));
    }

    if (mi.type == MONS_ROYAL_JELLY)
    {
        result << "It will release varied jellies when damaged or killed, with"
            " the number of jellies proportional to the amount of damage.\n";
        result << "It will release all of its jellies when polymorphed.\n";
    }

    return result.str();
}

static string _monster_missiles_description(const monster_info& mi)
{
    item_def *missile = mi.inv[MSLOT_MISSILE].get();
    if (!missile)
        return "";

    string desc;
    desc += uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE));
    desc += mi.pronoun_plurality() ? " are quivering " : " is quivering ";
    if (missile->is_type(OBJ_MISSILES, MI_THROWING_NET))
        desc += missile->name(DESC_A, false, false, true, false);
    else
        desc += pluralise(missile->name(DESC_PLAIN, false, false, true, false));
    desc += ".\n";
    return desc;
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
    description.cprintf("\nTo read a description, press the key listed above. "
        "(AdB) indicates damage (the sum of A B-sided dice), "
        "(x%%) indicates the chance to defeat your Will, "
        "and (y) indicates the spell range");
    description.cprintf(crawl_state.need_save
        ? "; shown in red if you are in range.\n"
        : ".\n");
    if (mark_spells)
        description += SPELL_LIST_END;

    return description.to_colour_string();
}

static const char *_speed_description(int speed)
{
    // These thresholds correspond to the player mutations for fast and slow.
    if (speed < 7)
        return "extremely slowly";
    else if (speed < 8)
        return "very slowly";
    else if (speed < 10)
        return "slowly";
    else if (speed > 15)
        return "extremely quickly";
    else if (speed > 13)
        return "very quickly";
    else if (speed > 10)
        return "quickly";

    return "normally";
}

static void _add_energy_to_string(int speed, int energy, string what,
                                  vector<string> &fast, vector<string> &slow)
{
    if (energy == 10)
        return;

    const int act_speed = (speed * 10) / energy;
    if (act_speed > 10 || (act_speed == 10 && speed < 10))
        fast.push_back(what + " " + _speed_description(act_speed));
    if (act_speed < 10 || (act_speed == 10 && speed > 10))
        slow.push_back(what + " " + _speed_description(act_speed));
}

/**
 * Display the % chance of a player hitting the given monster.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
void describe_to_hit(const monster_info& mi, ostringstream &result,
                     bool parenthesize, const item_def* weapon)
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
        ranged_attack attk(&you, nullptr, &fake_proj, is_pproj_active());
        acc_pct = to_hit_pct(mi, attk, false);
    }

    if (parenthesize)
        result << " (";
    result << "about " << (100 - acc_pct) << "% to evade ";
    if (weapon == nullptr)
        result << "your " << you.hand_name(true);
    else
        result << weapon->name(DESC_YOUR, false, false, false);
    if (parenthesize)
        result << ")";
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
    const int ev = you.evasion();

    const int to_land = weapon && is_unrandom_artefact(*weapon, UNRAND_SNIPER) ? AUTOMATIC_HIT :
                                                                total_base_hit + post_roll_modifiers;
    const int beat_ev_chance = mon_to_hit_pct(to_land, ev);

    const int shield_class = player_shield_class();
    const int shield_bypass = mon_shield_bypass(mi.hd);
    // ignore penalty for unseen attacker, as with EV above
    const int beat_sh_chance = mon_beat_sh_pct(shield_bypass, shield_class);

    const int hit_chance = beat_ev_chance * beat_sh_chance / 100;
    result << " (about " << hit_chance << "% to hit you)";
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

/**
 * Append information about a given monster's HP to the provided stream.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_monster_hp(const monster_info& mi, ostringstream &result)
{
    result << "Max HP: " << mi.get_max_hp_desc() << "\n";
}

/**
 * Append information about a given monster's AC to the provided stream.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_monster_ac(const monster_info& mi, ostringstream &result)
{
    // MAX_GHOST_EVASION + two pips (so with EV in parens it's the same)
    _print_bar(mi.ac, 5, "    AC:", result);
    result << "\n";
}

/**
 * Append information about a given monster's EV to the provided stream.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_monster_ev(const monster_info& mi, ostringstream &result)
{
    _print_bar(mi.ev, 5, "    EV:", result, mi.base_ev);
    if (crawl_state.game_started)
        describe_to_hit(mi, result, true, you.weapon());
    result << "\n";
}

/**
 * Append information about a given monster's WL to the provided stream.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_monster_wl(const monster_info& mi, ostringstream &result)
{
    if (mi.willpower() == WILL_INVULN)
    {
        result << (Options.char_set == CSET_ASCII
                        ? "  Will: \u221e\n" // ∞
                        : "  Will: inf\n");
        return;
    }

    const int bar_scale = WL_PIP;
    _print_bar(mi.willpower(), bar_scale, "  Will:", result);
    result << "\n";
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
        return uppercase_first(make_stringf("%s can travel through water.\n",
                               mi.pronoun(PRONOUN_SUBJECTIVE)));
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

// Describe a monster's (intrinsic) resistances, speed and a few other
// attributes.
static string _monster_stat_description(const monster_info& mi, bool mark_spells)
{
    if (mons_is_sensed(mi.type) || mons_is_projectile(mi.type))
        return "";

    ostringstream result;

    _describe_monster_hp(mi, result);
    _describe_monster_ac(mi, result);
    _describe_monster_ev(mi, result);
    _describe_monster_wl(mi, result);

    result << "\n";

    resists_t resist = mi.resists();

    const mon_resist_flags resists[] =
    {
        MR_RES_ELEC,    MR_RES_POISON, MR_RES_FIRE,
        MR_RES_STEAM,   MR_RES_COLD,   MR_RES_ACID,
        MR_RES_MIASMA,  MR_RES_NEG,    MR_RES_DAMNATION,
        MR_RES_VORTEX,  MR_RES_TORMENT,
    };

    vector<string> extreme_resists;
    vector<string> high_resists;
    vector<string> base_resists;
    vector<string> suscept;

    for (mon_resist_flags rflags : resists)
    {
        int level = get_resist(resist, rflags);

        if (level != 0)
        {
            const char* attackname = _get_resist_name(rflags);
            if (rflags == MR_RES_DAMNATION
                || rflags == MR_RES_VORTEX
                || rflags == MR_RES_TORMENT)
            {
                level = 3; // one level is immunity
            }
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
        base_resists.emplace_back("blinding");
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

    if (mi.threat != MTHRT_UNDEF)
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("look", plural) << " "
               << _get_threat_desc(mi.threat) << ".\n";
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

    // Seeing invisible.
    if (mi.can_see_invisible())
        result << uppercase_first(pronoun) << " can see invisible.\n";

    // Echolocation, wolf noses, jellies, etc
    if (!mons_can_be_blinded(mi.type))
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural)
               << " immune to blinding.\n";
    }

    if (mons_class_flag(mi.type, M_INSUBSTANTIAL))
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural)
               << " insubstantial and immune to ensnarement.\n";
    }

    // XXX: could mention "immune to dazzling" here, but that's spammy, since
    // it's true of such a huge number of monsters. (undead, statues, plants).
    // Might be better to have some place where players can see holiness &
    // information about holiness.......?

    if (mi.intel() <= I_BRAINLESS)
    {
        // Matters for Ely.
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural) << " mindless.\n";
    }
    else if (mi.intel() >= I_HUMAN)
    {
        // Matters for Yred, Gozag, Zin, TSO, Alistair....
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural) << " intelligent.\n";
    }

    // Unusual monster speed.
    const int speed = mi.base_speed();
    bool did_speed = false;
    if (speed != 10 && speed != 0)
    {
        did_speed = true;
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural) << " "
               << mi.speed_description();
    }
    const mon_energy_usage def = DEFAULT_ENERGY;
    if (!(mi.menergy == def))
    {
        const mon_energy_usage me = mi.menergy;
        vector<string> fast, slow;
        if (!did_speed)
            result << uppercase_first(pronoun) << " ";
        _add_energy_to_string(speed, me.move,
                              conjugate_verb("cover", plural) + " ground",
                              fast, slow);
        // since MOVE_ENERGY also sets me.swim
        if (me.swim != me.move)
        {
            _add_energy_to_string(speed, me.swim,
                                  conjugate_verb("swim", plural), fast, slow);
        }
        _add_energy_to_string(speed, me.attack,
                              conjugate_verb("attack", plural), fast, slow);
        if (mons_class_itemuse(mi.type) >= MONUSE_STARTING_EQUIPMENT)
        {
            _add_energy_to_string(speed, me.missile,
                                  conjugate_verb("shoot", plural), fast, slow);
        }
        _add_energy_to_string(
            speed, me.spell,
            mi.is_actual_spellcaster() ? conjugate_verb("cast", plural)
                                         + " spells" :
            mi.is_priest()             ? conjugate_verb("use", plural)
                                         + " invocations"
                                       : conjugate_verb("use", plural)
                                         + " natural abilities", fast, slow);
        _add_energy_to_string(speed, me.special,
                              conjugate_verb("use", plural)
                              + " special abilities",
                              fast, slow);
        if (mons_class_itemuse(mi.type) >= MONUSE_STARTING_EQUIPMENT)
        {
            _add_energy_to_string(speed, me.item,
                                  conjugate_verb("use", plural) + " items",
                                  fast, slow);
        }

        if (speed >= 10)
        {
            if (did_speed && fast.size() == 1)
                result << " and " << fast[0];
            else if (!fast.empty())
            {
                if (did_speed)
                    result << ", ";
                result << comma_separated_line(fast.begin(), fast.end());
            }
            if (!slow.empty())
            {
                if (did_speed || !fast.empty())
                    result << ", but ";
                result << comma_separated_line(slow.begin(), slow.end());
            }
        }
        else if (speed < 10)
        {
            if (did_speed && slow.size() == 1)
                result << " and " << slow[0];
            else if (!slow.empty())
            {
                if (did_speed)
                    result << ", ";
                result << comma_separated_line(slow.begin(), slow.end());
            }
            if (!fast.empty())
            {
                if (did_speed || !slow.empty())
                    result << ", but ";
                result << comma_separated_line(fast.begin(), fast.end());
            }
        }
        result << ".\n";
    }
    else if (did_speed)
        result << ".\n";

    if (mi.type == MONS_SHADOW)
    {
        // Cf. monster::action_energy() in monster.cc.
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("cover", plural)
               << " ground more quickly when invisible.\n";
    }

    if (mi.airborne())
        result << uppercase_first(pronoun) << " can fly.\n";

    // Unusual regeneration rates.
    if (!mi.can_regenerate())
        result << uppercase_first(pronoun) << " cannot regenerate.\n";
    else if (mons_class_fast_regen(mi.type))
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("regenerate", plural) << " "
               << (mi.type == MONS_PARGHIT ? "astonishingly " : "")
               << "quickly.\n";

    const char* mon_size = get_size_adj(mi.body_size(), true);
    if (mon_size)
    {
        result << uppercase_first(pronoun) << " "
               << conjugate_verb("are", plural) << " "
               << mon_size << ".\n";
    }

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

    result << _monster_attacks_description(mi);
    result << _monster_missiles_description(mi);
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
        inf.body << _describe_demon(mi.mname, mi.airborne()) << "\n";
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
        if (mons.has_ench(ENCH_INSANE))
            inf.body << "; frenzied and insane (otherwise " << att << ")";
        else
            inf.body << "; " << att;
    }
    else if (mons.has_ench(ENCH_INSANE))
        inf.body << "; frenzied and insane";

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
    title_hbox->add_child(move(dgn));
#endif

    auto title = make_shared<Text>();
    title->set_text(inf.title);
    title->set_margin_for_sdl(0, 0, 0, 10);
    title_hbox->add_child(move(title));

    title_hbox->set_cross_alignment(Widget::CENTER);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    vbox->add_child(move(title_hbox));

    desc += inf.body.str();
    if (crawl_state.game_is_hints())
        desc += formatted_string(hints_describe_monster(mi, has_stat_desc));
    desc += inf.footer;
    string raw_desc = trimmed_string(desc); // forces us back to a colour string

#ifdef USE_TILE_WEB
    // the spell list is delimited so that we can later replace this substring
    // with a placeholder marker; remove the delimeters for console display
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

#ifdef USE_TILE_LOCAL
# define MORE_PREFIX "[<w>!</w>" "|<w>Right-click</w>" "]: "
#else
# define MORE_PREFIX "[<w>!</w>" "]: "
#endif

    const char* mores[2] = {
        MORE_PREFIX "<w>Description</w>|Quote",
        MORE_PREFIX "Description|<w>Quote</w>",
    };

    for (int i = 0; i < (inf.quote.empty() ? 1 : 2); i++)
    {
        const formatted_string *content[2] = { &desc, &quote };
        auto scroller = make_shared<Scroller>();
        auto text = make_shared<Text>(content[i]->trim());
        text->set_wrap_text(true);
        scroller->set_child(text);
        desc_sw->add_child(move(scroller));

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

    auto popup = make_shared<ui::Popup>(move(vbox));

    bool done = false;
    int lastch;
    popup->on_keydown_event([&](const KeyEvent& ev) {
        const auto key = ev.key();
        lastch = key;
        done = key == CK_ESCAPE;
        if (!inf.quote.empty() && (key == '!' || key == CK_MOUSE_CMD))
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
        const vector<pair<spell_type,char>> spell_map = map_chars_to_spells(spells, nullptr);
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
        if (start == string::npos || end == string::npos || start > end)
            desc_without_spells += "\n\nBUGGY SPELLSET\n\nSPELLSET_PLACEHOLDER";
        else
            desc_without_spells.replace(start, end, "SPELLSET_PLACEHOLDER");
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

    ui::run_layout(move(popup), done);

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
        "multiple will.";
    const string vanish_info
        = make_stringf("\n\nClouds of this kind an adventurer makes will vanish"
                       " %s once outside their sight.",
                       opaque ? "quickly" : "almost instantly");
    return opacity_info + vanish_info;
}
