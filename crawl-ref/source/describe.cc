/**
 * @file
 * @brief Functions used to print information about various game objects.
**/

#include "AppHdr.h"

#include "describe.h"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <string>

#include "adjust.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "branch.h"
#include "butcher.h"
#include "cloud.h" // cloud_type_name
#include "clua.h"
#include "database.h"
#include "decks.h"
#include "delay.h"
#include "describe-spells.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "fight.h"
#include "food.h"
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
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-book.h"
#include "mon-cast.h" // mons_spell_range
#include "mon-death.h"
#include "mon-tentacle.h"
#include "options.h"
#include "output.h"
#include "process-desc.h"
#include "prompt.h"
#include "religion.h"
#include "skills.h"
#include "species.h"
#include "spl-book.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "spl-wpnench.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h" // to_string on Cygwin
#include "terrain.h"
#ifdef USE_TILE_LOCAL
 #include "tilereg-crt.h"
#endif
#include "unicode.h"

int count_desc_lines(const string &_desc, const int width)
{
    string desc = get_linebreak_string(_desc, width);
    return count(begin(desc), end(desc), '\n');
}

void print_description(const string &body)
{
    describe_info inf;
    inf.body << body;
    print_description(inf);
}

class default_desc_proc
{
public:
    int width() { return get_number_of_cols() - 1; }
    int height() { return get_number_of_lines(); }
    void print(const string &str) { cprintf("%s", str.c_str()); }

    void nextline()
    {
        if (wherey() < height())
            cgotoxy(1, wherey() + 1);
        else
            cgotoxy(1, height());
        // Otherwise cgotoxy asserts; let's just clobber the last line
        // instead, which should be noticeable enough.
    }
};

void print_description(const describe_info &inf)
{
    clrscr();
    textcolour(LIGHTGREY);

    default_desc_proc proc;
    process_description<default_desc_proc>(proc, inf);
}

static void _print_quote(const describe_info &inf)
{
    clrscr();
    textcolour(LIGHTGREY);

    default_desc_proc proc;
    process_quote<default_desc_proc>(proc, inf);
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
    case RING_TELEPORTATION:      return "*Tele";
    case RING_RESIST_CORROSION:   return "rCorr";
#if TAG_MAJOR_VERSION == 34
    case RING_TELEPORT_CONTROL:   return "+cTele";
#endif
    case AMU_HARM:                return "Harm";
    case AMU_MANA_REGENERATION:   return "RegenMP";
    case AMU_THE_GOURMAND:        return "Gourm";
#if TAG_MAJOR_VERSION == 34
    case AMU_DISMISSAL:           return "Dismiss";
    case AMU_CONSERVATION:        return "Cons";
    case AMU_CONTROLLED_FLIGHT:   return "cFly";
#endif
    case AMU_GUARDIAN_SPIRIT:     return "Spirit";
    case AMU_FAITH:               return "Faith";
    case AMU_REFLECTION:          return "Reflect";
    case AMU_INACCURACY:          return "Inacc";
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
        { ARTP_MAGIC_RESISTANCE,      prop_note::symbolic },
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
        { ARTP_CURSE,                 prop_note::plain },
        { ARTP_CLARITY,               prop_note::plain },
        { ARTP_RMSL,                  prop_note::plain },
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
        return "이것은 힘, 지능, 민첩을 유지시켜준다.";
#endif
    case RING_WIZARDRY:
        return "이것은 주문 성공률을 높여준다.";
    case RING_FIRE:
        return "이것은 화염 마법을 향상시킨다.";
    case RING_ICE:
        return "이것은 냉기 마법을 향상시킨다.";
    case RING_TELEPORTATION:
        return "이것은 당신을 몬스터 옆으로 텔레포트시킬 지 모른다.";
#if TAG_MAJOR_VERSION == 34
    case RING_TELEPORT_CONTROL:
        return "이것은 조정 가능한 텔레포트를 발동시킬 수 있다.";
#endif
    case AMU_HARM:
        return "이것은 주고 받는 데미지를 증가시킨다.";
    case AMU_MANA_REGENERATION:
        return "이것은 마력 재생량을 증진시킨다.";
    case AMU_THE_GOURMAND:
        return "이것은 당신이 배고프지 않아도 생고기를 먹을 수 있도록 도와준다.";
#if TAG_MAJOR_VERSION == 34
    case AMU_DISMISSAL:
        return "이것은 당신을 해칠 수 있는 크리쳐들을 텔레포트시킬 지 모른다.";
    case AMU_CONSERVATION:
        return "이것은 파괴로부터 인벤토리를 보호한다.";
#endif
    case AMU_GUARDIAN_SPIRIT:
        return "이것은 받는 피해량을 체력과 마력에 나눠 받도록 "
               "한다.";
    case AMU_FAITH:
        return "이것은 신의 호의를 빠르게 받을 수 있도록 도와준다.";
    case AMU_REFLECTION:
        return "이것은 방패처럼 당신을 보호해주고, 공격을 반사한다.";
    case AMU_INACCURACY:
        return "이것은 당신이 하는 모든 공격의 정확도를 감소시킨다.";
    }
    return "";
}

struct property_descriptor
{
    artefact_prop_type property;
    const char* desc;           // If it contains %d, will be replaced by value.
    bool is_graded_resist;
};

static string _randart_descrip(const item_def &item)
{
    string description;

    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_desc_properties(item, proprt, known);

    const property_descriptor propdescs[] =
    {
        { ARTP_AC, "당신의 AC에 (%d)만큼 영향을 준다.", false },
        { ARTP_EVASION, "당신의 회피에 (%d)만큼 영향을 준다", false},
        { ARTP_STRENGTH, "당신의 힘에 (%d)만큼 영향을 준다.", false},
        { ARTP_INTELLIGENCE, "당신의 지능에 (%d)만큼 영향을 준다.", false},
        { ARTP_DEXTERITY, "당신의 민첩성에 (%d)만큼 영향을 준다.", false},
        { ARTP_SLAYING, "당신의 원거리 무기와 근접 무기의 정확도와 피해량에"
                        "(%d)만큼 영향을 준다.", false},
        { ARTP_FIRE, "화염", true},
        { ARTP_COLD, "냉기", true},
        { ARTP_ELECTRICITY, "절연되어 있어 전기로부터 당신을 보호한다.", false},
        { ARTP_POISON, "독", true},
        { ARTP_NEGATIVE_ENERGY, "음에너지", true},
        { ARTP_MAGIC_RESISTANCE, "적대적인 마법부여에 대한 " 
                                 " 당신의 저항에 영향을 준다.", false},
        { ARTP_HP, "당신의 체력에 (%d)만큼 영향을 준다.", false},
        { ARTP_MAGICAL_POWER, "당신의 마력 비축량에 (%d)만큼 영향을 준다.", false},
        { ARTP_SEE_INVISIBLE, "보이지 않는 것들을 당신이 보게 해준다.", false},
        { ARTP_INVISIBLE, "당신을 보이지 않도록 만든다", false},
        { ARTP_FLY, "당신이 날 수 있도록 만든다.", false},
        { ARTP_BLINK, "당신이 블링크할 수 있도록 만든다.", false},
        { ARTP_BERSERK, "당신이 광폭화할 수 있게 한다.", false},
        { ARTP_NOISE, "전투 중에 소음을 만들 수 있다.", false},
        { ARTP_PREVENT_SPELLCASTING, "당신이 주문을 외우는 것을 막는다.", false},
        { ARTP_CAUSE_TELEPORTATION, "몬스터 옆으로 당신을 텔레포트시킬 수도 있다.", false},
        { ARTP_PREVENT_TELEPORTATION, "텔레포트 형태 대부분을 막는다.",
          false},
        { ARTP_ANGRY, "전투 중에 당신을 광폭화시킬 수도 있다.", false},
        { ARTP_CURSE, "장착 시에 스스로를 저주한다.", false},
        { ARTP_CLARITY, "당신을 혼란스럽지 않게 막아준다.", false},
        { ARTP_CONTAM, "벗을 때 마법 오염을 유발한다.", false},
        { ARTP_RMSL, "당신을 투사체로부터 지켜준다.", false},
        { ARTP_REGENERATION, "당신의 회복 속도를 올려준다.", false},
        { ARTP_RCORR, "당신을 산과 부식으로부터 지켜준다.", false},
        { ARTP_RMUT, "당신을 돌연변이로부터 지켜준다.", false},
        { ARTP_CORRODE, "피격될 때 당신을 부식시킬 수도 있다.", false},
        { ARTP_DRAIN, "벗을 때 생명력을 흡수한다.", false},
        { ARTP_SLOW, "피격될 때 당신을 느리게 만들 수도 있다.", false},
        { ARTP_FRAGILE, "벗을 때 파괴될 것이다.", false },
        { ARTP_SHIELDING, "당신의 방패 수치에 (%d)만큼 영향을 준다.", false},
    };

    // Give a short description of the base type, for base types with no
    // corresponding ARTP.
    if (item.base_type == OBJ_JEWELLERY
        && (item_ident(item, ISFLAG_KNOW_TYPE)))
    {
        const char* type = _jewellery_base_ability_description(item.sub_type);
        if (*type)
        {
            description += "\n";
            description += type;
        }
    }

    for (const property_descriptor &desc : propdescs)
    {
        if (known_proprt(desc.property))
        {
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
                    "당신을 다음 속성에 대해 극단적으로 취약하게 만든다 : ",
                    "당신을 다음 속성에 대해 매우 취약하게 만든다 : ",
                    "당신을 다음 속성에 대해 취약하게 한다 : ",
                    "Buggy descriptor!",
                    "당신을 다음 속성으로부터 보호한다 : ",
                    "당신을 다음 속성으로부터 강력히 보호한다 : ",
                    "당신을 다음 속성에 대해 거의 면역이도록 한다 : "
                };
                sdesc = prefixes[idx] + sdesc + '.';
            }

            description += '\n';
            description += sdesc;
        }
    }

    if (known_proprt(ARTP_STEALTH))
    {
        const int stval = proprt[ARTP_STEALTH];
        char buf[80];
        snprintf(buf, sizeof buf, "\n당신을 %s%s 은밀하게 만든다.",
                 (stval < -1 || stval > 1) ? "더 " : "",
                 (stval < 0) ? "적게" : "많이");
        description += buf;
    }

    return description;
}
#undef known_proprt

static const char *trap_names[] =
{
#if TAG_MAJOR_VERSION == 34
    "dart",
#endif
    "arrow", "spear",
#if TAG_MAJOR_VERSION > 34
    "teleport",
#endif
    "permanent teleport",
    "alarm", "blade",
    "bolt", "net", "Zot", "needle",
    "shaft", "passage", "pressure plate", "web",
#if TAG_MAJOR_VERSION == 34
    "gas", "teleport",
    "shadow", "dormant shadow",
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
    #define HRANDOM_ELEMENT(arr, id) arr[hash_rand(ARRAYSZ(arr), seed, id)]

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

    if (!hash_rand(5, seed, 4) && you.can_smell()) // 20%
        description << HRANDOM_ELEMENT(smell_descs, 5);

    if (hash_rand(2, seed, 6)) // 50%
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

static void _append_weapon_stats(string &description, const item_def &item)
{
    const int base_dam = property(item, PWPN_DAMAGE);
    const int ammo_type = fires_ammo_type(item);
    const int ammo_dam = ammo_type == MI_NONE ? 0 :
                                                ammo_type_damage(ammo_type);
    const skill_type skill = item_attack_skill(item);

    const string your_skill = crawl_state.need_save ?
      make_stringf("\n (당신의 기술: %.1f)", (float) you.skill(skill, 10) / 10)
      : "";
    description += make_stringf(
    "\n기본 명중률: %+d 기본 피해량: %d 기본 공격 지연시간: %.1f"
    "\n이 무기를 최소 공격 지연시간 (%.1f)으로 다루려면 숙련도 %d에 도달해야한다."
    "%s",
     property(item, PWPN_HIT),
     base_dam + ammo_dam,
     (float) property(item, PWPN_SPEED) / 10,
     (float) weapon_min_delay(item, item_brand_known(item)) / 10,
     weapon_min_delay_skill(item),
     your_skill.c_str());

    if (skill == SK_SLINGS)
    {
        description += make_stringf("\n탄환 발사:    기본 피해량: %d",
                                    base_dam +
                                    ammo_type_damage(MI_SLING_BULLET));
    }
}

static string _handedness_string(const item_def &item)
{
    string description;

    switch (you.hands_reqd(item))
    {
    case HANDS_ONE:
        if (you.species == SP_FORMICID)
            description += "한 손의 쌍으로 사용하는 무기다.";
        else
            description += "이것은 한 손으로 사용하는 무기다.";
        break;
    case HANDS_TWO:
        description += "이것은 양손으로 사용하는 무기다.";
        break;
    }

    return description;
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
            description += "\n\n이것은 발동하여 도달 범위를 늘릴 수 있다.";
            break;
        case SK_AXES:
            description += "\n\n착용자와 밀착한 모든 적들을 공격할 수 있으나, "
                           "대상이 아닌 적들에게는 보다 적은 피해량을 준다.";
            break;
        case SK_LONG_BLADES:
            description += "\n\n이것은 반격을 사용할 수 있다, 공격을 회피했을 때 "
                           "신속하게 보복할 수 있다.";
            break;
        case SK_SHORT_BLADES:
            {
                string adj = (item.sub_type == WPN_DAGGER) ? "극단적으로"
                                                           : "부분적으로";
                description += "\n\n부주의한 적들을 암습하기에 " + adj + "좋다.";
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

    // special weapon descrip
    if (item_type_known(item) && (spec_ench != SPWPN_NORMAL || enchanted))
    {
        description += "\n\n";

        switch (spec_ench)
        {
        case SPWPN_FLAMING:
            if (is_range_weapon(item))
            {
            description += "이 무기는 발사한 투사체가"
                "적중한 대상을 불태우도록 한다,";
                }
            else
                {
            description += "이 무기에는 적중한 대상을 불태우기 위해 "
                "특별히 마법이 부여돼있다,";
                }
            description += " 대부분의 적들에게 추가적인 피해를 입히고, "
                "특별히 취약한 몇몇 적들에게는 최대 절반의 피해를 "
                "추가로 입힌다.";
            if (!is_range_weapon(item) &&
            (damtype == DVORP_SLICING || damtype == DVORP_CHOPPING))
                {
            description += " 크고, 불타는 칼날은 "
                " 히드라 사냥꾼의 주요한 무기이다.";
                }
            break;
            case SPWPN_FREEZING:
            if (is_range_weapon(item))
                {
            description += "이 무기는 발사한 투사체가"
                "적중한 대상을 얼리도록 한다,";
                }
            else
                {
            description += "이 무기에는 타격된 대상을 얼리기 위해 "
                "특별히 마법이 부여돼있다,";
                }
            description += " 대부분의 적들에게 추가적인 피해를 주고 "
                "특별히 취약한 몇몇 적들에게는 최대 절반의 피해를 "
                "추가로 입힌다.";
            if (is_range_weapon(item))
            description += "투사체들은";
            else
            description += " 이 무기는";
            description += " 냉혈 동물들을 느리게 만들 수도 있다.";
            break;
            case SPWPN_HOLY_WRATH:
            description += "이 무기는 샤이닝원의 축복을 받았다";
            if (is_range_weapon(item))
                {
            description += "와, 여기에서 발사된 ";
            description += ammo_name(item);
            description += " 투사체들은";
                }
            else
            description += " ";
            description += "언데드들과 악마들에게 큰 피해를 유발하도록 샤이닝 원으로부터 축복받았다.";
            break;
            case SPWPN_ELECTROCUTION:
            if (is_range_weapon(item))
                {
            description += "이 무기는 발사한 탄환에 전하를 "
                "충전한다; 탄환이 적중하는 경우 때때로 이 탄환들에서 "
                "전하가 방출되고 강력한 피해를 입힐 수 있다.";
                }
            else
                {
            description += "때로, 적을 맞추는 경우, 이것은 "
                "전기 에너지를 방출하여 강력한 피해를 입힐 수 "
                "있다.";
                }
            break;
            case SPWPN_VENOM:
            if (is_range_weapon(item))
            description += "이 무기는 발사하는 탄환에 독성을 부여한다.";
            else
            description += "이 무기는 적중한 대상을 중독시킨다.";
            break;
            case SPWPN_PROTECTION:
            description += "이 무기는 사용자를 부상으로부터 "
                "보호한다 (피격 시 +AC).";
            break;
            case SPWPN_DRAINING:
            description += "참으로 소름끼치는 이 무기는, 맞은 대상의 "
                "생명력을 흡수한다.";
            break;
            case SPWPN_SPEED:
            description += "이 무기를 들면 확실히 빠르게 공격할 수 있다.";
            break;
            case SPWPN_VORPAL:
            if (is_range_weapon(item))
                {
                description += "이 무기로부터 발사된 모든 ";
                description += ammo_name(item);
                description += "은(는) 추가적인 피해를 입힌다.";
                }
            else
                {
            description += "이 무기는 적에게 추가적인 피해를 "
                "입힌다.";
                }
            break;
            case SPWPN_CHAOS:
            if (is_range_weapon(item))
                {
            description += "이 무기에서 발사된 각각의 투사체는 "
                "매번 다른, 무작위 효과를 가진다.";
                }
            else
                {
            description += "이 무기가 적대적인 대상을 때릴 때마다, 이것은 "
                "매번 다른, 무작위한 효과를 가진다.";
                }
            break;
            case SPWPN_VAMPIRISM:
            description += "이 무기는 추가적인 피해를 입히지는 못하지만, 사용자가 "
                "살아있는 적에게 피해를 줄 경우 사용자를 회복시킨다.";
            break;
            case SPWPN_PAIN:
            description += "강령 마법에 숙달된 이가 사용할 경우, "
                "이 무기는 생명체에게 추가적인 피해를 입힌다.";
            break;
            case SPWPN_DISTORTION:
            description += "이 무기는 주변의 공간을 전이시키고 왜곡시킨다. "
                "이 장비를 해제하는 것은 추방이나 큰 피해를 유발할 수 있다.";
            break;
            case SPWPN_PENETRATION:
            description += "이 무기에서 발사된 탄환들은 맞은 대상들을 "
                "관통하며, 최대 사거리에 도달할 때 까지 경로에 있는 "
                "모든 대상들을 맞출 수도 있다.";
            break;
            case SPWPN_REAPING:
            description += "이것으로 인해 사망한 몬스터는 괜찮은 상태의 "
                "시체를 남기며, 이 시체들은 다시 일으켜져서 "
                "살해자에게 우호적인 좀비로서 살아날 것이다.";
            break;
            case SPWPN_ANTIMAGIC:
            description += "이 무기는 사용자의 마법적인 에너지를 감소시키며, "
                "맞은 대상들의 주문과 마법적인 능력들을 방해한다 "
                "자연적인 능력들과 성스러운 기도들은 영향을 "
                "받지 않는다.";
            break;
            case SPWPN_NORMAL:
            ASSERT(enchanted);
            description += "이 무기는 특별한 속성이 부여되어 있지 않지만,"
                "(화염, 냉기 등), 좋든 나쁘든 간에 여전히 어떤 속성이 "
                "부여되어 있다.";
            break;
        }
    }

    if (you.duration[DUR_EXCRUCIATING_WOUNDS] && &item == you.weapon())
    {
        description += "\nIt is temporarily rebranded; it is actually a";
        if ((int) you.props[ORIGINAL_BRAND_KEY] == SPWPN_NORMAL)
            description += "n unbranded weapon.";
        else
        {
            description += " weapon of "
                        + ego_type_string(item, false, you.props[ORIGINAL_BRAND_KEY])
                        + ".";
        }
    }

    if (is_artefact(item))
    {
        string rand_desc = _randart_descrip(item);
        if (!rand_desc.empty())
        {
            description += "\n";
            description += rand_desc;
        }

        // XXX: Can't happen, right?
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES)
            && item_type_known(item))
        {
            description += "\n이 무기는 숨겨진 속성을 가지고 있는 것 같다.";
        }
    }

    if (verbose)
    {
        description += "\n\n이 ";
        if (is_unrandom_artefact(item))
            description += get_artefact_base_name(item);
        else
            description += "무기는";
        description += " 에서 얻었다";

        const skill_type skill = item_attack_skill(item);

        description +=
            make_stringf(" '%s' 카테고리. ",
                         skill == SK_FIGHTING ? "buggy" : skill_name(skill));

        description += _handedness_string(item);

        if (!you.could_wield(item, true) && crawl_state.need_save)
            description += "\n이것은 당신이 장비하기엔 너무 크다.";
    }

    if (!is_artefact(item))
    {
        if (item_ident(item, ISFLAG_KNOW_PLUSES) && item.plus >= MAX_WPN_ENCHANT)
            description += "\n이것은 더이상 강화될 수 없다.";
        else
        {
            description += "\n이것은 최대 다음 수치까지 강화될 수 있다 : +"
                           + to_string(MAX_WPN_ENCHANT) + ".";
        }
    }

    return description;
}

static string _describe_ammo(const item_def &item)
{
    string description;

    description.reserve(64);

    const bool can_launch = has_launcher(item);
    const bool can_throw  = is_throwable(&you, item, true);

    if (item.brand && item_type_known(item))
    {
        description += "\n\n";

        string threw_or_fired;
        if (can_throw)
        {
            threw_or_fired += "투척된";
            if (can_launch)
                threw_or_fired += " 혹은 ";
        }
        if (can_launch)
            threw_or_fired += "발사된";

        switch (item.brand)
        {
#if TAG_MAJOR_VERSION == 34
        case SPMSL_FLAME:
        description += "이것은 타격한 대상을 불태우며, 대부분의 적들에게 추가적인 "
                       "피해를 입히고 특히 취약한 적들에게는 최대 절반의 추가 피해를 "
                       "입힐 수 있다. 일반 탄환과 비교해 봤을 때 "
                       "충격에 의해 파괴될 확률이 두 배이다.";
            break;
        case SPMSL_FROST:
        description += "이것은 맞은 대상을 빙결시키며, 대부분의 적들에게 추가적인 "
                       "피해를 입히고 특히 취약한 적들에게는 최대 절반의 추가 피해를 "
                       "입힐 수 있다. 또한, 냉혈 피조물들에게는 감속을 "
                       "유발할 수 있다. 일반 탄화과 비교해 봤을 때 "
                       "충격에 의해 파괴될 확률이 두 배이다.";
            break;
#endif
        case SPMSL_CHAOS:
            description += "언제 ";

            if (can_throw)
            {
                description += "투척됐는지, ";
                if (can_launch)
                    description += "혹은 ";
            }

            if (can_launch)
                description += "적절한 발사대에서 발사된, ";

            description += "이 투사체는 무작위 효과를 가진다.";
            break;
        case SPMSL_POISONED:
            description += "이 투사체에는 독이 발라져 있다.";
            break;
        case SPMSL_CURARE:
        description += "이 바늘에는 질식을 유발하는 물질이 발라져 있으며 "
                       "적중한 대상에게 직접적인 피해를 가하는 한편 "
                       "중독과 감속 또한 유발한다.\n"
                       "이 바늘은 다른 바늘들과 비교해 보았을 때 충격에 의해 파괴될 확률이 "
                       "두 배이다.";
            break;
        case SPMSL_PARALYSIS:
            description += "이 투사체에는 마비를 유발하는 물질이 발라져 있다.";
            break;
        case SPMSL_SLEEP:
            description += "이 투사체에는 급속 진정제가 입혀져 있다.";
            break;
        case SPMSL_CONFUSION:
        description += "이 투사체에는 혼란을 유발하는 물질이 발라져 있다.";
        break;
#if TAG_MAJOR_VERSION == 34
    case SPMSL_SICKNESS:
        description += "이 투사체는 질병을 유발하는 무언가에 의해 오염되었다.";
        break;
#endif
    case SPMSL_FRENZY:
        description += "이 투사체는 적중한 대상을 생각없는 분노 상태로 만들는 물질이 "
                    "발라져 있으며, 분노한 대상은 자신에게 우호적인 대상들을 "
                    "공격한다.";
        break;
    case SPMSL_RETURNING:
        description += "숙련된 사용자는 이 투사체를 투척해 다시 자신에게 돌아오도록 "
                    "할 수 있다.";
        break;
    case SPMSL_PENETRATION:
        description += "이 투사체는 적중한 모든 대상을 관통하며, "
                    "최대 사거리에 도달할 때까지 경로에 있는 모든 대상들을 "
                    "맞출 수도 있다.";
        break;
    case SPMSL_DISPERSAL:
        description += "이 투사체는 적중한 대상에게 순간이동을 유발하며, 이들은 대개"
                    "로부터 멀어지는 방향으로 순간이동한다"
                    "한 사람 " + threw_or_fired + " 그것을.";
        break;
    case SPMSL_EXPLODING:
        description += "이 투사체는 대상을 적중시키거나, 장애물을 맞추거나, "
                    "혹은 최대 사거리에 도달하는 경우 폭발하여 파편으로 "
                    "분해된다.";
        break;
    case SPMSL_STEEL:
        description += "이 탄환은 일반적인 탄환에 비해 증가한 피해를 입힌다.";
        break;
    case SPMSL_SILVER:
        description += "이 투사체는 혼돈의 존재들과 마법적으로 변형된 존재들에 대해 "
                    "엄청나게 증가된 피해를 입힌다. 또한 변이된 존재들에 대해서도 "
                    "추가적인 피해를 입히는데, 피해를 입히는 정도는 "
                    "변이된 정도에 따라 다르다.";
            break;
        }
    }

    const int dam = property(item, PWPN_DAMAGE);
    if (dam)
    {
        const int throw_delay = (10 + dam / 2);
        const string your_skill = crawl_state.need_save ?
                make_stringf("\n (당신의 기술: %.1f)",
                    (float) you.skill(SK_THROWING, 10) / 10)
                    : "";

        description += make_stringf(
            "\n기본 데미지: %d 기본 공격 속도: %.1f"
            "\n이 투사체의 최소 공격 속도는 (%.1f) "
              "다음 기술 레벨에서 얻어진다 : %d."
            "%s",
            dam,
            (float) throw_delay / 10,
            (float) FASTEST_PLAYER_THROWING_SPEED / 10,
            (throw_delay - FASTEST_PLAYER_THROWING_SPEED) * 2,
            your_skill.c_str()
        );
    }


    if (ammo_always_destroyed(item))
        description += "\n충격으로 인해 항상 파괴된다.";
    else if (!ammo_never_destroyed(item))
        description += "\n충격으로 인해 파괴 될 수 있다.";

    return description;
}

static string _warlock_mirror_reflect_desc()
{
    const int SH = crawl_state.need_save ? player_shield_class() : 0;
    const int reflect_chance = 100 * SH / omnireflect_chance_denom(SH);
    return "\n\nWith your current SH, it has a " + to_string(reflect_chance) +
           "% chance to reflect enchantments and other normally unblockable "
           "effects.";
}

static string _describe_armour(const item_def &item, bool verbose)
{
    string description;

    description.reserve(200);

    if (verbose)
    {
        if (is_shield(item))
        {
            const float skill = you.get_shield_skill_to_offset_penalty(item);
            description += "\n";
            description += "\n기본 방패 수치: "
                        + to_string(property(item, PARM_AC));
            if (!is_useless_item(item))
            {
                description += "       페널티 무효화 기술 수치: "
                            + make_stringf("%.1f", skill);
                if (crawl_state.need_save)
                {
                    description += "\n                            "
                                + make_stringf("(당신의 기술: %.1f)",
                                               (float) you.skill(SK_SHIELDS, 10) / 10);
                }
                else
                    description += "\n";
            }

            if (is_unrandom_artefact(item, UNRAND_WARLOCK_MIRROR))
                description += _warlock_mirror_reflect_desc();
        }
        else
        {
            const int evp = property(item, PARM_EVASION);
            description += "\n\n기본 갑옷 수치: "
                        + to_string(property(item, PARM_AC));
            if (get_armour_slot(item) == EQ_BODY_ARMOUR)
            {
                description += "       방해도 수치: "
                            + to_string(-evp / 10);
            }
            // Bardings reduce evasion by a fixed amount, and don't have any of
            // the other effects of encumbrance.
            else if (evp)
            {
                description += "       회피: "
                            + to_string(evp / 30);
            }

            // only display player-relevant info if the player exists
            if (crawl_state.need_save && get_armour_slot(item) == EQ_BODY_ARMOUR)
            {
                description += make_stringf("\n이 종류의 보통 갑옷을 입으면 "
                                            "다음과 같은 효과를 얻을 수 있다: %d AC",
                                             you.base_ac_from(item));
            }
        }
    }

    const int ego = get_armour_ego_type(item);
    const bool enchanted = get_equip_desc(item) && ego == SPARM_NORMAL
                           && !item_ident(item, ISFLAG_KNOW_PLUSES);

    if ((ego != SPARM_NORMAL || enchanted) && item_type_known(item) && verbose)
    {
        description += "\n\n";

        switch (ego)
        {
        case SPARM_RUNNING:
            if (item.sub_type == ARM_NAGA_BARDING)
            description += "이것은 착용자가 대단한 속도로 이동하게 해 준다.";
            else
            description += "이것은 착용자가 대단한 속도로 달릴 수 있게 해 준다.";
            break;
        case SPARM_FIRE_RESISTANCE:
            description += "이것은 착용자를 열로부터 보호한다.";
            break;
        case SPARM_COLD_RESISTANCE:
            description += "이것은 착용자를 냉기로부터 보호한다.";
            break;
        case SPARM_POISON_RESISTANCE:
            description += "이것은 착용자를 독으로부터 보호한다.";
            break;
        case SPARM_SEE_INVISIBLE:
            description += "이것은 착용자가 보이지 않는 존재를 볼 수 있도록 해 준다.";
            break;
        case SPARM_INVISIBILITY:
            description += "이것이 활성화되면 착용자를 다른 이의 시야로부터 "
                "숨기지만, 동시에 착용자의 신진대사 속도 또한 "
                "크게 향상시킨다.";
            break;
        case SPARM_STRENGTH:
            description += "이것은 착용자의 힘을 증가시킨다 (힘 +3).";
            break;
        case SPARM_DEXTERITY:
            description += "이것은 착용자의 민첩성을 증가시킨다 (민첩 +3).";
            break;
        case SPARM_INTELLIGENCE:
            description += "이것은 당신을 더 영리하게 만들어준다 (지능 +3).";
            break;
        case SPARM_PONDEROUSNESS:
            description += "이것은 매우 육중하여, 당신의 움직임을 느리게 만든다.";
            break;
        case SPARM_FLYING:
            description += "이것은 착용자가 원하는 동안 계속해서 날 수 있는 능력을 "
                "가지고 있다.";
            break;
        case SPARM_MAGIC_RESISTANCE:
            description += "이것은 착용자의 마법에 대한 저항력을 "
                "증가시킨다.";
            break;
        case SPARM_PROTECTION:
            description += "이것은 착용자를 피해로부터 보호한다 (AC +3).";
            break;
        case SPARM_STEALTH:
            description += "이것은 착용자의 은밀도를 증가시킨다.";
            break;
        case SPARM_RESISTANCE:
            description += "이것은 착용자를 열과 냉기 둘 다로부터 "
                "보호한다.";
            break;
        case SPARM_POSITIVE_ENERGY:
            description += "이것은 착용자를 음에너지로부터 "
                "보호한다.";
            break;

        // This is only for robes.
        case SPARM_ARCHMAGI:
            description += "이것은 착용자의 마법 주문의 위력을 "
                "증가시킨다.";
            break;
#if TAG_MAJOR_VERSION == 34
        case SPARM_PRESERVATION:
            description += "이것은 별다른 능력이 없다.";
            break;
#endif
        case SPARM_REFLECTION:
            description += "이것은 날아오는 딱딱한 물건들을 반사하여 "
                "날아온 방향으로 되돌려 보낸다.";
            break;

        case SPARM_SPIRIT_SHIELD:
            description += "이것은 마법 능력을 대가로 착용자를 피해로부터 "
                "보호한다.";
            break;

        case SPARM_NORMAL:
            ASSERT(enchanted);
            description += "이것은 특별한 능력은 없지만 (불에 대한 저항 등,) "
                           "좋든 나쁘든 간에 어떤 능력이 부여되어 있긴 하다"
                           ".";

            break;

        // This is only for gloves.
        case SPARM_ARCHERY:
            description += "이것은 당신이 활과 석궁 등의 원거리 무기를 "
                           "보다 탁월하게 다루게 해 준다. (Slay+4).";
            break;

        // These are only for scarves.
        case SPARM_REPULSION:
            description += "이것은 투사체를 반사함으로서 착용자를 보호한다.";
            break;

        case SPARM_CLOUD_IMMUNE:
            description += "이것은 착용자를 구름의 영향으로부터 완벽하게 보호한다.";
            break;
        }
    }

    if (is_artefact(item))
    {
        string rand_desc = _randart_descrip(item);
        if (!rand_desc.empty())
        {
            description += "\n";
            description += rand_desc;
        }

        // Can't happen, right? (XXX)
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES) && item_type_known(item))
            description += "\n이 방어구에는 숨겨진 능력이 있는 것 같다.";
    }
    else
    {
        const int max_ench = armour_max_enchant(item);
        if (item.plus < max_ench || !item_ident(item, ISFLAG_KNOW_PLUSES))
        {
            description += "\n\n이것은 최대 다음 수치까지 강화될 수 있다 : +"
                           + to_string(max_ench) + ".";
        }
        else
            description += "\n\n이것은 더 이상 강화될 수 없다.";
    }

    return description;
}

static string _describe_jewellery(const item_def &item, bool verbose)
{
    string description;

    description.reserve(200);

    if (verbose && !is_artefact(item)
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        // Explicit description of ring power.
        if (item.plus != 0)
        {
            switch (item.sub_type)
            {
            case RING_PROTECTION:
                description += make_stringf("\n이것은 당신의 AC에 영향을 준다 (%+d).",
                                            item.plus);
                break;

            case RING_EVASION:
                description += make_stringf("\n이것은 당신의 회피력에 영향을 준다 (%+d).",
                                            item.plus);
                break;

            case RING_STRENGTH:
                description += make_stringf("\n이것은 당신의 힘에 영향을 준다 (%+d).",
                                            item.plus);
                break;

            case RING_INTELLIGENCE:
                description += make_stringf("\n이것은 당신의 지능에 영향을 준다 (%+d).",
                                            item.plus);
                break;

            case RING_DEXTERITY:
                description += make_stringf("\n이것은 당신의 민첩에 영향을 준다 (%+d).",
                                            item.plus);
                break;

            case RING_SLAYING:
                description += make_stringf("\n이것은 당신의 원거리, 근거리 공격의"
                      " 피해량과 적중률에 영향을 준다 (%+d).",
                      item.plus);
                break;

            case AMU_REFLECTION:
                description += make_stringf("\n이것은 당신의 SH에 영향을 준다 (%+d).",
                                            item.plus);
                break;

            default:
                break;
            }
        }
    }

    // Artefact properties.
    if (is_artefact(item))
    {
        string rand_desc = _randart_descrip(item);
        if (!rand_desc.empty())
        {
            description += "\n";
            description += rand_desc;
        }
        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES) ||
            !item_ident(item, ISFLAG_KNOW_TYPE))
        {
            description += "\n이 ";
            description += (jewellery_is_amulet(item) ? "목걸이" : "반지");
            description += "에는 숨겨진 능력이 있을 수도 있다.";
        }
    }

    return description;
}

static bool _compare_card_names(card_type a, card_type b)
{
    return string(card_name(a)) < string(card_name(b));
}

static bool _check_buggy_deck(const item_def &deck, string &desc)
{
    if (!is_deck(deck))
    {
        desc += "This isn't a deck at all!\n";
        return true;
    }

    const CrawlHashTable &props = deck.props;

    if (!props.exists(CARD_KEY)
        || props[CARD_KEY].get_type() != SV_VEC
        || props[CARD_KEY].get_vector().get_type() != SV_BYTE
        || cards_in_deck(deck) == 0)
    {
        return true;
    }

    return false;
}

static string _describe_deck(const item_def &item)
{
    string description;

    description.reserve(100);

    description += "\n";

    if (_check_buggy_deck(item, description))
        return "";

    if (item_type_known(item))
        description += deck_contents(item.sub_type) + "\n";

    description += make_stringf("\n대부분의 덱은 %d 에서 %d 카드로 시작한다.",
                                MIN_STARTING_CARDS,
                                MAX_STARTING_CARDS);

    const vector<card_type> drawn_cards = get_drawn_cards(item);
    if (!drawn_cards.empty())
    {
        description += "\n";
        description += "뽑은 카드(들): ";
        description += comma_separated_fn(drawn_cards.begin(),
                                          drawn_cards.end(),
                                          card_name);
    }

    const int num_cards = cards_in_deck(item);
    // The list of known cards, ending at the first one not known to be at the
    // top.
    vector<card_type> seen_top_cards;
    // Seen cards in the deck not necessarily contiguous with the start. (If
    // Nemelex wrath shuffled a deck that you stacked, for example.)
    vector<card_type> other_seen_cards;
    bool still_contiguous = true;
    for (int i = 0; i < num_cards; ++i)
    {
        uint8_t flags;
        const card_type card = get_card_and_flags(item, -i-1, flags);
        if (flags & CFLAG_SEEN)
        {
            if (still_contiguous)
                seen_top_cards.push_back(card);
            else
                other_seen_cards.push_back(card);
        }
        else
            still_contiguous = false;
    }

    if (!seen_top_cards.empty())
    {
        description += "\n";
        description += "다음 카드(들): ";
        description += comma_separated_fn(seen_top_cards.begin(),
                                          seen_top_cards.end(),
                                          card_name);
    }
    if (!other_seen_cards.empty())
    {
        description += "\n";
        sort(other_seen_cards.begin(), other_seen_cards.end(),
             _compare_card_names);

        description += "본 카드(들): ";
        description += comma_separated_fn(other_seen_cards.begin(),
                                          other_seen_cards.end(),
                                          card_name);
    }

    return description;
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

    if (!dump && !lookup)
    {
        string name = item.name(DESC_INVENTORY_EQUIP);
        if (!in_inventory(item))
            name = uppercase_first(name);
        description << name << ".";
    }

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
                if (item_type_known(item))
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
        if (!verbose
            && (Options.dump_book_spells || is_random_artefact(item)))
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

    case OBJ_WANDS:
    {
        const bool known_empty = is_known_empty_wand(item);

        if (!item_ident(item, ISFLAG_KNOW_PLUSES) && !known_empty)
        {
            description << "\n감정되지 않은 상태로 발동될 경우, "
                           "장비에 대한 미숙함으로 인해 수 회의 충전량이 "
                           "낭비될 것이다.";
        }


        if (item_type_known(item) && !item_ident(item, ISFLAG_KNOW_PLUSES))
        {
            description << "\n이것은 최대 " << wand_max_charges(item)
                        << " 회의 충전량을 가진다.";
        }

        if (known_empty)
            description << "\n안타깝게도, 이것은 더 이상의 충전량이 남아있지 않다.";
        break;
    }

    case OBJ_CORPSES:
        if (item.sub_type == CORPSE_SKELETON)
            break;

        // intentional fall-through
    case OBJ_FOOD:
        if (item.base_type == OBJ_CORPSES || item.sub_type == FOOD_CHUNK)
        {
            switch (determine_chunk_effect(item))
            {
            case CE_MUTAGEN:
                description << "\n\n이 고기를 먹는 것은 무작위 돌연변이를 "
                               "유발한다.";
                break;
            case CE_NOXIOUS:
                description << "\n\n이 고기는 독성이다.";
                break;
            default:
                break;
            }
        }
        break;

    case OBJ_STAVES:
        {
            string stats = "\n";
            _append_weapon_stats(stats, item);
            description << stats;
        }
        description << "\n\n이것은 '봉' 범주 안에 들어간다. ";
        description << _handedness_string(item);
        break;

    case OBJ_MISCELLANY:
        if (is_deck(item))
            description << _describe_deck(item);
        if (item.sub_type == MISC_ZIGGURAT && you.zigs_completed)
        {
            const int zigs = you.zigs_completed;
            description << "\n\n이것 "
                        << (zigs >= 27 ? "은 눈이 멀 것 같은 광채를 내뿜고 " : // just plain silly
                            zigs >=  9 ? "은 눈부신 광채에 둘러싸여 " :
                            zigs >=  3 ? "은 밝게 빛나고 " :
                                         "은 은은하게 빛나고 ")
                        << "있다.";
        }
        if (is_xp_evoker(item))
        {
            description << "\n\n한 번 "
                        << (item.sub_type == MISC_LIGHTNING_ROD
                            ? "모든 충전량이 사용된"
                            : "발동된")
                        << ", 이 도구 "
                        << (!item_is_horn_of_geryon(item) ?
                           "그리고 같은 종류의 모든 다른 도구들 " : "")
                        << "은 일시적으로 비활성화 상태가 될 것이다. 그러나, "
                        << (!item_is_horn_of_geryon(item) ? "그것들은 " : "그것은 ")
                        << "당신이 경험치를 얻음에 따라, 재충전할 것이다."
                        << (!evoker_charges(item.sub_type) ?
                           " 이 장치는 현재 비활성화 상태이다." : "");
        }
        break;

    case OBJ_POTIONS:
#ifdef DEBUG_BLOOD_POTIONS
        // List content of timer vector for blood potions.
        if (!dump && is_blood_potion(item))
        {
            item_def stack = static_cast<item_def>(item);
            CrawlHashTable &props = stack.props;
            if (!props.exists("timer"))
                description << "\nTimers not yet initialized.";
            else
            {
                CrawlVector &timer = props["timer"].get_vector();
                ASSERT(!timer.empty());

                description << "\nQuantity: " << stack.quantity
                            << "        Timer size: " << (int) timer.size();
                description << "\nTimers:\n";
                for (const CrawlStoreValue& store : timer)
                    description << store.get_int() << "  ";
            }
        }
#endif

    case OBJ_SCROLLS:
    case OBJ_ORBS:
    case OBJ_GOLD:
    case OBJ_RUNES:
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
#endif
        // No extra processing needed for these item types.
        break;

    default:
        die("Bad item class");
    }

    if (!verbose && item_known_cursed(item))
        description << "\n이것은 저주받았다.";
    else
    {
        if (verbose)
        {
            if (need_extra_line)
                description << "\n";
            if (item_known_cursed(item))
                description << "\n이것은 저주받았다.";

            if (is_artefact(item))
            {
                if (item.base_type == OBJ_ARMOUR
                    || item.base_type == OBJ_WEAPONS)
                {
                    description << "\n이 고대의 아티팩트는 마법이나 평범한 "
                        "방법으로는 변화시킬 수 없다.";
                }
                // Randart jewellery has already displayed this line.
                else if (item.base_type != OBJ_JEWELLERY
                         || (item_type_known(item) && is_unrandom_artefact(item)))
                {
                    description << "\n이것은 고대의 아티팩트이다.";
                }
            }
        }
    }

    if (god_hates_item_handling(item))
    {
        description << "\n\n" << uppercase_first(god_name(you.religion))
                    << "은(는) 그러한 물건을 사용하는 것을 허락하지 않는다.";
    }

    if (verbose && origin_describable(item))
        description << "\n" << origin_desc(item) << ".";

    // This information is obscure and differs per-item, so looking it up in
    // a docs file you don't know to exist is tedious.
    if (verbose)
    {
        description << "\n\n" << "Stash search prefixes: "
                    << userdef_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
        string menu_prefix = item_prefix(item, false);
        if (!menu_prefix.empty())
            description << "\nMenu/colouring prefixes: " << menu_prefix;
    }

    return description.str();
}

void get_feature_desc(const coord_def &pos, describe_info &inf)
{
    dungeon_feature_type feat = env.map_knowledge(pos).feat();

    string desc      = feature_description_at(pos, false, DESC_A, false);
    string db_name   = feat == DNGN_ENTER_SHOP ? "a shop" : desc;
    string long_desc = getLongDescription(db_name);

    inf.title = uppercase_first(desc);
    if (!ends_with(desc, ".") && !ends_with(desc, "!")
        && !ends_with(desc, "?"))
    {
        inf.title += ".";
    }

    const string marker_desc =
        env.markers.property_at(pos, MAT_ANY, "feature_description_long");

    // suppress this if the feature changed out of view
    if (!marker_desc.empty() && grd(pos) == feat)
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
    }

    // mention the ability to pray at altars
    if (feat_is_altar(feat))
    {
        long_desc +=
            make_stringf("\n(Pray here with '%s' to learn more.)\n",
                         command_to_string(CMD_GO_DOWNSTAIRS).c_str());
    }

    inf.body << long_desc;

    if (!feat_is_solid(feat))
    {
        string area_desc = "";
        if (haloed(pos) && !umbraed(pos))
            area_desc += "\n" + getLongDescription("haloed");
        if (umbraed(pos) && !haloed(pos))
            area_desc += "\n" + getLongDescription("umbraed");
        if (liquefied(pos))
            area_desc += "\n" + getLongDescription("liquefied");
        if (disjunction_haloed(pos))
            area_desc += "\n" + getLongDescription("disjunction haloed");

        inf.body << area_desc;
    }

    if (const cloud_type cloud = env.map_knowledge(pos).cloud())
    {
        const string cl_name = cloud_type_name(cloud);
        const string cl_desc = getLongDescription(cl_name + " cloud");
        inf.body << "\n\nA cloud of " << cl_name
                 << (cl_desc.empty() ? "." : ".\n\n")
                 << cl_desc << extra_cloud_info(cloud);
    }

    inf.quote = getQuoteString(db_name);
}

/// A message explaining how the player can toggle between quote &
static const string _toggle_message =
    "누르시오 '<w>!</w>'"
#ifdef USE_TILE_LOCAL
    " 혹은 <w>우클릭</w>"
#endif
    " 설명과 인용구를 전환하려면.";

/**
 * If the given description has an associated quote, print a message at the
 * bottom of the screen explaining how the player can toggle between viewing
 * that quote & the description, and then check whether the input corresponds
 * to such a toggle.
 *
 * @param inf[in]       The description in question.
 * @param key[in,out]   The input command. If zero, is set to getchm().
 * @return              Whether the description & quote should be toggled.
 */
static int _print_toggle_message(const describe_info &inf, int& key)
{
    mouse_control mc(MOUSE_MODE_MORE);

    if (inf.quote.empty())
    {
        if (!key)
            key = getchm();
        return false;
    }

    const int bottom_line = min(30, get_number_of_lines());
    cgotoxy(1, bottom_line);
    formatted_string::parse_string(_toggle_message).display();
    if (!key)
        key = getchm();

    if (key == '!' || key == CK_MOUSE_CMD)
        return true;

    return false;
}

void describe_feature_wide(const coord_def& pos, bool show_quote)
{
    describe_info inf;
    get_feature_desc(pos, inf);

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_feature");
#endif

    if (show_quote)
        _print_quote(inf);
    else
        print_description(inf);

    if (crawl_state.game_is_hints())
        hints_describe_pos(pos.x, pos.y);

    int key = 0;
    if (_print_toggle_message(inf, key))
        describe_feature_wide(pos, !show_quote);
}

void get_item_desc(const item_def &item, describe_info &inf)
{
    // Don't use verbose descriptions if the item contains spells,
    // so we can actually output these spells if space is scarce.
    const bool verbose = !item.has_spells();
    inf.body << get_item_description(item, verbose);
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
    case OBJ_MISCELLANY:
        if (!item_is_equipped(item))
        {
            if (item_is_wieldable(item))
                actions.push_back(CMD_WIELD_WEAPON);
            if (is_throwable(&you, item))
                actions.push_back(CMD_QUIVER_ITEM);
        }
        break;
    case OBJ_MISSILES:
        if (you.species != SP_FELID)
            actions.push_back(CMD_QUIVER_ITEM);
        break;
    case OBJ_ARMOUR:
        if (item_is_equipped(item))
            actions.push_back(CMD_REMOVE_ARMOUR);
        else
            actions.push_back(CMD_WEAR_ARMOUR);
        break;
    case OBJ_FOOD:
        if (can_eat(item, true, false))
            actions.push_back(CMD_EAT);
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
        if (!you_foodless()) // mummies and lich form forbidden
            actions.push_back(CMD_QUAFF);
        break;
    default:
        ;
    }
#if defined(CLUA_BINDINGS)
    if (clua.callbooleanfn(false, "ch_item_wieldable", "i", &item))
        actions.push_back(CMD_WIELD_WEAPON);
#endif

    if (item_is_evokable(item))
        actions.push_back(CMD_EVOKE);

    actions.push_back(CMD_DROP);

    if (!crawl_state.game_is_tutorial())
        actions.push_back(CMD_INSCRIBE_ITEM);

    return actions;
}

static string _actions_desc(const vector<command_type>& actions, const item_def& item)
{
    static const map<command_type, string> act_str =
    {
        { CMD_WIELD_WEAPON, "(w)ield" },
        { CMD_UNWIELD_WEAPON, "(u)nwield" },
        { CMD_QUIVER_ITEM, "(q)uiver" },
        { CMD_WEAR_ARMOUR, "(w)ear" },
        { CMD_REMOVE_ARMOUR, "(t)ake off" },
        { CMD_EVOKE, "e(v)oke" },
        { CMD_EAT, "(e)at" },
        { CMD_READ, "(r)ead" },
        { CMD_WEAR_JEWELLERY, "(p)ut on" },
        { CMD_REMOVE_JEWELLERY, "(r)emove" },
        { CMD_QUAFF, "(q)uaff" },
        { CMD_DROP, "(d)rop" },
        { CMD_INSCRIBE_ITEM, "(i)nscribe" },
        { CMD_ADJUST_INVENTORY, "(=)adjust" },
    };
    return "You can "
           + comma_separated_fn(begin(actions), end(actions),
                                [] (command_type cmd)
                                {
                                    return act_str.at(cmd);
                                },
                                ", or ")
           + " the " + item.name(DESC_BASENAME) + ".";
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
        { CMD_EAT,              'e' },
        { CMD_READ,             'r' },
        { CMD_WEAR_JEWELLERY,   'p' },
        { CMD_REMOVE_JEWELLERY, 'r' },
        { CMD_QUAFF,            'q' },
        { CMD_DROP,             'd' },
        { CMD_INSCRIBE_ITEM,    'i' },
        { CMD_ADJUST_INVENTORY, '=' },
    };

    key = tolower(key);

    for (auto cmd : actions)
        if (key == act_key.at(cmd))
            return cmd;

    return CMD_NO_CMD;
}

/**
 * Do the specified action on the specified item.
 *
 * @param item    the item to have actions done on
 * @param actions the list of actions to search in
 * @param keyin   the key that was pressed
 * @return whether to stay in the inventory menu afterwards
 */
static bool _do_action(item_def &item, const vector<command_type>& actions, int keyin)
{
    const command_type action = _get_action(keyin, actions);
    if (action == CMD_NO_CMD)
        return true;

    const int slot = item.link;
    ASSERT_RANGE(slot, 0, ENDOFPACK);

    redraw_screen();
    switch (action)
    {
    case CMD_WIELD_WEAPON:     wield_weapon(true, slot);            break;
    case CMD_UNWIELD_WEAPON:   wield_weapon(true, SLOT_BARE_HANDS); break;
    case CMD_QUIVER_ITEM:      quiver_item(slot);                   break;
    case CMD_WEAR_ARMOUR:      wear_armour(slot);                   break;
    case CMD_REMOVE_ARMOUR:    takeoff_armour(slot);                break;
    case CMD_EVOKE:            evoke_item(slot);                    break;
    case CMD_EAT:              eat_food(slot);                      break;
    case CMD_READ:             read(&item);                         break;
    case CMD_WEAR_JEWELLERY:   puton_ring(slot);                    break;
    case CMD_REMOVE_JEWELLERY: remove_ring(slot, true);             break;
    case CMD_QUAFF:            drink(&item);                        break;
    case CMD_DROP:             drop_item(slot, item.quantity);      break;
    case CMD_INSCRIBE_ITEM:    inscribe_item(item);                 break;
    case CMD_ADJUST_INVENTORY: adjust_item(slot);                   break;
    default:
        die("illegal inventory cmd %d", action);
    }
    return false;
}

/**
 *  Describe any item in the game.
 *
 *  @param item       the item to be described.
 *  @param fixup_desc a function (possibly null) to modify the
 *                    description before it's displayed.
 *  @return whether to stay in the inventory menu afterwards.
 */
bool describe_item(item_def &item, function<void (string&)> fixup_desc)
{
    if (!item.defined())
        return true;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_item");
#endif

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
        desc += hints_describe_item(item);

    if (fixup_desc)
        fixup_desc(desc);
    // spellbooks have their own UIs, so we don't currently support the
    // inscribe/drop/etc prompt UI for them.
    // ...it would be nice if we did, though.
    if (item.has_spells())
    {
        formatted_string fdesc(formatted_string::parse_string(desc));
        list_spellset(item_spellset(item), nullptr, &item, fdesc);
        // only continue the inventory loop if we didn't start memorizing a
        // spell & didn't destroy the item for amnesia.
        return !already_learning_spell() && item.is_valid();
    }
    else
    {
        const bool do_actions = in_inventory(item) // Dead men use no items.
                                && !(you.pending_revival
                                     || crawl_state.updating_scores);
        vector<command_type> actions;
        formatted_scroller menu;
        menu.add_text(desc, false, get_number_of_cols());
        if (do_actions)
        {
            actions = _allowed_actions(item);
            menu.set_flags(menu.get_flags() | MF_ALWAYS_SHOW_MORE);
            menu.set_more(formatted_string(_actions_desc(actions, item), CYAN));
        }
        menu.show();
        if (do_actions)
            return _do_action(item, actions, menu.getkey());
        else
            return true;
    }
}

void inscribe_item(item_def &item)
{
    mprf_nocap(MSGCH_EQUIPMENT, "%s", item.name(DESC_INVENTORY).c_str());

    const bool is_inscribed = !item.inscription.empty();
    string prompt = is_inscribed ? "새겨진 글귀를 무엇으로 다시 새길것인가? "
                                 : "무엇으로 새기겠는가? ";

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
    you.redraw_quiver = true;
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
        || (get_spell_flags(spell) & SPFLAG_MONSTER))
    {
        return description; // all other info is player-dependent
    }

    const string failure = failure_rate_to_string(raw_spell_fail(spell));
    description += make_stringf("        Fail: %s", failure.c_str());

    description += "\n\nPower : ";
    description += spell_power_string(spell);
    description += "\nRange : ";
    description += spell_range_string(spell);
    description += "\nHunger: ";
    description += spell_hunger_string(spell);
    description += "\nNoise : ";
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
            if (you.species == SP_DEMIGOD)
            {
                result += "\n";
                result += "도대체 이것을 어떻게 얻은 것인가?";
            }
            else if (you_worship(GOD_TROG))
            {
                result += "\n";
                result += "기도술이 마법과 꽤나 연관이 있기 때문에, 트로그는 기도술을 "
                          "사용하지 않는다는 점을 기억하라.";
            }
            break;

        case SK_SPELLCASTING:
            if (you_worship(GOD_TROG))
            {
                result += "\n";
                result += "명심하라, 트로그가 이것에 대해 강력히 반대한다는 것을.";
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
    const int pow = mons_power_for_hd(spell, hd, false) / ENCH_POW_FACTOR;
    return min(cap, pow);
}

/**
 * What are the odds of the given spell, cast by a monster with the given
 * spell_hd, affecting the player?
 */
int hex_chance(const spell_type spell, const int hd)
{
    const int capped_pow = _hex_pow(spell, hd);
    const int chance = hex_success_chance(you.res_magic(), capped_pow,
                                          100, true);
    if (spell == SPELL_STRIP_RESISTANCE)
        return chance + (100 - chance) / 3; // ignores mr 1/3rd of the time
    return chance;
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
    if (!crawl_state.need_save || (get_spell_flags(spell) & SPFLAG_MONSTER))
        return ""; // all info is player-dependent

    string description;

    // Report summon cap
    const int limit = summons_limit(spell);
    if (limit)
    {
        description += "당신은 이 주문으로 소환한 존재를 " + number_in_words(limit)
                        + "마리만" + (limit > 1 ? "" : "")
                        + " 유지할 수 있다.\n";
    }

    if (god_hates_spell(spell, you.religion))
    {
        description += uppercase_first(god_name(you.religion))
                       + "은(는) 이 주문을 사용하는 행위에 눈살을 찌푸린다.\n";
        if (god_loathes_spell(spell, you.religion))
            description += "감히 이 주문을 캐스팅한다면 파문 당할 것이다!\n";
    }
    else if (god_likes_spell(spell, you.religion))
    {
        description += uppercase_first(god_name(you.religion))
                       + "이(가) 이 주문의 사용을 지원한다.\n";
    }

    if (!you_can_memorise(spell))
    {
        description += "\n당신은 이 주문을 외울 수 없다. 왜냐하면 "
                       + desc_cannot_memorise_reason(spell)
                       + "\n";
    }
    else if (spell_is_useless(spell, true, false))
    {
        description += "\n이 주문은 지금 사용해도 아무런 효과가 없다. 왜냐하면 "
                       + spell_uselessness_reason(spell, true, false)
                       + "\n";
    }

    return description;
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
 * @param item          The item holding the spell, if any.
 * @return              Whether you can memorise the spell.
 */
static bool _get_spell_description(const spell_type spell,
                                  const monster_info *mon_owner,
                                  string &description,
                                  const item_def* item = nullptr)
{
    description.reserve(500);

    description  = spell_title(spell);
    description += "\n\n";
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
        const int hd = mon_owner->spell_hd();
        const int range = mons_spell_range(spell, hd);
        description += "\nRange : "
                       + range_string(range, range, mons_char(mon_owner->type))
                       + "\n";

        // only display this if the player exists (not in the main menu)
        if (crawl_state.need_save && (get_spell_flags(spell) & SPFLAG_MR_CHECK)
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
            description += make_stringf("Chance to beat your MR: %d%%%s\n",
                                        hex_chance(spell, hd),
                                        wiz_info.c_str());
        }

    }
    else
        description += player_spell_desc(spell);

    // Don't allow memorization after death.
    // (In the post-game inventory screen.)
    if (crawl_state.player_is_dead())
        return false;

    const string quote = getQuoteString(string(spell_title(spell)) + " spell");
    if (!quote.empty())
        description += "\n" + quote;

    if (item && item->base_type == OBJ_BOOKS && in_inventory(*item)
        && !you.has_spell(spell) && you_can_memorise(spell))
    {
        description += "\n(M)emorise this spell.\n";
        return true;
    }

    return false;
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
 * memorizing the spell in question, if the player is able & chooses to do so.
 *
 * @param spelled   The spell in question.
 * @param mon_owner If this spell is being examined from a monster's
 *                  description, 'mon_owner' is that monster. Else, null.
 * @param item      The item holding the spell, if any.
 */
void describe_spell(spell_type spelled, const monster_info *mon_owner,
                    const item_def* item)
{
#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_spell");
#endif

    string desc;
    const bool can_mem = _get_spell_description(spelled, mon_owner, desc, item);
    print_description(desc);

    mouse_control mc(MOUSE_MODE_MORE);
    char ch;
    if ((ch = getchm()) == 0)
        ch = getchm();

    if (can_mem && toupper(ch) == 'M')
    {
        redraw_screen();
        if (!learn_spell(spelled) || !you.turn_is_over)
            more();
        redraw_screen();
    }
}

static string _describe_draconian(const monster_info& mi)
{
    string description;
    const int subsp = mi.draco_or_demonspawn_subspecies();

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
        description += "입과 콧구멍에서 스파크가 터져 나온다.";
        break;
    case MONS_YELLOW_DRACONIAN:
        description += "산성 기체가 주위를 소용돌이치고 있다.";
        break;
    case MONS_GREEN_DRACONIAN:
        description += "턱에서 독액이 뚝뚝 떨어지고 있다.";
        break;
    case MONS_PURPLE_DRACONIAN:
        description += "형체가 마법 에너지로 일렁거리고 있다.";
        break;
    case MONS_RED_DRACONIAN:
        description += "콧구멍에서 연기가 뿜어져 나오고 있다.";
        break;
    case MONS_WHITE_DRACONIAN:
        description += "콧구멍에서 서리가 뿜어져 나오고 있다.";
        break;
    case MONS_GREY_DRACONIAN:
        description += "비늘과 꼬리가 물에 적응했다.";
        break;
    case MONS_PALE_DRACONIAN:
        description += "초고열의 짙은 증기 속에서 모습이 잘 보이지 않는다.";
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
        return "강력하고 예측할 수 없는 파괴적인 주문들을 연성한다.";
    case MONS_WARMONGER:
        return "전투에 최적화되어 있으며, 끊임없이 싸우며 자신에게 적대적인 존재들의 "
               "마법 사용을 방해한다.";
    case MONS_CORRUPTER:
        return "주위의 공간을 타락시키며, 적대적인 존재들의 육체를 "
               "비틀어버릴 수 있다.";
    case MONS_BLACK_SUN:
        return "위험한 빛으로 빛나며, 죽음의 신에 대한 충성의 대가로 "
               "어둠의 힘을 사용한다.";
    default:
        return "";
    }
}

static string _describe_demonspawn_base(int species)
{
    switch (species)
    {
    case MONS_MONSTROUS_DEMONSPAWN:
        return "기원이 무엇이었는지는 모르겠지만, 그보다 훨씬 야수성을 띤 것은 분명해 보인다.";
    case MONS_GELID_DEMONSPAWN:
        return "얼음 갑옷으로 둘러싸여 있다.";
    case MONS_INFERNAL_DEMONSPAWN:
        return "강력한 열을 방출한다.";
    case MONS_TORTUROUS_DEMONSPAWN:
        return "앙상한 뼈들로 위협을 가한다.";
    }
    return "";
}

static string _describe_demonspawn(const monster_info& mi)
{
    string description;
    const int subsp = mi.draco_or_demonspawn_subspecies();

    description += _describe_demonspawn_base(subsp);

    if (subsp != mi.type)
    {
        const string demonspawn_role = _describe_demonspawn_role(mi.type);
        if (!demonspawn_role.empty())
            description += " " + demonspawn_role;
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
    case MR_RES_ROTTING:
        return "rotting";
    case MR_RES_NEG:
        return "negative energy";
    case MR_RES_DAMNATION:
        return "damnation";
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

/**
 * Describe monster attack 'flavours' that have extra range.
 *
 * @param flavour   The flavour in question; e.g. AF_REACH_STING.
 * @return          If the flavour has extra-long range, say so. E.g.,
 *                  " from a distance". (Else "").
 */
static const char* _flavour_range_desc(attack_flavour flavour)
{
    if (flavour == AF_REACH || flavour == AF_REACH_STING)
        return " from a distance";
    return "";
}

static string _flavour_base_desc(attack_flavour flavour)
{
    static const map<attack_flavour, string> base_descs = {
        { AF_ACID,              "deal extra acid damage"},
        { AF_BLINK,             "blink itself" },
        { AF_COLD,              "deal up to %d extra cold damage" },
        { AF_CONFUSE,           "cause confusion" },
        { AF_DRAIN_STR,         "drain strength" },
        { AF_DRAIN_INT,         "drain intelligence" },
        { AF_DRAIN_DEX,         "drain dexterity" },
        { AF_DRAIN_STAT,        "drain strength, intelligence or dexterity" },
        { AF_DRAIN_XP,          "drain skills" },
        { AF_ELEC,              "deal up to %d extra electric damage" },
        { AF_FIRE,              "deal up to %d extra fire damage" },
        { AF_HUNGER,            "cause hunger" },
        { AF_MUTATE,            "cause mutations" },
        { AF_POISON_PARALYSE,   "poison and cause paralysis or slowing" },
        { AF_POISON,            "cause poisoning" },
        { AF_POISON_STRONG,     "cause strong poisoning" },
        { AF_ROT,               "cause rotting" },
        { AF_VAMPIRIC,          "drain health from the living" },
        { AF_KLOWN,             "cause random powerful effects" },
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
        { AF_ENGULF,            "engulf with water" },
        { AF_PURE_FIRE,         "" },
        { AF_DRAIN_SPEED,       "drain speed" },
        { AF_VULN,              "reduce resistance to hostile enchantments" },
        { AF_SHADOWSTAB,        "deal increased damage when unseen" },
        { AF_DROWN,             "deal drowning damage" },
        { AF_CORRODE,           "cause corrosion" },
        { AF_SCARAB,            "drain speed and drain health" },
        { AF_TRAMPLE,           "knock back the defender" },
        { AF_REACH_STING,       "cause poisoning" },
        { AF_WEAKNESS,          "cause weakness" },
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
    const string flavour_desc = make_stringf(base_desc.c_str(), flavour_dam);

    if (!flavour_triggers_damageless(flavour)
        && flavour != AF_KITE && flavour != AF_SWOOP)
    {
        return " to " + flavour_desc + " if any damage is dealt";
    }

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

    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
    {
        const mon_attack_def &attack = mi.attack[i];
        if (attack.type == AT_NONE)
            break; // assumes there are no gaps in attack arrays

        const item_def* weapon = _weapon_for_attack(mi, i);
        mon_attack_info attack_info = { attack, weapon };

        ++attack_counts[attack_info];
    }

    // XXX: hack alert
    if (mons_genus(mi.base_type) == MONS_HYDRA)
    {
        ASSERT(attack_counts.size() == 1);
        for (auto &attack_count : attack_counts)
            attack_count.second = mi.num_heads;
    }

    vector<string> attack_descs;
    for (const auto &attack_count : attack_counts)
    {
        const mon_attack_info &info = attack_count.first;
        const mon_attack_def &attack = info.definition;
        const string weapon_note
            = info.weapon ? make_stringf(" plus %s %s",
                                         mi.pronoun(PRONOUN_POSSESSIVE),
                                         info.weapon->name(DESC_PLAIN).c_str())
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
                             mon_attack_name(attack.type).c_str(),
                             flavour_damage(attack.flavour, mi.hd, false)));
            continue;
        }

        // Damage is listed in parentheses for attacks with a flavour
        // description, but not for plain attacks.
        bool has_flavour = !_flavour_base_desc(attack.flavour).empty();
        const string damage_desc =
            make_stringf("%sfor up to %d damage%s%s%s",
                         has_flavour ? "(" : "",
                         attack.damage,
                         attack_count.second > 1 ? " each" : "",
                         weapon_note.c_str(),
                         has_flavour ? ")" : "");

        attack_descs.push_back(
            make_stringf("%s%s%s%s %s%s",
                         _special_flavour_prefix(attack.flavour),
                         mon_attack_name(attack.type).c_str(),
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
        result << ".\n";
    }

    return result.str();
}

static string _monster_spells_description(const monster_info& mi)
{
    static const string panlord_desc =
        "It may possess any of a vast number of diabolical powers.\n";

    // Show a generic message for pan lords, since they're secret.
    if (mi.type == MONS_PANDEMONIUM_LORD && !mi.props.exists(SEEN_SPELLS_KEY))
        return panlord_desc;

    // Show monster spells and spell-like abilities.
    if (!mi.has_spells())
        return "";

    formatted_string description;
    if (mi.type == MONS_PANDEMONIUM_LORD)
        description.cprintf("%s", panlord_desc.c_str());
    describe_spellset(monster_spellset(mi), nullptr, description, &mi);
    description.cprintf("To read a description, press the key listed above.\n");
    return description.tostring();
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
    if (act_speed > 10)
        fast.push_back(what + " " + _speed_description(act_speed));
    if (act_speed < 10)
        slow.push_back(what + " " + _speed_description(act_speed));
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
                       ostringstream &result, int base_value = INT_MAX)
{
    if (base_value == INT_MAX)
        base_value = value;

    result << name << " ";

    const int display_max = value ? value : base_value;
    const bool currently_disabled = !value && base_value;

    if (currently_disabled)
      result << "(";

    for (int i = 0; i * scale < display_max; i++)
    {
        result << "+";
        if (i % 5 == 4)
            result << " ";
    }

    if (currently_disabled)
      result << ")";

#ifdef DEBUG_DIAGNOSTICS
    if (!you.suppress_wizard)
        result << " (" << value << ")";
#endif

    if (currently_disabled)
    {
        result << " (Normal " << name << ")";

#ifdef DEBUG_DIAGNOSTICS
        if (!you.suppress_wizard)
            result << " (" << base_value << ")";
#endif
    }
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
    _print_bar(mi.ac, 5, "AC", result);
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
    _print_bar(mi.ev, 5, "EV", result, mi.base_ev);
    result << "\n";
}

/**
 * Append information about a given monster's MR to the provided stream.
 *
 * @param mi[in]            Player-visible info about the monster in question.
 * @param result[in,out]    The stringstream to append to.
 */
static void _describe_monster_mr(const monster_info& mi, ostringstream &result)
{
    if (mi.res_magic() == MAG_IMMUNE)
    {
        result << "MR ∞\n";
        return;
    }

    const int bar_scale = MR_PIP;
    _print_bar(mi.res_magic(), bar_scale, "MR", result);
    result << "\n";
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
    _describe_monster_mr(mi, result);

    result << "\n";

    resists_t resist = mi.resists();

    const mon_resist_flags resists[] =
    {
        MR_RES_ELEC,    MR_RES_POISON, MR_RES_FIRE,
        MR_RES_STEAM,   MR_RES_COLD,   MR_RES_ACID,
        MR_RES_ROTTING, MR_RES_NEG,    MR_RES_DAMNATION,
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
            if (rflags == MR_RES_DAMNATION)
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

    if (mi.threat != MTHRT_UNDEF)
    {
        result << uppercase_first(pronoun) << " looks "
               << _get_threat_desc(mi.threat) << ".\n";
    }

    if (!resist_descriptions.empty())
    {
        result << uppercase_first(pronoun) << " is "
               << comma_separated_line(resist_descriptions.begin(),
                                       resist_descriptions.end(),
                                       "; and ", "; ")
               << ".\n";
    }

    // Is monster susceptible to anything? (On a new line.)
    if (!suscept.empty())
    {
        result << uppercase_first(pronoun) << " is susceptible to "
               << comma_separated_line(suscept.begin(), suscept.end())
               << ".\n";
    }

    if (mi.is(MB_CHAOTIC))
    {
        result << uppercase_first(pronoun) << " is vulnerable to silver and"
                                              " hated by Zin.\n";
    }

    if (mons_class_flag(mi.type, M_STATIONARY)
        && !mons_is_tentacle_or_tentacle_segment(mi.type))
    {
        result << uppercase_first(pronoun) << " cannot move.\n";
    }

    if (mons_class_flag(mi.type, M_COLD_BLOOD)
        && get_resist(resist, MR_RES_COLD) <= 0)
    {
        result << uppercase_first(pronoun) << " is cold-blooded and may be "
                                              "slowed by cold attacks.\n";
    }

    // Seeing invisible.
    if (mi.can_see_invisible())
        result << uppercase_first(pronoun) << " can see invisible.\n";

    // Echolocation, wolf noses, jellies, etc
    if (!mons_can_be_blinded(mi.type))
        result << uppercase_first(pronoun) << " is immune to blinding.\n";
    // XXX: could mention "immune to dazzling" here, but that's spammy, since
    // it's true of such a huge number of monsters. (undead, statues, plants).
    // Might be better to have some place where players can see holiness &
    // information about holiness.......?

    if (mi.intel() <= I_BRAINLESS)
    {
        // Matters for Ely.
        result << uppercase_first(pronoun) << " is mindless.\n";
    }
    else if (mi.intel() >= I_HUMAN)
    {
        // Matters for Yred, Gozag, Zin, TSO, Alistair....
        result << uppercase_first(pronoun) << " is intelligent.\n";
    }

    // Unusual monster speed.
    const int speed = mi.base_speed();
    bool did_speed = false;
    if (speed != 10 && speed != 0)
    {
        did_speed = true;
        result << uppercase_first(pronoun) << " is " << mi.speed_description();
    }
    const mon_energy_usage def = DEFAULT_ENERGY;
    if (!(mi.menergy == def))
    {
        const mon_energy_usage me = mi.menergy;
        vector<string> fast, slow;
        if (!did_speed)
            result << uppercase_first(pronoun) << " ";
        _add_energy_to_string(speed, me.move, "covers ground", fast, slow);
        // since MOVE_ENERGY also sets me.swim
        if (me.swim != me.move)
            _add_energy_to_string(speed, me.swim, "swims", fast, slow);
        _add_energy_to_string(speed, me.attack, "attacks", fast, slow);
        if (mons_class_itemuse(mi.type) >= MONUSE_STARTING_EQUIPMENT)
            _add_energy_to_string(speed, me.missile, "shoots", fast, slow);
        _add_energy_to_string(
            speed, me.spell,
            mi.is_actual_spellcaster() ? "casts spells" :
            mi.is_priest()             ? "uses invocations"
                                       : "uses natural abilities", fast, slow);
        _add_energy_to_string(speed, me.special, "uses special abilities",
                              fast, slow);
        if (mons_class_itemuse(mi.type) >= MONUSE_STARTING_EQUIPMENT)
            _add_energy_to_string(speed, me.item, "uses items", fast, slow);

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
        result << uppercase_first(pronoun) << " covers ground more"
               << " quickly when invisible.\n";
    }

    if (mi.airborne())
        result << uppercase_first(pronoun) << " can fly.\n";

    // Unusual regeneration rates.
    if (!mi.can_regenerate())
        result << uppercase_first(pronoun) << " cannot regenerate.\n";
    else if (mons_class_fast_regen(mi.type))
        result << uppercase_first(pronoun) << " regenerates quickly.\n";

    // Size
    static const char * const sizes[] =
    {
        "tiny",
        "very small",
        "small",
        nullptr,     // don't display anything for 'medium'
        "large",
        "very large",
        "giant",
    };
    COMPILE_CHECK(ARRAYSZ(sizes) == NUM_SIZE_LEVELS);

    if (sizes[mi.body_size()])
    {
        result << uppercase_first(pronoun) << " is "
        << sizes[mi.body_size()] << ".\n";
    }

    if (in_good_standing(GOD_ZIN, 0))
    {
        const int check = mi.hd - zin_recite_power();
        if (check >= 0)
        {
            result << uppercase_first(pronoun) << " is too strong to be"
                                                  " recited to.";
        }
        else if (check >= -5)
        {
            result << uppercase_first(pronoun) << " may be too strong to be"
                                                  " recited to.";
        }
        else
        {
            result << uppercase_first(pronoun) << " is weak enough to be"
                                                  " recited to.";
        }

        if (you.wizard)
        {
            result << " (Recite power:" << zin_recite_power()
                   << ", Hit dice:" << mi.hd << ")";
        }
        result << "\n";
    }

    result << _monster_attacks_description(mi);
    result << _monster_spells_description(mi);

    return result.str();
}

string serpent_of_hell_flavour(monster_type m)
{
    switch (m)
    {
    case MONS_SERPENT_OF_HELL_COCYTUS:
        return "cocytus";
    case MONS_SERPENT_OF_HELL_DIS:
        return "dis";
    case MONS_SERPENT_OF_HELL_TARTARUS:
        return "tartarus";
    default:
        return "gehenna";
    }
}

// Fetches the monster's database description and reads it into inf.
void get_monster_db_desc(const monster_info& mi, describe_info &inf,
                         bool &has_stat_desc, bool force_seen)
{
    if (inf.title.empty())
        inf.title = getMiscString(mi.common_name(DESC_DBNAME) + " title");
    if (inf.title.empty())
        inf.title = uppercase_first(mi.full_name(DESC_A)) + ".";

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
        inf.body << "The apparition of " << get_ghost_description(mi) << ".\n";
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

    case MONS_PILLAR_OF_SALT:
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
    string result = _monster_stat_description(mi);
    if (!result.empty())
    {
        inf.body << "\n" << result;
        has_stat_desc = true;
    }

    bool stair_use = false;
    if (!mons_class_can_use_stairs(mi.type))
    {
        inf.body << It << " 계단을 오르내릴 수 없다.\n";
        stair_use = true;
    }

    if (mi.is(MB_SUMMONED))
    {
        inf.body << "\n이 몬스터는 소환되었으며, 그래서 "
                    "일시적이다. " << it_o << "을(를) 죽이더라도 경험치를 얻지 못하며, "
                    "음식이나 아이템들도 얻을 수 없을 것이다.";
        if (!stair_use)
            inf.body << ", 또한 " << it << " 계단을 오르내릴 수 없다";
        inf.body << ".\n";
    }
    else if (mi.is(MB_PERM_SUMMON))
    {
        inf.body << "\n이 몬스터는 지속 가능한 방법으로 소환되었다. "
                    "" << it_o << "을(를) 죽이더라도 경험치나 음식, 아이템들을 얻을 수 "
                    "없지만, " << it_o << "은(는) 송환시킬 수 없다.\n";
    }
    else if (mi.is(MB_NO_REWARD))
    {
        inf.body << "\n이 몬스터를 죽이더라도 경험치는 없으며, 음식이나"
                    " 아이템도 없을 것이다.";
    }
    else if (mons_class_leaves_hide(mi.type))
    {
        inf.body << "\n만약 " << it << "이(가) 죽는다면, "
                    "방어구로 사용할 수 있는 " << mi.pronoun(PRONOUN_POSSESSIVE)
                 << "의 짐승 가죽을, 복구하는 것이 가능할 지 모른다.\n";
    }

    if (mi.is(MB_SUMMONED_CAPPED))
    {
        inf.body << "\n이 종류의 몬스터들을 모두 유지하기에는 이 종류의 몬스터들을 "
                    "너무 많이 소환했다. 그래서 이 몬스터에 대한 소환은 곧 "
                    "만료될 것이다.\n";
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
#endif
}

int describe_monsters(const monster_info &mi, bool force_seen,
                      const string &footer)
{
    describe_info inf;
    bool has_stat_desc = false;
    get_monster_db_desc(mi, inf, has_stat_desc, force_seen);

    if (!footer.empty())
    {
        if (inf.footer.empty())
            inf.footer = footer;
        else
            inf.footer += "\n" + footer;
    }

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_monster");
#endif

    spell_scroller fs(monster_spellset(mi), &mi, nullptr);
    fs.add_text(inf.title);
    fs.add_text(inf.body.str(), false, get_number_of_cols() - 1);
    if (crawl_state.game_is_hints())
        fs.add_text(hints_describe_monster(mi, has_stat_desc).c_str());

    formatted_scroller qs;

    if (!inf.quote.empty())
    {
        fs.add_item_formatted_string(
                formatted_string::parse_string("\n" + _toggle_message));

        qs.add_text(inf.title);
        qs.add_text(inf.quote, false, get_number_of_cols() - 1);
        qs.add_item_formatted_string(
                formatted_string::parse_string("\n" + _toggle_message));
    }

    fs.add_item_formatted_string(formatted_string::parse_string(inf.footer));

    bool show_quote = false;
    while (true)
    {
        if (show_quote)
            qs.show();
        else
            fs.show();

        int keyin = (show_quote ? qs : fs).get_lastch();
        // this is never actually displayed to the player
        // we just use it to check whether we should toggle.
        if (_print_toggle_message(inf, keyin))
            show_quote = !show_quote;
        else
            return keyin;
    }
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
                               species_name(ghost.species).c_str(),
                               get_job_name(ghost.job));

    if (abbrev || strwidth(desc) > 40)
    {
        desc = make_stringf("%s %s%s",
                            rank,
                            get_species_abbrev(ghost.species),
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
                        species_has_low_str(gspecies), mi.i_ghost.religion)
         << ", " << _xl_rank_name(mi.i_ghost.xl_rank) << " ";

    if (concise)
    {
        gstr << get_species_abbrev(gspecies)
             << get_job_abbrev(mi.i_ghost.job);
    }
    else
    {
        gstr << species_name(gspecies)
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
    ostringstream data;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_skill");
#endif

    data << get_skill_description(skill, true);

    print_description(data.str());
    getchm();
}

// only used in tiles
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

void alt_desc_proc::nextline()
{
    ostr << "\n";
}

void alt_desc_proc::print(const string &str)
{
    ostr << str;
}

int alt_desc_proc::count_newlines(const string &str)
{
    return count(begin(str), end(str), '\n');
}

void alt_desc_proc::trim(string &str)
{
    int idx = str.size();
    while (--idx >= 0)
    {
        if (str[idx] != '\n')
            break;
    }
    str.resize(idx + 1);
}

bool alt_desc_proc::chop(string &str)
{
    int loc = -1;
    for (size_t i = 1; i < str.size(); i++)
        if (str[i] == '\n' && str[i-1] == '\n')
            loc = i;

    if (loc == -1)
        return false;

    str.resize(loc);
    return true;
}

void alt_desc_proc::get_string(string &str)
{
    str = replace_all(ostr.str(), "\n\n\n\n", "\n\n");
    str = replace_all(str, "\n\n\n", "\n\n");

    trim(str);
    while (count_newlines(str) > h)
    {
        if (!chop(str))
            break;
    }
}

/**
 * Provide auto-generated information about the given cloud type. Describe
 * opacity & related factors.
 *
 * @param cloud_type        The cloud_type in question.
 * @return e.g. "\nThis cloud is opaque; one tile will not block vision, but
 *      multiple will. \nClouds of this kind the player makes will vanish very
 *      quickly once outside the player's sight."
 */
string extra_cloud_info(cloud_type cloud_type)
{
    const bool opaque = is_opaque_cloud(cloud_type);
    const string opacity_info = !opaque ? "" :
        "\n이 구름은 불투명하다; 타일 하나의 구름은 시야를 가리지 않으나, "
        "다수의 구름은 시야를 가릴 것이다.";
    const string vanish_info
        = make_stringf("\n모험가가 만드는 이 종류의 구름은 그들의 시야 밖에서 "
                       "%s 사라질 것이다.",
                       opaque ? "빠르게" : "거의 즉시");
    return opacity_info + vanish_info;
}
