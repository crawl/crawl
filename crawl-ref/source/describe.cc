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
#include "localise.h"
#include "macro.h"
#include "melee-attack.h" // describe_to_hit
#include "message.h"
#include "mon-behv.h"
#include "message-util.h"
#include "mon-cast.h" // mons_spell_range
#include "mon-death.h"
#include "mon-tentacle.h"
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
#include "spl-wpnench.h"
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

using namespace ui;


static void _print_bar(int value, int scale, string name,
                       ostringstream &result, int base_value = INT_MAX);

static void _describe_mons_to_hit(const monster_info& mi, ostringstream &result);

int count_desc_lines(const string &_desc, const int width)
{
    string desc = get_linebreak_string(_desc, width);
    return count(begin(desc), end(desc), '\n');
}

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
        { ARTP_BERSERK,               prop_note::plain },
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

    // For randart jewellery, note the base jewellery type if it's not
    // covered by artefact_desc_properties()
    if (item.base_type == OBJ_JEWELLERY
        && (item_ident(item, ISFLAG_KNOW_TYPE)))
    {
        const char* type = jewellery_base_ability_string(item.sub_type);
        if (*type)
            propnames.push_back(type);
    }
    else if (item_brand_known(item)
             && !(is_unrandom_artefact(item) && entry
                  && entry->flags & UNRAND_FLAG_SKIP_EGO))
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
    string name = localise(artp_name(prop));
    name = chop_string(name, MAX_ARTP_NAME_LEN - 1, false) + ":";
    name = chop_string(name, MAX_ARTP_NAME_LEN, true);
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
        { ARTP_AC, "It affects your AC (%+d).", false },
        { ARTP_EVASION, "It affects your evasion (%+d).", false},
        { ARTP_STRENGTH, "It affects your strength (%+d).", false},
        { ARTP_INTELLIGENCE, "It affects your intelligence (%+d).", false},
        { ARTP_DEXTERITY, "It affects your dexterity (%+d).", false},
        { ARTP_SLAYING, "It affects your accuracy and damage with ranged "
                        "weapons and melee (%+d).", false},
        { ARTP_FIRE, "fire", true},
        { ARTP_COLD, "cold", true},
        { ARTP_ELECTRICITY, "It insulates you from electricity.", false},
        { ARTP_POISON, "poison", true},
        { ARTP_NEGATIVE_ENERGY, "negative energy", true},
        { ARTP_HP, "It affects your health (%+d).", false},
        { ARTP_MAGICAL_POWER, "It affects your magic capacity (%+d).", false},
        { ARTP_SEE_INVISIBLE, "It lets you see invisible.", false},
        { ARTP_INVISIBLE, "It lets you turn invisible.", false},
        { ARTP_FLY, "It grants you flight.", false},
        { ARTP_BLINK, "It lets you blink.", false},
        { ARTP_BERSERK, "It lets you go berserk.", false},
        { ARTP_NOISE, "It may make noises in combat.", false},
        { ARTP_PREVENT_SPELLCASTING, "It prevents spellcasting.", false},
        { ARTP_CAUSE_TELEPORTATION, "It may teleport you next to monsters.", false},
        { ARTP_PREVENT_TELEPORTATION, "It prevents most forms of teleportation.",
          false},
        { ARTP_ANGRY,  "It may make you go berserk in combat.", false},
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
        { ARTP_SHIELDING, "It affects your SH (%+d).", false},
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

        string sdesc = localise(desc.desc, proprt[desc.property]);

        if (desc.is_graded_resist)
        {
            int idx = proprt[desc.property] + 3;
            idx = min(idx, 6);
            idx = max(idx, 0);

            const char* sentences[] =
            {
                "It makes you extremely vulnerable to %s.",
                "It makes you very vulnerable to %s.",
                "It makes you vulnerable to %s.",
                "Buggy descriptor!",
                "It protects you from %s.",
                "It greatly protects you from %s.",
                "It renders you almost immune to %s."
            };
            sdesc = localise(sentences[idx], sdesc);
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
        if (need_newline)
            description += "\n";
        description += _padded_artp_name(ARTP_WILLPOWER);
        description += " ";
        if (stval < -1)
            description += localise("It greatly decreases your willpower.");
        else if (stval > 1)
            description += localise("It greatly increases your willpower.");
        else if (stval < 0)
            description += localise("It decreases your willpower.");
        else
            description += localise("It increases your willpower.");
        need_newline = true;
    }

    if (known_proprt(ARTP_STEALTH))
    {
        const int stval = proprt[ARTP_STEALTH];
        if (need_newline)
            description += '\n';
        description += _padded_artp_name(ARTP_STEALTH);
        if (stval < -1)
            description += localise("It makes you much less stealthy.");
        else if (stval > 1)
            description += localise("It makes you much more stealthy.");
        else if (stval < 0)
            description += localise("It makes you less stealthy.");
        else
            description += localise("It makes you more stealthy.");
    }

    return description;
}
#undef known_proprt

// If item is an unrandart with a DESCRIP field, return its contents.
// Otherwise, return "".
static string _artefact_descrip(const item_def &item)
{
    if (!is_artefact(item)) return "";

    ostringstream out;
    if (is_unrandom_artefact(item))
    {
        bool need_newline = false;
        auto entry = get_unrand_entry(item.unrand_idx);
        if (entry->dbrand)
        {
            out << localise(entry->dbrand);
            need_newline = true;
        }
        if (entry->descrip)
        {
            out << (need_newline ? "\n\n" : "") << localise(entry->descrip);
            need_newline = true;
        }
        if (!_randart_descrip(item).empty())
            out << (need_newline ? "\n\n" : "") << _randart_descrip(item);
    }
    else
        out << _randart_descrip(item);

    // XXX: Can't happen, right?
    if (!item_ident(item, ISFLAG_KNOW_PROPERTIES) && item_type_known(item))
        out << "\n" << localise("It may have some hidden properties.");

    return out.str();
}

static const char *trap_names[] =
{
    "dart trap",
    "arrow trap", "spear trap",
#if TAG_MAJOR_VERSION > 34
    "dispersal trap",
    "teleport trap",
#endif
    "permanent teleport trap",
    "alarm trap", "blade trap",
    "bolt trap", "net trap", "Zot trap",
#if TAG_MAJOR_VERSION == 34
    "needle trap",
#endif
    "shaft", "passage of Golubria", "pressure plate", "web",
#if TAG_MAJOR_VERSION == 34
    "gas trap", "teleport trap",
    "shadow trap", "dormant shadow trap", "dispersal trap"
#endif
};

string trap_name(trap_type trap)
{
    if (trap == TRAP_GOLUBRIA)
        return "passage"; // @noloc
    else
        return replace_last(full_trap_name(trap), " trap", ""); // @noloc
}

string full_trap_name(trap_type trap)
{
    COMPILE_CHECK(ARRAYSZ(trap_names) == NUM_TRAPS);

    if (trap >= 0 && trap < NUM_TRAPS)
        return trap_names[trap];
    return "";
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
        if (tspec == lowercase_string(trap_name((trap_type)i)))
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

    const string start = "One of the many lords of Pandemonium, @name@ has ";

    static const char* body_types[] =
    {
        "an armoured body",
        "a vast, spindly body",
        "a fat body",
        "an obese body",
        "a muscular body",
        "a spiked body",
        "a splotchy body",
        "a slender body",
        "a tentacled body",
        "an emaciated body",
        "a bug-like body",
        "a skeletal body",
        "a mantis body",
        "a slithering body",
    };

    static const char* wing_names[] =
    {
        " with small, bat-like wings",
        " with bony wings",
        " with sharp, metallic wings",
        " with the wings of a moth",
        " with thin, membranous wings",
        " with dragonfly wings",
        " with large, powerful wings",
        " with fluttering wings",
        " with great, sinister wings",
        " with hideous, tattered wings",
        " with sparrow-like wings",
        " with hooked wings",
        " with strange knobs attached",
        " which hovers in mid-air",
        " with sacs of gas hanging from its back",
    };

    const char* head_names[] =
    {
        " and a cubic structure in place of a head",
        " and a brain for a head",
        " and a hideous tangle of tentacles for a mouth",
        " and the head of an elephant",
        " and an eyeball for a head",
        " and wears a helmet over its head",
        " and a horn in place of a head",
        " and a thick, horned head",
        " and the head of a horse",
        " and a vicious glare",
        " and snakes for hair",
        " and the face of a baboon",
        " and the head of a mouse",
        " and a ram's head",
        " and the head of a rhino",
        " and eerily human features",
        " and a gigantic mouth",
        " and a mass of tentacles growing from its neck",
        " and a thin, worm-like head",
        " and huge, compound eyes",
        " and the head of a frog",
        " and an insectoid head",
        " and a great mass of hair",
        " and a skull for a head",
        " and a cow's skull for a head",
        " and the head of a bird",
        " and a large fungus growing from its neck",
        " and an ominous eye at the end of a thin stalk",
        " and a face from nightmares",
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
    description << replace_all(localise(start), "@name@", name);

    description << HRANDOM_ELEMENT(body_types, 2);

    if (flying)
        description << HRANDOM_ELEMENT(wing_names, 3);

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
        "it seems unusually buggy",
        "it sports a set of venomous tails",
        "it flies swiftly and unpredictably",
        "it's breath smoulders ominously",
        "it is covered with eyes and tentacles",
        "it flickers and crackles with electricity",
        "it is covered in dense fur and muscle",
    };
    COMPILE_CHECK(ARRAYSZ(facet_descs) == NUM_BEAST_FACETS);

    if (facets.size() == 0)
        return "";

    string res = comma_separated_fn(begin(facets), end(facets),
                      [] (const CrawlStoreValue &sv) -> string {
                          const int facet = sv.get_int();
                          ASSERT_RANGE(facet, 0, NUM_BEAST_FACETS);
                          return facet_descs[facet];
                      }, ", and ", ", ");

    return uppercase_first(localise("%s.", res));
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
    else if (is_shield(item))
        return round(you.get_shield_skill_to_offset_penalty(item) * 10);
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
 * Whether it would make sense to set a training target for an item.
 *
 * @param item the item to check.
 * @param ignore_current whether to ignore any current training targets (e.g. if there is a higher target, it might not make sense to set a lower one).
 */
static bool _could_set_training_target(const item_def &item, bool ignore_current)
{
    if (!crawl_state.need_save || is_useless_item(item)
        || you.has_mutation(MUT_DISTRIBUTED_TRAINING))
    {
        return false;
    }

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
 * are relevant (weapons, missiles, shields)
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
        target_button_desc = localise(
            "; use <white>(s)</white> to set %d.%d as a target for %s.",
                                min_scaled_target / 10, min_scaled_target % 10,
                                skill_name(skill));
    }
    int you_skill_temp = you.skill(skill, 10);
    int you_skill = you.skill(skill, 10, false, false);

    string res;
    if (you_skill_temp == you_skill)
        res = "Your skill: %d.%d";
    else
        res = "Your (base) skill: %d.%d";

    return localise(res, you_skill / 10, you_skill % 10)
           + target_button_desc;
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

    const bool hypothetical = !crawl_state.need_save ||
                                    (training != you.training[skill]);

    const skill_diff diffs = skill_level_to_diffs(skill,
                                (double) scaled_target / 10, training, false);
    const int level_diff = xp_to_level_diff(diffs.experience / 10, 10);
    const bool level_maxed = (you.experience_level + (level_diff + 9) / 10) > 27;

    if (hypothetical && level_maxed)
        description = "At %d%% training you would reach %d.%d in the equivalent of %d.%d XLs.";
    else if (hypothetical)
        description = "At %d%% training you would reach %d.%d in about %d.%d XLs.";
    else if (level_maxed)
        description = "At current training (%d%%) you reach %d.%d in the equivalent of %d.%d XLs.";
    else
        description = "At current training (%d%%) you reach %d.%d in about %d.%d XLs.";

    description = localise(description,
            (int)(hypothetical ? training : you.training[skill]),
            scaled_target / 10, scaled_target % 10,
            level_diff / 10, level_diff % 10);
    if (you.wizard)
    {
        description += "\n    "; // @noloc
        description += localise("(%d xp, %d skp)",
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

static void _append_weapon_stats(string &description, const item_def &item)
{
    const int base_dam = property(item, PWPN_DAMAGE);
    const int ammo_type = fires_ammo_type(item);
    const int ammo_dam = ammo_type == MI_NONE ? 0 :
                                                ammo_type_damage(ammo_type);
    const skill_type skill = _item_training_skill(item);
    const int mindelay_skill = _item_training_target(item);

    const bool could_set_target = _could_set_training_target(item, true);

    if (skill == SK_SLINGS)
    {
        description += "\n";
        description += localise("Firing bullets:    Base damage: %d",
                                    base_dam +
                                    ammo_type_damage(MI_SLING_BULLET));
    }

    if (item.base_type == OBJ_STAVES
        && item_type_known(item)
        && staff_skill(static_cast<stave_type>(item.sub_type)) != SK_NONE
        && is_useless_skill(staff_skill(static_cast<stave_type>(item.sub_type))))
    {
        description += "\n";
        description += localise(
            "Your inability to study %s prevents you from drawing on the"
            " full power of this staff in melee.",
            skill_name(staff_skill(static_cast<stave_type>(item.sub_type))));
    }

    description += "\n";
    description += localise(
        "Base accuracy: %+d  Base damage: %d  Base attack delay: %.1f",
        property(item, PWPN_HIT),
        base_dam + ammo_dam,
        (float) property(item, PWPN_SPEED) / 10);

    description += "\n";
    description += localise(
        "This weapon's minimum attack delay (%.1f) is reached at skill level %d.",
        (float) weapon_min_delay(item, item_brand_known(item)) / 10,
        mindelay_skill / 10);

    if (!is_useless_item(item))
    {
        description += "\n    " + _your_skill_desc(skill,
                    could_set_target && in_inventory(item), mindelay_skill);
    }

    if (could_set_target)
        _append_skill_target_desc(description, skill, mindelay_skill);
}

static string _handedness_string(const item_def &item)
{
    const bool quad = you.has_mutation(MUT_QUADRUMANOUS);

    if (!quad)
    {
        if (you.hands_reqd(item) == HANDS_ONE)
            return localise("It is a one-handed weapon.");
        else
            return localise("It is a two-handed weapon.");
    }
    else
    {
        if (you.hands_reqd(item) == HANDS_ONE)
            return localise("It is a weapon for one hand-pair.");
        else
            return localise("It is a weapon for two hand-pairs.");
    }
 }

static string _describe_weapon(const item_def &item, bool verbose)
{
    string description;

    description.reserve(200);

    description = "";

    if (verbose)
    {
        description += "\n";
        _append_weapon_stats(description, item);
    }

    const int spec_ench = (is_artefact(item) || verbose)
                          ? get_weapon_brand(item) : SPWPN_NORMAL;
    const int damtype = get_vorpal_type(item);

    if (verbose)
    {
        switch (item_attack_skill(item))
        {
        case SK_POLEARMS:
            description += "\n\n";
            description += localise("It can be evoked to extend its reach.");
            break;
        case SK_AXES:
            description += "\n\n";
            description += localise("It hits all enemies adjacent to the wielder, "
                                    "dealing less damage to those not targeted.");
            break;
        case SK_LONG_BLADES:
            description += "\n\n";
            description += localise("It can be used to riposte, swiftly "
                                    "retaliating against a missed attack.");
            break;
        case SK_SHORT_BLADES:
            description += "\n\n";
            if (item.sub_type == WPN_DAGGER)
            {
                description += localise("It is extremely good for stabbing"
                                        " helpless or unaware enemies.");
            }
            else
            {
                description += localise("It is particularly good for stabbing"
                                        " helpless or unaware enemies.");
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
            {
                description += localise("Any ammunition fired from it burns "
                                        "those it strikes, dealing additional "
                                        "fire damage.");
            }
            else
            {
                description += localise("It burns those it strikes, dealing "
                                        "additional fire damage.");
            }
            if (!is_range_weapon(item) &&
                (damtype == DVORP_SLICING || damtype == DVORP_CHOPPING))
            {
                description += localise(" ");
                description += localise("Big, fiery blades are also staple "
                                        "armaments of hydra-hunters.");
            }
            break;
        case SPWPN_FREEZING:
            if (is_range_weapon(item))
            {
                description += localise("Any ammunition fired from it freezes "
                                        "those it strikes, dealing additional "
                                        "cold damage.");
            }
            else
            {
                description += localise("It freezes those it strikes, dealing "
                                        "additional cold damage.");
            }
            description += localise(" ");
            description += localise("It can also slow down cold-blooded creatures.");
            break;
        case SPWPN_HOLY_WRATH:
            if (is_range_weapon(item))
            {
                description += localise("It has been blessed by the Shining One"
                                        ", and any ammunition fired from it"
                                        " cause great damage to the undead and demons.");
            }
            else
            {
                description += localise("It has been blessed by the Shining One to"
                                        " cause great damage to the undead and demons.");
            }
            break;
        case SPWPN_ELECTROCUTION:
            if (is_range_weapon(item))
            {
                description += localise("Any ammunition fired from it "
                    "occasionally discharges a powerful burst of electricity "
                    "upon striking a foe.");
            }
            else
            {
                description += localise("It occasionally discharges a "
                    "powerful burst of electricity upon striking a foe.");
            }
            break;
        case SPWPN_VENOM:
            if (is_range_weapon(item))
            {
                description += localise("Any ammunition fired from it poisons "
                    "the flesh of those it strikes.");
            }
            else
                description += localise("It poisons the flesh of those it strikes.");
            break;
        case SPWPN_PROTECTION:
            description += localise("It grants its wielder temporary protection when "
                "it strikes (+7 to AC).");
            break;
        case SPWPN_DRAINING:
            description += localise("A truly terrible weapon, it drains the "
                "life of any living foe it strikes.");
            break;
        case SPWPN_SPEED:
            description += localise("Attacks with this weapon are significantly faster.");
            break;
        case SPWPN_VORPAL:
            if (is_range_weapon(item))
            {
                description += localise("Any ammunition fired from it inflicts "
                                        "extra damage upon your enemies.");
            }
            else
            {
                description += localise("It inflicts extra damage upon your "
                                        "enemies.");
            }
            break;
        case SPWPN_CHAOS:
            if (is_range_weapon(item))
            {
                description += localise("Each projectile launched from it has a "
                                        "different, random effect.");
            }
            else
            {
                description += localise("Each time it hits an enemy it has a "
                                        "different, random effect.");
            }
            break;
        case SPWPN_VAMPIRISM:
            description += localise("It occasionally heals its wielder for a portion "
                "of the damage dealt when it wounds a living foe.");
            break;
        case SPWPN_PAIN:
            description += localise("In the hands of one skilled in necromantic "
                "magic, it inflicts extra damage on living creatures.");
            break;
        case SPWPN_DISTORTION:
            description += localise("It warps and distorts space around it, and may "
                "blink, banish, or inflict extra damage upon those it strikes. "
                "Unwielding it can cause banishment or high damage.");
            break;
        case SPWPN_PENETRATION:
            description += localise("Any ammunition fired by it passes through the "
                "targets it hits, potentially hitting all targets in "
                "its path until it reaches maximum range.");
            break;
        case SPWPN_REAPING:
            description += localise("Any living foe damaged by it may be reanimated "
                "upon death as a zombie friendly to the wielder, with an "
                "increasing chance as more damage is dealt.");
            break;
        case SPWPN_ANTIMAGIC:
            description += localise("It reduces the magical energy of the wielder, "
                "and disrupts the spells and magical abilities of those it "
                "strikes. Natural abilities and divine invocations are not "
                "affected.");
            break;
        case SPWPN_SPECTRAL:
            description += localise("When its wielder attacks, the weapon's spirit "
                "leaps out and strikes again. The spirit shares a part of "
                "any damage it takes with its wielder.");
            break;
        case SPWPN_ACID:
             if (is_range_weapon(item))
             {
                description += localise("Any ammunition fired from it "
                    "is coated in acid, damaging and corroding those it "
                    "strikes.");
             }
            else
                description += localise("It is coated in acid, damaging and "
                    "corroding those it strikes.");
            break;
        case SPWPN_NORMAL:
            ASSERT(enchanted);
            description += localise("It has no special brand (it is not flaming, "
                "freezing, etc), but is still enchanted in some way - "
                "positive or negative.");
            break;
        }
    }

    if (you.duration[DUR_EXCRUCIATING_WOUNDS] && &item == you.weapon())
    {
        description += "\n";
        if ((int) you.props[ORIGINAL_BRAND_KEY] == SPWPN_NORMAL)
        {
            description += localise("It is temporarily rebranded; it is "
                                    "actually an unbranded weapon.");
        }
        else
        {
            brand_type original = static_cast<brand_type>(
                you.props[ORIGINAL_BRAND_KEY].get_int());
            string weap_desc = article_a(
                weapon_brand_desc("weapon", item, false, original), true);
            description += localise("It is temporarily rebranded; it is actually %s.",
                                    weap_desc);
        }
    }

    string art_desc = _artefact_descrip(item);
    if (!art_desc.empty())
        description += "\n\n" + art_desc;

    if (verbose)
    {
        const skill_type skill = item_attack_skill(item);
        description += "\n\n";
        if (is_unrandom_artefact(item))
        {
            string base = article_a(get_artefact_base_name(item));
            description += localise("This is %s.", base);
            description += localise(" ");
        }
        description += localise("This weapon falls into the '%s' category.",
                         skill == SK_FIGHTING ? "buggy" : skill_name(skill));

        // XX this is shown for felids, does that actually make sense?
        description += localise(" ");
        description += _handedness_string(item);

        if (!you.could_wield(item, true, true) && crawl_state.need_save)
        {
            description += "\n";
            description += localise("It is too large for you to wield.");
        }
    }

    if (!is_artefact(item))
    {
        description += "\n";
        if (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus >= MAX_WPN_ENCHANT)
            description += localise("It cannot be enchanted further.");
        else
        {
            description += localise("It can be maximally enchanted to +%d.",
                                    MAX_WPN_ENCHANT);
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
            description += "It has a random effect on any target it hits.";
            break;
        case SPMSL_POISONED:
            description += "It is coated with poison.";
            break;
        case SPMSL_CURARE:
            description += "It is tipped with a substance that causes "
                           "asphyxiation, dealing direct damage as well as "
                           "poisoning and slowing those it strikes.\n"
                           "It is twice as likely to be destroyed on impact as "
                           "other darts.";
            break;
        case SPMSL_FRENZY:
            description += "It is tipped with a substance that sends those it "
                           "hits into a mindless rage, attacking friend and "
                           "foe alike.\n"
                           "The chance of successfully applying its effect "
                           "increases with Throwing and Stealth skill.";

            break;
        case SPMSL_BLINDING:
            description += "It is tipped with a substance that causes "
                           "blindness and brief confusion.\n"
                           "The chance of successfully applying its effect "
                           "increases with Throwing and Stealth skill.";
            break;
        case SPMSL_DISPERSAL:
            description += "It causes any target it hits to blink, with a "
                           "tendency towards blinking further away from the "
                           "attacker.";
            break;
        case SPMSL_SILVER:
            description += "It deals increased damage compared to normal ammo "
                           "and substantially increased damage to chaotic "
                           "and magically transformed beings. It also inflicts "
                           "extra damage against mutated beings, according to "
                           "how mutated they are.";
            break;
        }

        description = "\n\n" + localise(description);
    }

    const int dam = property(item, PWPN_DAMAGE);
    const bool player_throwable = is_throwable(&you, item);
    if (player_throwable)
    {
        const int throw_delay = (10 + dam / 2);
        const int target_skill = _item_training_target(item);
        const bool could_set_target = _could_set_training_target(item, true);

        description += "\n";
        description += localise(
            "Base damage: %d  Base attack delay: %.1f",
            dam,
            (float) throw_delay / 10
        );
        description += "\n";
        description += localise(
            "This projectile's minimum attack delay (%.1f) "
                "is reached at skill level %d.",
            (float) FASTEST_PLAYER_THROWING_SPEED / 10,
            target_skill / 10
        );

        if (!is_useless_item(item))
        {
            description += "\n    " +
                    _your_skill_desc(SK_THROWING,
                        could_set_target && in_inventory(item), target_skill);
        }
        if (could_set_target)
            _append_skill_target_desc(description, SK_THROWING, target_skill);
    }

    if (ammo_always_destroyed(item))
    {
        description += "\n\n";
        description += localise("It is always destroyed on impact.");
    }
    else if (!ammo_never_destroyed(item))
    {
        description += "\n\n";
        description += localise("It may be destroyed on impact.");
    }

    return description;
}

static string _warlock_mirror_reflect_desc()
{
    const int SH = crawl_state.need_save ? player_shield_class() : 0;
    const int reflect_chance = 100 * SH / omnireflect_chance_denom(SH);
    string desc = "\n\n";
    desc += localise("With your current SH, it has a %d%% chance to reflect "
                     "attacks against your willpower and other normally "
                     "unblockable effects.", reflect_chance);
    return desc;
}

static string _armour_ac_sub_change_description(const item_def &item)
{
    string description;

    description.reserve(100);

    int you_ac_with_this_item =
                 you.armour_class_with_one_sub(item);

    int diff = you_ac_with_this_item - you.armour_class();

    description += "\n\n";
    if (diff > 0)
    {
        description += localise("If you switch to wearing this armour, "
                                "your AC would increase by %d (%d -> %d).",
                                diff, you.armour_class(), you_ac_with_this_item);
    }
    else if (diff < 0)
    {
        description += localise("If you switch to wearing this armour, "
                                "your AC would decrease by %d (%d -> %d).",
                                abs(diff), you.armour_class(), you_ac_with_this_item);
    }
    else
    {
        description += localise("If you switch to wearing this armour, "
                                "your AC would remain unchanged.");
    }

    return description;
}

static string _armour_ac_remove_change_description(const item_def &item)
{
    string description;

    int you_ac_without_item =
                 you.armour_class_with_one_removal(item);

    int diff = you_ac_without_item - you.armour_class();

    description += "\n\n";
    if (diff > 0)
    {
        description += localise("If you remove this armour, "
                                "your AC would increase by %d (%d -> %d).",
                                diff, you.armour_class(), you_ac_without_item);
    }
    else if (diff < 0)
    {
        description += localise("If you remove this armour, "
                                "your AC would decrease by %d (%d -> %d).",
                                abs(diff), you.armour_class(), you_ac_without_item);
    }
    else
    {
        description += localise("If you remove this armour, "
                                "your AC would remain unchanged.");
    }

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
    case SPARM_ARCHERY:
        return "it improves its wearer's accuracy and damage with "
               "ranged weapons, such as bows and javelins (Slay +4).";
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
    default:
        return "it makes the wearer crave the taste of eggplant.";
    }
}

static string _describe_armour(const item_def &item, bool verbose)
{
    string description;

    description.reserve(300);

    if (verbose)
    {
        if (is_shield(item))
        {
            const int target_skill = _item_training_target(item);
            description += "\n\n";
            description += localise("Base shield rating: %d",
                                    property(item, PARM_AC));
            const bool could_set_target = _could_set_training_target(item, true);

            if (!is_useless_item(item))
            {
                description += "       ";
                description += localise("Skill to remove penalty: %d.%d",
                                        target_skill / 10, target_skill % 10);

                if (crawl_state.need_save)
                {
                    description += "\n    "
                        + _your_skill_desc(SK_SHIELDS,
                          could_set_target && in_inventory(item), target_skill);
                }
                else
                    description += "\n";
                if (could_set_target)
                {
                    _append_skill_target_desc(description, SK_SHIELDS,
                                                                target_skill);
                }
            }

            if (is_unrandom_artefact(item, UNRAND_WARLOCK_MIRROR))
                description += _warlock_mirror_reflect_desc();
        }
        else
        {
            const int evp = property(item, PARM_EVASION);
            description += "\n\n";
            description += localise("Base armour rating: %d",
                                    property(item, PARM_AC));
            if (get_armour_slot(item) == EQ_BODY_ARMOUR)
            {
                description += "       ";
                description += localise("Encumbrance rating: %d", -evp / 10);
            }
            // Bardings reduce evasion by a fixed amount, and don't have any of
            // the other effects of encumbrance.
            else if (evp)
            {
                description += "       ";
                description += localise("Evasion: %d", evp / 30);
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
            string name = localise(string(armour_ego_name(item, true)));
            name = chop_string(name, MAX_ARTP_NAME_LEN - 1, false) + ":";
            name.append(MAX_ARTP_NAME_LEN - strwidth(name), ' ');
            description += name;
        }
        else
        {
            // i18n: need space in front of "of" for translation (because
            // normally it's in the context of "<thing> of <ego>").
            string ego_name = " of " + string(armour_ego_name(item, false));
            ego_name = localise(ego_name);
            // now remove that space
            if (starts_with(ego_name, " "))
                ego_name = ego_name.substr(1);
            ego_name = uppercase_first(ego_name);
            description += localise("'%s': ", LocalisationArg(ego_name, false));
        }

        string ego_desc = localise(_item_ego_desc(ego));
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

    if (!is_artefact(item))
    {
        const int max_ench = armour_max_enchant(item);
        if (max_ench > 0)
        {
            description += "\n\n";
            if (item.plus < max_ench || !item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                description += localise("It can be maximally enchanted to +%d.",
                                        max_ench);
            }
            else
                description += localise("It cannot be enchanted further.");
        }

    }

    // Only displayed if the player exists (not for item lookup from the menu).
    if (crawl_state.need_save
        && can_wear_armour(item, false, true)
        && item_ident(item, ISFLAG_KNOW_PLUSES)
        && !is_shield(item))
    {
        description += _armour_ac_change(item);
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

    return localise("If you quaff this potion your AC would be %d.",
                    treeform_ac);
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
            description += "\n\n";
            description += localise("It affects your shielding (%+d).",
                                    AMU_REFLECT_SH / 2);
        }
        else if (item.plus != 0)
        {
            switch (item.sub_type)
            {
            case RING_PROTECTION:
                description += "\n\n";
                description += localise("It affects your AC (%+d).",
                                        item.plus);
                break;

            case RING_EVASION:
                description += "\n\n";
                description += localise("It affects your evasion (%+d).",
                                        item.plus);
                break;

            case RING_STRENGTH:
                description += "\n\n";
                description += localise("It affects your strength (%+d).",
                                        item.plus);
                break;

            case RING_INTELLIGENCE:
                description += "\n\n";
                description += localise("It affects your intelligence (%+d).",
                                        item.plus);
                break;

            case RING_DEXTERITY:
                description += "\n\n";
                description += localise("It affects your dexterity (%+d).",
                                        item.plus);
                break;

            case RING_SLAYING:
                description += "\n\n";
                description += localise("It affects your accuracy and"
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
        return "\n" + localise("It has a curse placed upon it.");

    const CrawlVector& curses = item.props[CURSE_KNOWLEDGE_KEY].get_vector();

    if (curses.empty())
        return "\n" + localise("It has a curse placed upon it.");

    ostringstream desc;

    desc << "\n"
         << localise("It has a curse which improves the following skills:");
    for (auto curse: curses)
    {
        desc << "\n";
        desc << desc_curse_skills(curse) << localise(".");
    }
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
 * @param verbose Controls various switches for the length of the description.
 * @param dump    This controls which style the name is shown in.
 * @param lookup  If true, the name is not shown at all.
 *   If either of those two are true, the DB description is not shown.
 * @return a string with the name, db desc, and some other data.
 */
string get_item_description(const item_def &item, bool verbose,
                            bool dump, bool lookup)
{
    ostringstream description;

#ifdef DEBUG_DIAGNOSTICS
    if (!dump && !you.suppress_wizard)
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
        description << "\n\n";

        bool need_base_desc = !lookup;

        if (dump)
        {
            description << "[" // @noloc
                        << localise(item.name(DESC_DBNAME, true, false, false))
                        << "]"; // @noloc
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
            description << localise("It is an ancient artefact.");
            need_base_desc = false;
        }

        if (need_base_desc)
        {
            string db_name = item.name(DESC_DBNAME, true, false, false);
            string db_desc = getLongDescription(db_name);

            if (db_desc.empty())
            {
                if (item_type_removed(item.base_type, item.sub_type))
                    description << localise("This item has been removed.") << "\n";
                else if (item_type_known(item))
                {
                    description << localise("[ERROR: no desc for item name '%s'"
                                            "]. Perhaps this item has been removed?",
                                            db_name);
                    description << "\n";
                }
                else
                {
                    string name = item.name(DESC_A, true, false, false);
                    name = add_punctuation(name, ".", true);
                    description << uppercase_first(name);
                    description << "\n";
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
        desc = _describe_weapon(item, verbose);
        if (desc.empty())
            need_extra_line = false;
        else
            description << desc;
        break;

    case OBJ_ARMOUR:
        desc = _describe_armour(item, verbose);
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
        if (!verbose && is_random_artefact(item))
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
            string stats = "\n";
            _append_weapon_stats(stats, item);
            description << stats;
        }
        description << "\n\n";
        description << localise("It falls into the 'Staves' category.");
        description << localise(" ");
        description << _handedness_string(item);
        break;

    case OBJ_MISCELLANY:
        if (item.sub_type == MISC_ZIGGURAT && you.zigs_completed)
        {
            const int zigs = you.zigs_completed;
            description << "\n\n";
            string glow_msg;
            if (zigs >= 27)
                glow_msg = "It is surrounded by a blinding glow."; // just plain silly
            else if (zigs >= 9)
                glow_msg = "It is surrounded by a dazzling glow.";
            else if (zigs >= 3)
                glow_msg = "It is surrounded by a bright glow.";
            else
                glow_msg = "It is surrounded by a gentle glow.";
            description << localise(glow_msg);
        }
        if (is_xp_evoker(item))
        {
            description << "\n\n";
            if (item_is_horn_of_geryon(item))
            {
                description << localise("Once activated, this device is "
                                        "rendered temporarily inert.");
                description << localise(" However, it recharges as you gain experience.");
            }
            else
            {
                if (item.sub_type == MISC_LIGHTNING_ROD)
                {
                    description << localise("Once all charges have been used, "
                                       "this device and all other devices of "
                                       "its kind are rendered temporarily inert.");
                }
                else
                {
                    description << localise("Once activated, this device and "
                                       "all other devices of its kind are "
                                       "rendered temporarily inert.");
                }
                description << localise(" However, they recharge as you gain experience.");
            }

            if (evoker_charges(item.sub_type) == 0)
                description << localise(" The device is presently inert.");
        }
        break;

    case OBJ_POTIONS:
        if (verbose && item_type_known(item))
        {
            if (item.sub_type == POT_LIGNIFY)
                description << "\n\n" + _describe_lignify_ac();
            else if (item.sub_type == POT_CANCELLATION)
            {
                if (player_is_cancellable())
                {
                    description << "\n\n"
                                << localise("If you drink this now, you will no longer be %s.",
                                            describe_player_cancellation());
                }
                else
                    description << "\n\nDrinking this now will have no effect.";
            }
        }
        break;

    case OBJ_WANDS:
        if (item_type_known(item))
        {
            description << "\n";

            const spell_type spell = spell_in_wand(static_cast<wand_type>(item.sub_type));

            const string damage_str = spell_damage_string(spell, true);
            if (damage_str != "")
                description << "\n" << localise("Damage: ") << damage_str;

            description << "\n" << localise("Noise: ")
                        << spell_noise_string(spell);
        }
        break;

    case OBJ_SCROLLS:
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
    else
    {
        if (verbose)
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
                    description << "\n";
                    description << localise("This ancient artefact cannot be "
                                       "changed by magic or mundane means.");
                }
                // Randart jewellery has already displayed this line.
                else if (item.base_type != OBJ_JEWELLERY
                         || (item_type_known(item) && is_unrandom_artefact(item)))
                {
                    description << "\n" << localise("It is an ancient artefact.");
                }
            }
        }
    }

    if (god_hates_item(item))
    {
        description << "\n\n";
        description << uppercase_first(
                           localise("%s disapproves of the use of such an item.",
                                    god_name(you.religion)));
    }

    if (verbose && origin_describable(item))
        description << "\n" << origin_desc(item, true) << ".";

    // This information is obscure and differs per-item, so looking it up in
    // a docs file you don't know to exist is tedious.
    if (verbose)
    {
        description << "\n\n" << localise("Stash search prefixes: ")
                    << userdef_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
        string menu_prefix = item_prefix(item, false);
        if (!menu_prefix.empty())
            description << "\n" << localise("Menu/colouring prefixes: ") << menu_prefix;
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
    {
        ret = localise("A cloud of %s.", cl_name);
        ret += (cl_desc.empty() ? "" : "\n\n");
    }
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
            localise("A covering of ice."),
            getLongDescription("ice covered"),
            tile_def(TILE_FLOOR_ICY)
        });
    }
    else if (!feat_is_solid(feat))
    {
        if (haloed(pos) && !umbraed(pos))
        {
            ret.push_back({
                localise("A halo."),
                getLongDescription("haloed"),
                tile_def(TILE_HALO_RANGE)
            });
        }
        if (umbraed(pos) && !haloed(pos))
        {
            ret.push_back({
                localise("An umbra."),
                getLongDescription("umbraed"),
                tile_def(TILE_UMBRA)
            });
        }
        if (liquefied(pos))
        {
            ret.push_back({
                localise("Liquefied ground."),
                getLongDescription("liquefied"),
                tile_def(TILE_LIQUEFACTION)
            });
        }
        if (disjunction_haloed(pos))
        {
            ret.push_back({
                localise("Translocational energy."),
                getLongDescription("disjunction haloed"),
                tile_def(TILE_DISJUNCT)
            });
        }
    }
    if (const auto cloud = env.map_knowledge(pos).cloudinfo())
    {
        ret.push_back({
            localise("A cloud of %s.", cloud_type_name(cloud->type)),
            get_cloud_desc(cloud->type, false),
            tile_def(tileidx_cloud(*cloud)),
        });
    }
    return ret;
}

void get_feature_desc(const coord_def &pos, describe_info &inf, bool include_extra)
{
    dungeon_feature_type feat = env.map_knowledge(pos).feat();

    string desc      = feature_description_at(pos, false, DESC_A);
    string db_name   = feat == DNGN_ENTER_SHOP ? "a shop" : desc; // @noloc
    strip_suffix(db_name, " (summoned)");
    string long_desc = getLongDescription(db_name);

    inf.title = uppercase_first(localise(desc));
    if (!ends_with(desc, ".") && !ends_with(desc, "!")
        && !ends_with(desc, "?"))
    {
        inf.title = add_punctuation(inf.title, ".", false);
    }

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

        if (feat_is_stone_stair(feat) || feat_is_escape_hatch(feat))
        {
            if (is_unknown_stair(pos))
            {
                long_desc += "\nYou have not yet explored it and cannot tell ";
                long_desc += "where it leads.";
            }
            else
            {
                long_desc +=
                    make_stringf("\nYou can view the location it leads to by "
                                 "examining it with <w>%s</w> and pressing "
                                 "<w>%s</w>.",
                                 command_to_string(CMD_DISPLAY_MAP).c_str(),
                                 command_to_string(
                                     feat_stair_direction(feat) ==
                                         CMD_GO_UPSTAIRS ? CMD_MAP_PREV_LEVEL
                                         : CMD_MAP_NEXT_LEVEL).c_str());
            }
        }
    }

    // mention the ability to pray at altars
    if (feat_is_altar(feat))
    {
        long_desc += "\n";
        long_desc += localise("(Pray here with <w>'%s'</w> to learn more.)",
                              command_to_string(CMD_GO_DOWNSTAIRS));
        long_desc += "\n";
    }

    // mention that permanent trees are usually flammable
    // (expect for autumnal trees in Wucad Mu's Monastery)
    if (feat_is_flammable(feat) && !is_temp_terrain(pos)
        && env.markers.property_at(pos, MAT_ANY, "veto_destroy") != "veto")
    {
        if (feat == DNGN_TREE)
            long_desc += "\n" + getLongDescription("tree burning");
        else if (feat == DNGN_MANGROVE)
            long_desc += "\n" + getLongDescription("mangrove burning");
        else if (feat == DNGN_DEMONIC_TREE)
            long_desc += "\n" + getLongDescription("demonic tree burning");
    }

    // mention that diggable walls are
    if (feat_is_diggable(feat)
        && env.markers.property_at(pos, MAT_ANY, "veto_destroy") != "veto")
    {
        long_desc += "\n" + localise("It can be dug through.");
    }

    inf.body << long_desc;

    if (include_extra)
    {
        const auto extra_descs = _get_feature_extra_descs(pos);
        for (const auto &d : extra_descs)
            inf.body << (d.title == extra_descs.back().title ? "" : "\n") << d.description;
    }

    inf.quote = getQuoteString(db_name);
}

void describe_feature_wide(const coord_def& pos)
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
        feats.emplace_back(f);
    }
    auto extra_descs = _get_feature_extra_descs(pos);
    for (const auto &desc : extra_descs)
    {
        feat_info f = { "", "", "", tile_def(TILEG_TODO)};
        f.title = desc.title;
        f.body = trimmed_string(desc.description);
        f.tile = desc.tile;
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
#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif
    scroller->set_child(move(vbox));

    auto popup = make_shared<ui::Popup>(scroller);

    bool done = false;
    popup->on_keydown_event([&](const KeyEvent& ev) {
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
    tiles.push_ui_layout("describe-feature-wide", 0);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    ui::run_layout(move(popup), done);
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
    const bool verbose = !item.has_spells();
    string name = item.name(DESC_INVENTORY_EQUIP);
    name = add_punctuation(name, ".", true);
    if (!in_inventory(item))
        name = uppercase_first(name);
    inf.body << name << get_item_description(item, verbose);
}

static vector<command_type> _allowed_actions(const item_def& item)
{
    vector<command_type> actions;
    actions.push_back(CMD_ADJUST_INVENTORY);
    if (item_equip_slot(item) == EQ_WEAPON)
        actions.push_back(CMD_UNWIELD_WEAPON);
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
        if (_could_set_training_target(item, false))
            actions.push_back(CMD_SET_SKILL_TARGET);
        if (!item_is_equipped(item))
        {
            if (item_is_wieldable(item))
                actions.push_back(CMD_WIELD_WEAPON);
        }
        break;
    case OBJ_MISSILES:
        if (_could_set_training_target(item, false))
            actions.push_back(CMD_SET_SKILL_TARGET);
        if (!you.has_mutation(MUT_NO_GRASPING))
            actions.push_back(CMD_QUIVER_ITEM);
        break;
    case OBJ_ARMOUR:
        if (_could_set_training_target(item, false))
            actions.push_back(CMD_SET_SKILL_TARGET);
        if (item_is_equipped(item))
            actions.push_back(CMD_REMOVE_ARMOUR);
        else
            actions.push_back(CMD_WEAR_ARMOUR);
        break;
    case OBJ_SCROLLS:
    //case OBJ_BOOKS: these are handled differently
        actions.push_back(CMD_READ);
        break;
    case OBJ_JEWELLERY:
        if (item_is_equipped(item))
            actions.push_back(CMD_REMOVE_JEWELLERY);
        else
            actions.push_back(CMD_WEAR_JEWELLERY);
        break;
    case OBJ_POTIONS:
        if (you.can_drink()) // mummies and lich form forbidden
            actions.push_back(CMD_QUAFF);
        break;
    default:
        ;
    }
    if (clua.callbooleanfn(false, "ch_item_wieldable", "i", &item))
        actions.push_back(CMD_WIELD_WEAPON);

    if (item_is_evokable(item))
    {
        actions.push_back(CMD_QUIVER_ITEM);
        actions.push_back(CMD_EVOKE);
    }

    actions.push_back(CMD_DROP);

    if (!crawl_state.game_is_tutorial())
        actions.push_back(CMD_INSCRIBE_ITEM);

    return actions;
}

static string _actions_desc(const vector<command_type>& actions)
{
    static const map<command_type, string> act_str =
    {
        { CMD_WIELD_WEAPON, "(w)ield" },
        { CMD_UNWIELD_WEAPON, "(u)nwield" },
        { CMD_QUIVER_ITEM, "(q)uiver" },
        { CMD_WEAR_ARMOUR, "(w)ear" },
        { CMD_REMOVE_ARMOUR, "(t)ake off" },
        { CMD_EVOKE, "e(v)oke" },
        { CMD_READ, "(r)ead" },
        { CMD_WEAR_JEWELLERY, "(p)ut on" },
        { CMD_REMOVE_JEWELLERY, "(r)emove" },
        { CMD_QUAFF, "(q)uaff" },
        { CMD_DROP, "(d)rop" },
        { CMD_INSCRIBE_ITEM, "(i)nscribe" },
        { CMD_ADJUST_INVENTORY, "(=)adjust" },
        { CMD_SET_SKILL_TARGET, "(s)kill" },
    };
    string ret = comma_separated_fn(begin(actions), end(actions),
                                [] (command_type cmd)
                                {
                                    return act_str.at(cmd);
                                },
                                ", or ");
    return localise("%s.", ret);
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
        { CMD_QUIVER_ITEM,      'q' },
        { CMD_WEAR_ARMOUR,      'w' },
        { CMD_REMOVE_ARMOUR,    't' },
        { CMD_EVOKE,            'v' },
        { CMD_READ,             'r' },
        { CMD_WEAR_JEWELLERY,   'p' },
        { CMD_REMOVE_JEWELLERY, 'r' },
        { CMD_QUAFF,            'q' },
        { CMD_DROP,             'd' },
        { CMD_INSCRIBE_ITEM,    'i' },
        { CMD_ADJUST_INVENTORY, '=' },
        { CMD_SET_SKILL_TARGET, 's' },
    };

    key = tolower_safe(key);

    for (auto cmd : actions)
        if (key == act_key.at(cmd))
            return cmd;

    return CMD_NO_CMD;
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

    const int slot = item.link;
    ASSERT_RANGE(slot, 0, ENDOFPACK);

    switch (action)
    {
    case CMD_WIELD_WEAPON:     wield_weapon(true, slot);            break;
    case CMD_UNWIELD_WEAPON:   wield_weapon(true, SLOT_BARE_HANDS); break;
    case CMD_QUIVER_ITEM:      you.quiver_action.set_from_slot(slot); break;
    case CMD_WEAR_ARMOUR:      wear_armour(slot);                   break;
    case CMD_REMOVE_ARMOUR:    takeoff_armour(slot);                break;
    case CMD_READ:             read(&item);                         break;
    case CMD_WEAR_JEWELLERY:   puton_ring(slot);                    break;
    case CMD_REMOVE_JEWELLERY: remove_ring(slot, true);             break;
    case CMD_QUAFF:            drink(&item);                        break;
    case CMD_DROP:             drop_item(slot, item.quantity);      break;
    case CMD_INSCRIBE_ITEM:    inscribe_item(item);                 break;
    case CMD_ADJUST_INVENTORY: adjust_item(slot);                   break;
    case CMD_SET_SKILL_TARGET: target_item(item);                   break;
    case CMD_EVOKE:
        evoke_item(slot);
        break;
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

    string name = item.name(DESC_INVENTORY_EQUIP);
    name = add_punctuation(name, ".", true);
    if (!in_inventory(item))
        name = uppercase_first(name);

    string desc = get_item_description(item, true, false);

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
    string desc_without_spells = fs_desc.to_colour_string();
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
            footer_text.cprintf(localise("Select a spell, or "));
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
 *  @return whether to remain in the inventory menu after description
 *
 */
bool describe_item(item_def &item, function<void (string&)> fixup_desc)
{

    const bool do_actions = in_inventory(item) // Dead men use no items.
            && !(you.pending_revival || crawl_state.updating_scores);
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
    string description = "\n";
    description += localise("Level: %d", spell_difficulty(spell));

    const string schools = spell_schools_string(spell);
    description += "        ";
    if (schools.find("/") == string::npos)
        description += localise("School: ");
    else
        description += localise("Schools: ");
    description += schools;

    if (!crawl_state.need_save
        || (get_spell_flags(spell) & spflag::monster))
    {
        return description; // all other info is player-dependent
    }

    int failure;
    if (you.divine_exegesis)
        failure = 0;
    else
        failure = raw_spell_fail(spell);
    description += "        " + localise("Fail: %d%%", failure);

    const string damage_string = spell_damage_string(spell);
    const int acc = spell_acc(spell);

    // the following labels are right-aligned
    int width = 0;

    string power_label = localise("Power: ");
    width = max(width, strwidth(power_label));

    string damage_label;
    if (!damage_string.empty())
    {
        damage_label = localise("Damage: ");
        width = max(width, strwidth(damage_label));
    }

    string acc_label;
    if (acc != -1)
    {
        acc_label = localise("Accuracy: ");
        width = max(width, strwidth(acc_label));
    }

    string range_label = localise("Range: ");
    width = max(width, strwidth(range_label));

    string noise_label = localise("Noise: ");
    width = max(width, strwidth(noise_label));

    description += "\n\n" + chop_string(power_label, width, true, true);
    description += spell_power_string(spell);

    if (damage_string != "")
    {
        description += "\n" + chop_string(damage_label, width, true, true);
        description += damage_string;
    }
    if (acc != -1)
    {
        ostringstream acc_str;
        _print_bar(acc, 3, "", acc_str);
        description += "\n" + chop_string(acc_label, width, true, true);
        description += acc_str.str();
    }

    description += "\n" + chop_string(range_label, width, true, true);
    description += spell_range_string(spell);
    description += "\n" + chop_string(noise_label, width, true, true);
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

    switch (skill)
    {
        case SK_INVOCATIONS:
            if (you.has_mutation(MUT_FORLORN))
            {
                result += "\n";
                result += localise("How on earth did you manage to pick this up?");
            }
            else if (you_worship(GOD_TROG))
            {
                result += "\n";
                result += localise("Note that Trog doesn't use Invocations, due to its "
                          "close connection to magic.");
            }
            break;

        case SK_SPELLCASTING:
            if (you_worship(GOD_TROG))
            {
                result += "\n";
                result += localise("Keep in mind, though, that Trog would greatly "
                          "disapprove of this.");
            }
            break;
        default:
            // No further information.
            break;
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
int hex_chance(const spell_type spell, const int hd)
{
    const int capped_pow = _hex_pow(spell, hd);
    const int chance = hex_success_chance(you.willpower(), capped_pow,
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
        { spschool::hexes, "debuffs and slows you" },
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
        descs.push_back(localise("deals up to %d %s damage", dam,
                                     comma_separated_line(dam_flavors.begin(),
                                                         dam_flavors.end(),
                                                         " or ").c_str()));
    }

    return comma_separated_line(descs.begin(), descs.end(),
                                localise(" or "), localise("; "));
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

    if (fail_severity(spell) == 0)
        description << localise("Miscasting this spell causes magic contamination.");
    else
    {
        string dmg_str = _miscast_damage_string(spell);
        description << localise("Miscasting this spell causes magic "
                                "contamination and also %s.",
                                LocalisationArg(dmg_str, false));
    }
    description << "\n";

    if (spell == SPELL_SPELLFORGED_SERVITOR)
    {
        spell_type servitor_spell = player_servitor_spell();
        if (servitor_spell == SPELL_NO_SPELL)
            description << localise("Your servitor would be unable to mimic any of your spells.");
        else
        {
            description << localise("Your servitor casts %s.",
                                    spell_title(player_servitor_spell()));
        }
        description << "\n";
    }

    // Report summon cap
    const int limit = summons_limit(spell);
    if (limit == 1)
    {
        description << localise("You can sustain at most 1 creature "
                                "summoned by this spell.");
        description << "\n";
    }
    else if (limit > 1)
    {
        description << localise("You can sustain at most %d creatures "
                                "summoned by this spell.", limit);
        description << "\n";
    }

    if (god_hates_spell(spell, you.religion))
    {
        description <<
            uppercase_first(localise("%s frowns upon the use of this spell.",
                                     god_name(you.religion)));
        description << "\n";

        if (god_loathes_spell(spell, you.religion))
        {
            description << localise("You'd be excommunicated if you dared to cast it!");
            description << "\n";
        }
    }
    else if (god_likes_spell(spell, you.religion))
    {
        description <<
            uppercase_first(localise("%s supports the use of this spell.",
                                     god_name(you.religion)));
        description << "\n";
    }

    if (!you_can_memorise(spell))
    {
        string reason = desc_cannot_memorise_reason(spell);
        description << "\n";
        if (you.has_spell(spell))
            description << localise("You cannot cast this spell because %s", reason);
        else
            description << localise("You cannot memorise this spell because %s", reason);
        description << "\n";
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
        string reason = spell_uselessness_reason(spell, true, false);
        description << "\n";
        description << localise("This spell would have no effect right now because %s",
                                reason);
        description << "\n";
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
        description += localise(
                           "This spell has no description. "
                           "Casting it may therefore be unwise. "
#ifdef DEBUG
                           "Instead, go fix it. "
#else
                           "Please file a bug report."
#endif
                       );
    }

    if (mon_owner)
    {
        const int hd = mon_owner->spell_hd();
        const int range = mons_spell_range_for_hd(spell, hd);
        description += "\n" + localise("Range : ");
        if (spell == SPELL_CALL_DOWN_LIGHTNING)
            description += stringize_glyph(mons_char(mon_owner->type)) + "..---->";
        else
            description += range_string(range, range, mons_char(mon_owner->type));
        description += "\n";

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
                wiz_info += localise(" (pow %d)", _hex_pow(spell, hd));
#endif
            description += you.immune_to_hex(spell)
                ? localise("You cannot be affected by this "
                           "spell right now. %s",
                           wiz_info.c_str())
                : localise("Chance to defeat your Will: %d%%%s",
                           hex_chance(spell, hd),
                           wiz_info.c_str());
            description += "\n";
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
void get_spell_desc(const spell_type spell, describe_info &inf)
{
    string desc;
    _get_spell_description(spell, nullptr, desc);
    inf.body << desc;
}

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
    inf.title = localise(ability_name(ability));
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
        inf.title = "a stacked deck";
    else
        inf.title = "the " + deck_name(deck);
    inf.title = uppercase_first(localise(inf.title));

    inf.body << deck_description(deck);

    show_description(inf);
}

static string _describe_draconian(const monster_info& mi)
{
    string description, colour;
    const int subsp = mi.draco_or_demonspawn_subspecies();

    if (subsp != mi.type)
    {
        switch (subsp)
        {
        case MONS_BLACK_DRACONIAN:      colour = "black ";   break;
        case MONS_YELLOW_DRACONIAN:     colour = "yellow ";  break;
        case MONS_GREEN_DRACONIAN:      colour = "green ";   break;
        case MONS_PURPLE_DRACONIAN:     colour = "purple ";  break;
        case MONS_RED_DRACONIAN:        colour = "red ";     break;
        case MONS_WHITE_DRACONIAN:      colour = "white ";   break;
        case MONS_GREY_DRACONIAN:       colour = "grey ";    break;
        case MONS_PALE_DRACONIAN:       colour = "pale ";    break;
        default:
            break;
        }

        description = localise("It has %sscales. ");
    }

    switch (subsp)
    {
    case MONS_BLACK_DRACONIAN:
        description += localise("Sparks flare out of its mouth and nostrils.");
        break;
    case MONS_YELLOW_DRACONIAN:
        description += localise("Acidic fumes swirl around it.");
        break;
    case MONS_GREEN_DRACONIAN:
        description += localise("Venom drips from its jaws.");
        break;
    case MONS_PURPLE_DRACONIAN:
        description += localise("Its outline shimmers with magical energy.");
        break;
    case MONS_RED_DRACONIAN:
        description += localise("Smoke pours from its nostrils.");
        break;
    case MONS_WHITE_DRACONIAN:
        description += localise("Frost pours from its nostrils.");
        break;
    case MONS_GREY_DRACONIAN:
        description += localise("Its scales and tail are adapted to the water.");
        break;
    case MONS_PALE_DRACONIAN:
        description += localise("It is cloaked in a pall of superheated steam.");
        break;
    default:
        break;
    }

    return description;
}

static string _describe_demonspawn_role(monster_type type)
{
    switch (type)
    {
    case MONS_BLOOD_SAINT:
        return "It weaves powerful and unpredictable spells of devastation.";
    case MONS_WARMONGER:
        return "It is devoted to combat, disrupting the magic of its foes as "
               "it battles endlessly.";
    case MONS_CORRUPTER:
        return "It corrupts space around itself, and can twist even the very "
               "flesh of its opponents.";
    case MONS_BLACK_SUN:
        return "It shines with an unholy radiance, and wields powers of "
               "darkness from its devotion to the deities of death.";
    default:
        return "";
    }
}

static string _describe_demonspawn_base(int species)
{
    switch (species)
    {
    case MONS_MONSTROUS_DEMONSPAWN:
        return "It is more beast now than whatever species it is descended from.";
    case MONS_GELID_DEMONSPAWN:
        return "It is covered in icy armour.";
    case MONS_INFERNAL_DEMONSPAWN:
        return "It gives off an intense heat.";
    case MONS_TORTUROUS_DEMONSPAWN:
        return "It oozes dark energies.";
    }
    return "";
}

static string _describe_demonspawn(const monster_info& mi)
{
    string description;
    const int subsp = mi.draco_or_demonspawn_subspecies();

    description += localise(_describe_demonspawn_base(subsp));

    if (subsp != mi.type)
    {
        const string demonspawn_role = _describe_demonspawn_role(mi.type);
        if (!demonspawn_role.empty())
        {
            description = localise(description);
            description += localise(" ") + localise(demonspawn_role);
        }
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
    default:
        return "buggy resistance";
    }
}

static const char* _get_threat_desc(mon_threat_level_type threat)
{
    switch (threat)
    {
    case MTHRT_TRIVIAL: return "This monster looks harmless.";
    case MTHRT_EASY:    return "This monster looks easy.";
    case MTHRT_TOUGH:   return "This monster looks dangerous.";
    case MTHRT_NASTY:   return "This monster looks extremely dangerous.";
    case MTHRT_UNDEF:
    default:            return "This montsre looks buggily threatening."; // @noloc
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
    switch (flavour)
    {
        case AF_KITE:
            return "retreat from adjacent foes and ";
        case AF_SWOOP:
            return "swoop behind its foe and ";
        default:
            return "";
    }
}

static string _flavour_base_desc(attack_flavour flavour)
{
    static const map<attack_flavour, string> base_descs = {
        { AF_ACID,              " and deal extra acid damage if any damage is dealt"},
        { AF_BLINK,             " and blink self if any damage is dealt" },
        { AF_BLINK_WITH,        " and blink together with the defender if any damage is dealt" },
        { AF_COLD,              " and deal up to %d extra cold damage if any damage is dealt" },
        { AF_CONFUSE,           " and cause confusion if any damage is dealt" },
        { AF_DRAIN_STR,         " and drain strength if any damage is dealt" },
        { AF_DRAIN_INT,         " and drain intelligence if any damage is dealt" },
        { AF_DRAIN_DEX,         " and drain dexterity if any damage is dealt" },
        { AF_DRAIN_STAT,        " and drain strength, intelligence or dexterity if any damage is dealt" },
        { AF_DRAIN,             " and drain life if any damage is dealt" },
        { AF_ELEC,              " and deal up to %d extra electric damage if any damage is dealt" },
        { AF_FIRE,              " and deal up to %d extra fire damage if any damage is dealt" },
        { AF_MUTATE,            " and cause mutations if any damage is dealt" },
        { AF_POISON_PARALYSE,   " and poison and cause paralysis or slowing if any damage is dealt" },
        { AF_POISON,            " and cause poisoning if any damage is dealt" },
        { AF_POISON_STRONG,     " and cause strong poisoning if any damage is dealt" },
        { AF_VAMPIRIC,          " and drain health from the living if any damage is dealt" },
        { AF_DISTORT,           " and cause wild translocation effects if any damge is dealt" },
        { AF_RAGE,              " and cause berserking if any damage is dealt" },
        { AF_STICKY_FLAME,      " and apply sticky flame if any damage is dealt" },
        { AF_CHAOTIC,           " and cause unpredictable effects if any damage is dealt" },
        { AF_STEAL,             " and steal items if any damage is dealt" },
        { AF_CRUSH,             " and begin ongoing constriction" },
        { AF_REACH,             "" },
        { AF_HOLY,              " and deal extra damage to undead and demons if any damage is dealt" },
        { AF_ANTIMAGIC,         " and drain magic if any dmage is dealt" },
        { AF_PAIN,              " and cause pain to the living if any damage is dealt" },
        { AF_ENSNARE,           " and ensnare with webbing if any damage is dealt" },
        { AF_ENGULF,            " and engulf" },
        { AF_PURE_FIRE,         "" },
        { AF_DRAIN_SPEED,       " and drain speed if any damage is dealt" },
        { AF_VULN,              " and reduce willpower if any damage is dealt" },
        { AF_SHADOWSTAB,        " and deal increased damage when unseen" },
        { AF_DROWN,             " and deal drowning damage" },
        { AF_CORRODE,           " and cause corrosion" },
        { AF_SCARAB,            " and drain speed and drain health if any damage is dealt" },
        { AF_TRAMPLE,           " and knock back the defender if any damage is dealt" },
        { AF_REACH_STING,       " and cause poisoning if any damage is dealt" },
        { AF_REACH_TONGUE,      " and deal extra acid damage if any damage is dealt" },
        { AF_WEAKNESS,          " and cause weakness if any damage is dealt" },
        { AF_KITE,              "" },
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
    return localise(base_desc, flavour_dam);
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

        ++attack_counts[attack_info];
    }

    // Hydrae have only one explicit attack, which is repeated for each head.
    if (mons_genus(mi.base_type) == MONS_HYDRA)
        for (auto &attack_count : attack_counts)
            attack_count.second = mi.num_heads;

    vector<string> attack_descs;
    for (const auto &attack_count : attack_counts)
    {
        const mon_attack_info &info = attack_count.first;
        const int times = attack_count.second;
        const mon_attack_def &attack = info.definition;

        const string weapon_name =
              info.weapon ? info.weapon->name(DESC_A)
            : article_a(ghost_brand_name(special_flavour, mi.type));

        // XXX: hack alert
        if (attack.flavour == AF_PURE_FIRE)
        {
            attack_descs.push_back(
                localise("%s for up to %d fire damage",
                             mon_attack_name(attack.type, false).c_str(),
                             flavour_damage(attack.flavour, mi.hd, false)));
            continue;
        }

        string verb = mon_attack_name(attack.type, false);
        string damage_desc;
        if (!flavour_has_reach(attack.flavour))
        {
            if (attack_count.second > 1 && weapon_name.empty())
            {
                damage_desc = localise("%s %d times for up to %d damage each",
                                       verb, times, attack.damage);
            }
            else if (weapon_name.empty())
            {
                damage_desc = localise("%s for up to %d damage",
                                       verb, attack.damage);
            }
            else if (attack_count.second > 1)
            {
                damage_desc = localise("%s %d times for up to %d damage each plus %s",
                                       verb, times, attack.damage, weapon_name);
            }
            else
            {
                damage_desc = localise("%s for up to %d damage plus %s",
                                       verb, attack.damage, weapon_name);
            }
        }
        else
        {
            if (attack_count.second > 1 && weapon_name.empty())
            {
                damage_desc = localise("%s from a distance %d times for up to %d damage each",
                                       verb, times, attack.damage);
            }
            else if (weapon_name.empty())
            {
                damage_desc = localise("%s from a distance for up to %d damage",
                                       verb, attack.damage);
            }
            else if (attack_count.second > 1)
            {
                damage_desc = localise("%s from a distance %d times for up to %d damage each plus %s",
                                       verb, times, attack.damage, weapon_name);
            }
            else
            {
                damage_desc = localise("%s from a distance for up to %d damage plus %s",
                                       verb, attack.damage, weapon_name);
            }
        }

        damage_desc = localise(_special_flavour_prefix(attack.flavour)) + damage_desc;
        damage_desc += _flavour_effect(attack.flavour, mi.hd);
        attack_descs.push_back(damage_desc);
    }

    if (!attack_descs.empty())
    {
        string list = comma_separated_line(attack_descs.begin(),
                                           attack_descs.end(),
                                           localise("; and "), localise("; "));
        result << localise("It can %s.", LocalisationArg(list, false));
        _describe_mons_to_hit(mi, result);
        result << localise(".") << "\n";
    }

    if (mi.type == MONS_ROYAL_JELLY)
    {
        result << localise("It will release varied jellies when damaged or killed, with"
                           " the number of jellies proportional to the amount of damage.")
               << "\n";
        result << localise("It will release all of its jellies when polymorphed.")
               << "\n";
    }

    return result.str();
}

static string _monster_missiles_description(const monster_info& mi)
{
    item_def *missile = mi.inv[MSLOT_MISSILE].get();
    if (!missile)
        return "";

    string miss;
    if (missile->is_type(OBJ_MISSILES, MI_THROWING_NET))
        miss = missile->name(DESC_A, false, false, true, false);
    else
        miss = pluralise(missile->name(DESC_PLAIN, false, false, true, false));

    return localise("It is quivering %s.", miss) + "\n";
}

static string _monster_spells_description(const monster_info& mi)
{
    // Show monster spells and spell-like abilities.
    if (!mi.has_spells())
        return "";

    formatted_string description;
    describe_spellset(monster_spellset(mi), nullptr, description, &mi);
    description.cprintf("\n");
    description.cprintf(localise("To read a description, press the key listed above. "
        "(AdB) indicates damage (the sum of A B-sided dice), "
        "(x%%) indicates the chance to defeat your Will, "
        "and (y) indicates the spell range"));
    if (crawl_state.need_save)
        description.cprintf(localise("; shown in red if you are in range"));
    description.cprintf(localise("."));
    description.cprintf("\n");

    return description.to_colour_string();
}

static const char *_speed_description(int speed)
{
    // These thresholds correspond to the player mutations for fast and slow.
    ASSERT(speed != 10);
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

    return "buggily";
}

static void _add_energy_to_string(int speed, int energy, string what,
                                  vector<string> &fast, vector<string> &slow)
{
    if (energy == 10)
        return;

    const int act_speed = (speed * 10) / energy;
    string msg = localise(what, _speed_description(act_speed));
    if (act_speed > 10)
        fast.push_back(msg);
    if (act_speed < 10)
        slow.push_back(msg);
}

/**
 * Display the % chance of a player hitting the given monster.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
void describe_to_hit(const monster_info& mi, ostringstream &result,
                     bool parenthesize)
{
    // TODO: don't do this if the player doesn't exist (main menu)

    const item_def* weapon = you.weapon();
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
        const int missile = quiver::find_action_from_launcher(you.weapon())->get_item();
        if (missile < 0)
            return; // failure to launch
        ranged_attack attk(&you, nullptr, &you.inv[missile], is_pproj_active());
        acc_pct = to_hit_pct(mi, attk, false);
    }

    if (parenthesize)
        result << localise(" (");

    string weap_name;
    if (weapon == nullptr)
        weap_name = "your " + you.hand_name(true);
    else
        weap_name = weapon->name(DESC_YOUR, false, false, false);

    result << localise("about %d%% to evade %s", (100 - acc_pct), weap_name);

    if (parenthesize)
        result << localise(")");
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
    const int base_to_hit = mon_to_hit_base(mi.hd, skilled, !melee);
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
    result << localise(" (about %d%% to hit you)", hit_chance);
}

/**
 * Print a bar of +s and .s representing a given stat to a provided stream.
 *
 * @param value[in]         The current value represented by the bar.
 * @param scale[in]         The value that each + and . represents.
 * @param name              The name of the bar.
 * @param result[in,out]    The stringstream to append to.
 * @param base_value[in]    The 'base' value represented by the bar. If
 *                          INT_MAX, is ignored.
 */
static void _print_bar(int value, int scale, string name,
                       ostringstream &result, int base_value)
{
    if (base_value == INT_MAX)
        base_value = value;

    if (name.size())
        result << chop_string(localise(name), 7, true, true) << " ";

    const int display_max = value ? value : base_value;
    const bool currently_disabled = !value && base_value;

    string bar;
    for (int i = 0; i * scale < display_max; i++)
    {
        bar += "+";
        if (i % 5 == 4)
            bar += " ";
    }

    if (currently_disabled)
      result << localise("none (normally %s)", LocalisationArg(bar, false));
    else if (display_max == 0)
        result <<  localise("none");
    else
        result << bar;

#ifdef DEBUG_DIAGNOSTICS
    if (!you.suppress_wizard)
        result << " (" << value << ")";
#endif

#ifdef DEBUG_DIAGNOSTICS
    if (currently_disabled)
        if (!you.suppress_wizard)
            result << " (base: " << base_value << ")";
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
    result << localise("Max HP: ") <<  mi.get_max_hp_desc(true) << "\n";
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
    _print_bar(mi.ac, 5, "AC:", result);
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
    _print_bar(mi.ev, 5, "EV:", result, mi.base_ev);
    describe_to_hit(mi, result, true);
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
        result << chop_string(localise("Will:"), 7, true, true) << " ∞\n";
        return;
    }

    const int bar_scale = WL_PIP;
    _print_bar(mi.willpower(), bar_scale, "Will:", result);
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
    const monster_type type = mons_is_job(mi.type)
                              ? mi.draco_or_demonspawn_subspecies()
                              : mi.type;

    switch (mons_habitat_type(type, mi.base_type))
    {
    case HT_AMPHIBIOUS:
        return localise("It can travel through water.") + "\n";
    case HT_AMPHIBIOUS_LAVA:
        return localise("It can travel through lava.") + "\n";
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
    "very large",
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
    ostringstream result;
    if (mi.is(MB_ALLY_TARGET))
    {
        auto allies = find_allies_targeting(*m);
        if (allies.size() == 1)
            result << "It is currently targeted by " << allies[0]->name(DESC_YOUR) << ".\n";
        else
        {
            result << "It is currently targeted by allies:\n";
            for (auto *a : allies)
                result << "  " << a->name(DESC_YOUR) << "\n";
        }
    }

    // TODO: this might be ambiguous, give a relative position?
    if (mi.attitude == ATT_FRIENDLY && m->get_foe())
        result << "It is currently targeting " << m->get_foe()->name(DESC_THE) << ".\n";

    return result.str();
}

// Describe a monster's (intrinsic) resistances, speed and a few other
// attributes.
static string _monster_stat_description(const monster_info& mi)
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
        MR_RES_VORTEX,
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
            if (rflags == MR_RES_DAMNATION || rflags == MR_RES_VORTEX)
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

    if (mi.props.exists(CLOUD_IMMUNE_MB_KEY) && mi.props[CLOUD_IMMUNE_MB_KEY])
        extreme_resists.emplace_back("clouds of all kinds");

    vector<string> resist_descriptions;
    if (!extreme_resists.empty())
    {
        const string tmp = localise("It is immune to %s.",
                                    comma_separated_line(extreme_resists.begin(),
                                                         extreme_resists.end()));
        resist_descriptions.push_back(tmp);
    }
    if (!high_resists.empty())
    {
        const string tmp = localise("It is very resistant to %s.",
                                    comma_separated_line(high_resists.begin(),
                                                         high_resists.end()));
        resist_descriptions.push_back(tmp);
    }
    if (!base_resists.empty())
    {
        const string tmp = localise("It is resistant to %s.",
                                    comma_separated_line(base_resists.begin(),
                                                         base_resists.end()));
        resist_descriptions.push_back(tmp);
    }

    if (mi.threat != MTHRT_UNDEF)
        result << localise(_get_threat_desc(mi.threat)) << "\n";

    for (string rdesc: resist_descriptions)
        result << rdesc << "\n";

    // Is monster susceptible to anything? (On a new line.)
    if (!suscept.empty())
    {
        result << localise("It is susceptible to %s.",
                           comma_separated_line(suscept.begin(),
                                                suscept.end()))
               << "\n";
    }

    if (mi.is(MB_CHAOTIC))
    {
        result << localise("It is vulnerable to silver and hated by Zin.")
               << "\n";
    }

    if (mons_class_flag(mi.type, M_STATIONARY)
        && !mons_is_tentacle_or_tentacle_segment(mi.type))
    {
        result << localise("It cannot move.") << "\n";
    }

    if (mons_class_flag(mi.type, M_COLD_BLOOD)
        && get_resist(resist, MR_RES_COLD) <= 0)
    {
        result << localise("It is cold-blooded and may be slowed by cold attacks.")
               << "\n";
    }

    // Seeing invisible.
    if (mi.can_see_invisible())
        result << localise("It can see invisible.") << "\n";

    // Echolocation, wolf noses, jellies, etc
    if (!mons_can_be_blinded(mi.type))
        result << localise("It is immune to blinding.") << "\n";
    // XXX: could mention "immune to dazzling" here, but that's spammy, since
    // it's true of such a huge number of monsters. (undead, statues, plants).
    // Might be better to have some place where players can see holiness &
    // information about holiness.......?

    if (mi.intel() <= I_BRAINLESS)
    {
        // Matters for Ely.
        result << localise("It is mindless.") << "\n";
    }
    else if (mi.intel() >= I_HUMAN)
    {
        // Matters for Yred, Gozag, Zin, TSO, Alistair....
        result << localise("It is intelligent.") << "\n";
    }

    // Unusual monster speed.
    const int speed = mi.base_speed();
    if (speed != 10 && speed != 0)
        result << localise("It is %s.", mi.speed_description()) << "\n";
    const mon_energy_usage def = DEFAULT_ENERGY;
    if (!(mi.menergy == def))
    {
        const mon_energy_usage me = mi.menergy;
        vector<string> fast, slow;
        _add_energy_to_string(speed, me.move,
                              "It covers ground %s.",
                              fast, slow);
        // since MOVE_ENERGY also sets me.swim
        if (me.swim != me.move)
        {
            _add_energy_to_string(speed, me.swim,
                                  "It swims %s.", fast, slow);
        }
        _add_energy_to_string(speed, me.attack,
                              "It attacks %s.", fast, slow);
        if (mons_class_itemuse(mi.type) >= MONUSE_STARTING_EQUIPMENT)
        {
            _add_energy_to_string(speed, me.missile,
                                  "It shoots %s.", fast, slow);
        }

        if (mi.is_actual_spellcaster())
        {
            _add_energy_to_string(speed, me.spell, "It casts spells %s.",
                                  fast, slow);
        }
        else if (mi.is_priest())
        {
            _add_energy_to_string(speed, me.spell, "It uses invocations %s.",
                                  fast, slow);
        }
        else
        {
            _add_energy_to_string(speed, me.spell,
                                  "It uses natural abilities %s.", fast, slow);
        }

        _add_energy_to_string(speed, me.special,
                              "It uses special abilities %s.",
                              fast, slow);
        if (mons_class_itemuse(mi.type) >= MONUSE_STARTING_EQUIPMENT)
        {
            _add_energy_to_string(speed, me.item,
                                  "It uses items %s.",
                                  fast, slow);
        }

        for (string msg: fast)
            result << msg << "\n";

        for (string msg: slow)
            result << msg << "\n";
    }

    if (mi.type == MONS_SHADOW)
    {
        // Cf. monster::action_energy() in monster.cc.
        result << localise("It covers  ground more quickly when invisible.")
               << "\n";
    }

    if (mi.airborne())
        result << localise("It can fly.") << "\n";

    // Unusual regeneration rates.
    if (!mi.can_regenerate())
        result << localise("It cannot regenerate.") << "\n";
    else if (mons_class_fast_regen(mi.type))
        result << localise("It regenerates quickly.") << "\n";

    const char* mon_size = get_size_adj(mi.body_size(), true);
    if (mon_size)
        result << localise("It is %s.", mon_size) << "\n";

    if (in_good_standing(GOD_ZIN, 0) && !mi.pos.origin() && monster_at(mi.pos))
    {
        recite_counts retval;
        monster *m = monster_at(mi.pos);
        auto eligibility = zin_check_recite_to_single_monster(m, retval);
        if (eligibility == RE_INELIGIBLE)
            result << localise("It cannot be affected by reciting Zin's laws.");
        else if (eligibility == RE_TOO_STRONG)
            result << localise("It is too strong to be affected by reciting Zin's laws.");
        else // RE_ELIGIBLE || RE_RECITE_TIMER
            result << localise("It can can be affected by reciting Zin's laws.");

        if (you.wizard)
        {
            result << localise(" (Recite power: %d, Hit Dice: %d)",
                               zin_recite_power(), mi.hd);
        }
        result << "\n";
    }

    result << _monster_attacks_description(mi);
    result << _monster_missiles_description(mi);
    result << _monster_habitat_description(mi);
    result << _monster_spells_description(mi);

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
                         bool &has_stat_desc)
{
    if (inf.title.empty())
        inf.title = getMiscString(mi.common_name(DESC_DBNAME) + " title");
    if (inf.title.empty())
        inf.title = uppercase_first(localise(mi.full_name(DESC_A))) + ".";

    string db_name;

    if (mi.props.exists("dbname"))
        db_name = mi.props["dbname"].get_string();
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
        symbol = "cap-" + symbol; // @noloc

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

    case MONS_MONSTROUS_DEMONSPAWN:
    case MONS_GELID_DEMONSPAWN:
    case MONS_INFERNAL_DEMONSPAWN:
    case MONS_TORTUROUS_DEMONSPAWN:
    case MONS_BLOOD_SAINT:
    case MONS_WARMONGER:
    case MONS_CORRUPTER:
    case MONS_BLACK_SUN:
    {
        inf.body << "\n" << _describe_demonspawn(mi) << "\n";
        break;
    }

    case MONS_PLAYER_GHOST:
        inf.body << localise("The apparition of %s.",
                             LocalisationArg(get_ghost_description(mi), false))
                 << "\n";
        if (mi.props.exists(MIRRORED_GHOST_KEY))
            inf.body << localise("It looks just like you...spooky!") << "\n";
        break;

    case MONS_PLAYER_ILLUSION:
        inf.body << localise("An illusion of %s.",
                             LocalisationArg(get_ghost_description(mi), false))
                 << "\n";
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
            inf.body << "\n" << localise("It is quickly melting away.") << "\n";
        break;

    case MONS_BRIAR_PATCH: // death msg uses "crumbling"
    case MONS_PILLAR_OF_SALT:
        // XX why are these "quick" here but "slow" elsewhere??
        if (mi.is(MB_SLOWLY_DYING))
            inf.body << "\n" << localise("It is quickly crumbling away.") << "\n";
        break;

    case MONS_PROGRAM_BUG:
        inf.body << localise("If this monster is a \"program bug\", then it's "
                "recommended that you save your game and reload. Please report "
                "monsters who masquerade as program bugs or run around the "
                "dungeon without a proper description to the authorities.");
        inf.body << "\n";
        break;

    default:
        break;
    }

    if (mons_class_is_fragile(mi.type))
    {
        if (mi.is(MB_CRUMBLING))
            inf.body << "\n" << localise("It is quickly crumbling away.") << "\n";
        else if (mi.is(MB_WITHERING))
            inf.body << "\n" << localise("It is quickly withering away.") << "\n";
        else
            inf.body << "\n" << localise("If struck, it will die soon after.") << "\n";
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
        inf.body << "\n";
        if (curse_power <= 10)
        {
            inf.body << localise("It will inflict a necromantic curse on its "
                                 "foe when destroyed.");
        }
        else
        {
            inf.body << localise("It will inflict a powerful necromantic "
                                 "curse on its foe when destroyed.");
        }
        inf.body << "\n";
    }

    // Get information on resistances, speed, etc.
    string result = _monster_stat_description(mi);
    if (!result.empty())
    {
        inf.body << "\n" << result;
        has_stat_desc = true;
    }

    bool did_stair_use = false;
    if (!mons_class_can_use_stairs(mi.type))
    {
        inf.body << localise("It is incapable of using stairs.") << "\n";
        did_stair_use = true;
    }

    result = _monster_current_target_description(mi);
    if (!result.empty())
        inf.body << "\n" << result;

    if (mi.is(MB_SUMMONED) || mi.is(MB_PERM_SUMMON))
    {
        inf.body << "\n";
        if (mi.is(MB_SUMMONED))
            inf.body << localise("This monster has been summoned, and is thus only temporary.");
        else
            inf.body << localise("This monster has been summoned in a durable way.");
        inf.body << localise(" ");
        // TODO: hacks; convert angered_by_attacks to a monster_info check
        // (but on the other hand, it is really limiting to not have access
        // to the monster...)
        if (!mi.pos.origin() && monster_at(mi.pos)
                                && monster_at(mi.pos)->angered_by_attacks()
                                && mi.attitude == ATT_FRIENDLY)
        {
            inf.body << localise("If angered it will immediately vanish, "
                                 "yielding no experience or items");
        }
        else
            inf.body << localise("Killing it yields no experience or items");

        if (!did_stair_use && !mi.is(MB_PERM_SUMMON))
            inf.body << localise("; it is incapable of using stairs");

        if (mi.is(MB_PERM_SUMMON))
            inf.body << localise(" and it cannot be abjured");

        inf.body << localise(".") << "\n";
    }
    else if (mi.is(MB_NO_REWARD))
        inf.body << "\n" << localise("Killing this monster yields no experience or items.");
    else if (mons_class_leaves_hide(mi.type))
    {
        inf.body << "\n";
        inf.body << localise("If it is slain, it may be possible to recover "
                             "its hide, which can be used as armour.");
        inf.body << "\n";
    }

    if (mi.is(MB_SUMMONED_CAPPED))
    {
        inf.body << "\n";
        inf.body << localise("You have summoned too many monsters of this kind "
                             "to sustain them all, and thus this one will "
                             "shortly expire.");
        inf.body << "\n";
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
    if (mons.strict_neutral())
        attitude.emplace_back("strict_neutral");
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

    if (mons.props.exists("blame"))
    {
        inf.body << "\n\nMonster blame chain:\n";

        const CrawlVector& blame = mons.props["blame"].get_vector();

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

    get_monster_db_desc(mi, inf, has_stat_desc);

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
    desc = formatted_string::parse_string(trimmed_string(desc));

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

    const string mores[2] = {
        localise(MORE_PREFIX) + localise("<w>Description</w>|Quote"),
        localise(MORE_PREFIX) + localise("Description|<w>Quote</w>"),
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
    formatted_string needle;
    describe_spellset(spells, nullptr, needle, &mi);
    string desc_without_spells = desc.to_colour_string();
    if (!needle.empty())
    {
        desc_without_spells = replace_all(desc_without_spells,
                needle.to_colour_string(), "SPELLSET_PLACEHOLDER");
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
    "weakling ",
    "amateur ",
    "novice ",
    "journeyman ",
    "adept ",
    "veteran ",
    "master ",
    "legendary "
};

string short_ghost_description(const monster *mon, bool abbrev)
{
    ASSERT(mons_is_pghost(mon->type));

    const ghost_demon &ghost = *(mon->ghost);
    const char* rank = xl_rank_names[ghost_level_to_rank(ghost.xl)];

    string fmt = string("%s%s ") + get_job_name(ghost.job); // @noloc
    string desc = localise(fmt, rank, species::name(ghost.species));

    if (abbrev || strwidth(desc) > 40)
    {
        fmt = string("%s%s") + get_job_abbrev(ghost.job); // @noloc
        desc = localise(fmt, rank, species::get_abbrev(ghost.species));
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

    gstr << mi.mname << localise(" ")
         << skill_title_by_rank(mi.i_ghost.best_skill,
                        mi.i_ghost.best_skill_rank,
                        true,
                        gspecies,
                        species::has_low_str(gspecies), mi.i_ghost.religion)
         << localise(", ");

    string rank = xl_rank_names[ghost_level_to_rank(mi.i_ghost.xl_rank)];

    string fmt;
    if (concise)
    {
        fmt = string("a %s%s") + get_job_abbrev(mi.i_ghost.job); // @noloc
        gstr << localise(fmt, rank, species::get_abbrev(gspecies));
    }
    else
    {
        fmt = string("a %s%s ") + get_job_name(mi.i_ghost.job); // @noloc
        gstr << localise(fmt, rank, species::name(gspecies));
    }

    if (mi.i_ghost.religion != GOD_NO_GOD)
        gstr << localise(" of %s", god_name(mi.i_ghost.religion));

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

// only used in tiles
string get_command_description(const command_type cmd, bool terse)
{
    string lookup = command_to_name(cmd);

    if (!terse)
        lookup += " verbose"; // @noloc

    string result = getLongDescription(lookup);
    if (result.empty())
    {
        if (!terse)
        {
            // Try for the terse description.
            result = get_command_description(cmd, true);
            if (!result.empty())
                return add_punctuation(result, ".", false);
        }
        return command_to_name(cmd);
    }

    // i18n: TODO: Fix this
    return result.substr(0, result.length() - 1);
}

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
    ostringstream desc;
    desc << "\n";
    if (is_opaque_cloud(cloud_type))
    {
        desc << localise("This cloud is opaque; one tile will not block vision"
                         ", but multiple will.");
        desc << "\n\n";
        desc << localise("Clouds of this kind an adventurer makes will vanish "
                         "quickly once outside their sight.");
    }
    else
    {
        desc << localise("Clouds of this kind an adventurer makes will vanish "
                         "almost instantly once outside their sight.");
    }
    return desc.str();
}
