/**
 * @file
 * @brief Functions used to print information about spells, spellbooks, etc.
 **/

#include "AppHdr.h"

#include "describe-spells.h"

#include "colour.h"
#include "delay.h"
#include "describe.h"
#include "english.h"
#include "externs.h"
#include "god-abil.h"
#include "invent.h"
#include "libutil.h"
#include "menu.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-explode.h" // ball_lightning_damage
#include "mon-project.h" // iood_damage
#include "religion.h"
#include "shopping.h"
#include "spl-book.h"
#include "spl-damage.h"
#include "spl-summoning.h" // mons_ball_lightning_hd
#include "spl-util.h"
#include "spl-zap.h"
#include "stringutil.h"
#include "state.h"
#include "tag-version.h"
#include "tileweb.h"
#include "unicode.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif

static string _effect_string(spell_type spell, const monster_info *mon_owner);

/**
 * Returns a spellset containing the player-known spells for the given item.
 *
 * @param item      The item in question.
 * @return          A single-element vector, containing the list of all
 *                  non-null spells in the given book, blank-labeled.
 */
spellset item_spellset(const item_def &item)
{
    if (!item.has_spells())
        return {};

    return { { "\n", spells_in_book(item) } };
}

/**
 * What's the appropriate descriptor for a given type of "spell" that's not
 * really a spell?
 *
 * @param type              The type of spell-ability; e.g. MON_SPELL_MAGICAL.
 * @return                  A descriptor of the spell type; e.g. "divine",
 *                          "magical", etc.
 */
static string _ability_type_descriptor(mon_spell_slot_flag type)
{
    static const map<mon_spell_slot_flag, string> descriptors =
    {
        { MON_SPELL_NATURAL, "natural" },
        { MON_SPELL_MAGICAL, "magical" },
        { MON_SPELL_PRIEST,  "divine"  },
        { MON_SPELL_VOCAL,   "natural" },
    };

    return lookup(descriptors, type, "buggy");
}

static const char* _abil_type_vuln_core(bool silencable, bool antimagicable)
{
    // No one gets confused by the rare spells that are hit by silence
    // but not antimagic, AFAIK. Let's keep it simple.
    if (!antimagicable)
        return "silence";
    if (silencable)
        return "silence and antimagic";
    // Explicitly clarify about spells that are hit by antimagic but
    // NOT silence, since those confuse players nonstop.
    return "antimagic (but not silence)";
}

/**
 * What type of effects is this spell type vulnerable to?
 *
 * @param type              The type of spell-ability; e.g. MON_SPELL_MAGICAL.
 * @return                  A description of the spell's vulnerabilities.
 */
static string _ability_type_vulnerabilities(mon_spell_slot_flag type)
{
    if (type == MON_SPELL_NATURAL)
        return "";
    const bool silencable = type == MON_SPELL_WIZARD
                            || type == MON_SPELL_PRIEST
                            || type == MON_SPELL_VOCAL;
    const bool antimagicable = type == MON_SPELL_WIZARD
                               || type == MON_SPELL_MAGICAL;
    ASSERT(silencable || antimagicable);
    return make_stringf(", which are affected by %s",
                        _abil_type_vuln_core(silencable, antimagicable));
}

/**
 * What description should a given (set of) monster spellbooks be prefixed
 * with?
 *
 * @param type              The type of book(s); e.g. MON_SPELL_MAGICAL.
 * @return                  A header string for the bookset; e.g.,
 *                          "has mastered one of the following spellbooks:"
 *                          "possesses the following natural abilities:"
 */
static string _booktype_header(mon_spell_slot_flag type, bool pronoun_plural)
{
    if (type == MON_SPELL_EVOKE)
    {
        return make_stringf("%s the following wand spells:",
                            conjugate_verb("possess", pronoun_plural).c_str());
    }

    const string vulnerabilities = _ability_type_vulnerabilities(type);

    if (type == MON_SPELL_WIZARD)
    {
        return make_stringf("%s mastered %s%s:",
                            conjugate_verb("have", pronoun_plural).c_str(),
                            "the following spells",
                            vulnerabilities.c_str());
    }

    const string descriptor = _ability_type_descriptor(type);

    return make_stringf("%s the following %s abilities%s:",
                        conjugate_verb("possess", pronoun_plural).c_str(),
                        descriptor.c_str(),
                        vulnerabilities.c_str());
}

/**
 * Append all spells of a given type that a given monster may know to the
 * provided vector.
 *
 * @param mi                The player's knowledge of a monster.
 * @param type              The type of spells to select.
 *                          (E.g. MON_SPELL_MAGICAL, MON_SPELL_WIZARD...)
 * @param[out] all_books    An output vector of "spellbooks".
 */
static void _monster_spellbooks(const monster_info &mi,
                                mon_spell_slot_flag type,
                                spellset &all_books)
{
    vector<mon_spell_slot> book_slots = get_unique_spells(mi, type);

    if (book_slots.empty())
        return;

    spellbook_contents output_book;

    output_book.label += make_stringf("\n%s %s",
        uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE)).c_str(),
        _booktype_header(type, mi.pronoun_plurality()).c_str());

    // Does the monster have a spell that allows them to cast Abjuration?
    bool mons_abjure = false;

    for (const auto& slot : book_slots)
    {
        const spell_type spell = slot.spell;
        if (!spell_is_soh_breath(spell))
        {
            output_book.spells.emplace_back(spell);
            if (get_spell_flags(spell) & spflag::mons_abjure)
                mons_abjure = true;
            continue;
        }

        const vector<spell_type> *breaths = soh_breath_spells(spell);
        ASSERT(breaths);
        for (auto breath : *breaths)
            output_book.spells.emplace_back(breath);
    }

    if (mons_abjure)
        output_book.spells.emplace_back(SPELL_ABJURATION);

    all_books.emplace_back(output_book);
}

static void _monster_wand_spellbook(const monster_info &mi,
                                spellset &all_books)
{
    if (mi.itemuse() < MONUSE_STARTING_EQUIPMENT)
        return;

    const item_def* wand = mi.inv[MSLOT_WAND].get();
    if (!wand)
        return;

    spellbook_contents book;

    book.label += make_stringf("\n%s %s",
        uppercase_first(mi.pronoun(PRONOUN_SUBJECTIVE)).c_str(),
        _booktype_header(MON_SPELL_EVOKE, mi.pronoun_plurality()).c_str());

    const wand_type wandtyp = static_cast<wand_type>(wand->sub_type);
    ASSERT(wandtyp < NUM_WANDS);
    book.spells.emplace_back(spell_in_wand(wandtyp));

    all_books.emplace_back(book);
}

/**
 * Return a spellset containing the spells potentially given by the given
 * monster information.
 *
 * @param mi    The player's knowledge of a monster.
 * @return      The spells potentially castable by that monster (as far as
 *              the player knows).
 */
spellset monster_spellset(const monster_info &mi)
{
    if (!mi.has_spells())
        return {};

    static const mon_spell_slot_flag book_flags[] =
    {
        MON_SPELL_NATURAL,
        MON_SPELL_VOCAL,
        MON_SPELL_MAGICAL,
        MON_SPELL_PRIEST,
        MON_SPELL_WIZARD,
    };

    spellset books;

    for (auto book_flag : book_flags)
        _monster_spellbooks(mi, book_flag, books);

    _monster_wand_spellbook(mi, books);

    ASSERT(books.size());
    return books;
}


/**
 * Build a flat vector containing all unique spells in a given spellset.
 *
 * @param spellset      The spells in question.
 * @return              An ordered set of unique spells in the given set.
 *                      Guaranteed to be in the same order as in the spellset.
 */
static vector<spell_type> _spellset_contents(const spellset &spells)
{
    // find unique spells (O(nlogn))
    set<spell_type> unique_spells;
    for (auto &book : spells)
        for (auto spell : book.spells)
            unique_spells.insert(spell);

    // list spells in original order (O(nlogn)?)
    vector<spell_type> spell_list;
    for (auto &book : spells)
    {
        for (auto spell : book.spells)
        {
            if (unique_spells.erase(spell) == 1)
                spell_list.emplace_back(spell);
        }
    }

    return spell_list;
}

/**
 * What spell should a given colour be listed with?
 *
 * @param spell         The spell in question.
 * @param source_item   The physical item holding the spells. May be null.
 */
static int _spell_colour(spell_type spell, const item_def* const source_item)
{
    if (!crawl_state.need_save)
        return COL_UNKNOWN;

    if (!source_item)
        return spell_highlight_by_utility(spell, COL_UNKNOWN);

    if (you.has_spell(spell))
        return COL_MEMORIZED;

    // this is kind of ugly.
    if (!you_can_memorise(spell)
        || you.experience_level < spell_difficulty(spell)
        || player_spell_levels() < spell_levels_required(spell))
    {
        return COL_USELESS;
    }

    if (god_hates_spell(spell, you.religion))
        return COL_FORBIDDEN;

    if (!you.has_spell(spell))
        return COL_UNMEMORIZED;

    return spell_highlight_by_utility(spell, COL_UNKNOWN);
}

/**
 * List the name(s) of the school(s) the given spell is in.
 *
 * XXX: This is almost certainly duplicating something somewhere. Also, it's
 * pretty ugly.
 *
 * @param spell     The spell in question.
 * @return          A '/'-separated list of spellschool names.
 */
static string _spell_schools(spell_type spell)
{
    string schools;

    for (const auto school_flag : spschools_type::range())
    {
        if (!spell_typematch(spell, school_flag))
            continue;

        if (!schools.empty())
            schools += "/";
        schools += spelltype_long_name(school_flag);
    }

    return schools;
}

/**
 * Produce a mapping from characters (used as indices) to spell types in
 * the given spellset.
 *
 * @param spells        A list of 'books' of spells.
 * @param source_item   The source of the spells; a book, or nullptr in the
 *                      case of monster spellbooks.
 * @return              A list of all unique spells in the given set, ordered
 *                      either in original order or column-major order, the
 *                      latter in the case of a double-column layout.
 */
vector<pair<spell_type,char>> map_chars_to_spells(const spellset &spells)
{
    char next_ch = 'a';
    const vector<spell_type> flat_spells = _spellset_contents(spells);
    vector<pair<spell_type,char>> ret;
    for (auto spell : flat_spells)
        ret.emplace_back(pair<spell_type,char>(spell, next_ch++));
    return ret;
}

static string _range_string(const spell_type &spell, const monster_info *mon_owner, int hd)
{
    auto flags = get_spell_flags(spell);
    int pow = mons_power_for_hd(spell, hd);
    int range = spell_range(spell, pow, mon_owner && mon_owner->is(MB_PLAYER_SERVITOR));
    const bool has_range = mon_owner
                        && range > 0
                        && !testbits(flags, spflag::selfench);
    if (!has_range)
        return "";

    int minrange = 0;
    if (spell == SPELL_CALL_DOWN_LIGHTNING || spell == SPELL_FLASHING_BALESTRA)
        minrange = 2;

    const bool in_range = has_range
                    && crawl_state.need_save
                    && in_bounds(mon_owner->pos)
                    && grid_distance(you.pos(), mon_owner->pos) <= range
                    && grid_distance(you.pos(), mon_owner->pos) >= minrange;
    const char *range_col = in_range ? "lightred" : "lightgray";
    return make_stringf("(<%s>%d%s</%s>)", range_col, range,
                                           minrange > 0 ? "*" : "",
                                           range_col);
}

// TODO: deduplicate with the same-named function in spl-cast.cc
static dice_def _spell_damage(spell_type spell, int hd)
{
    const int pow = mons_power_for_hd(spell, hd);

    switch (spell)
    {
        case SPELL_FREEZE:
            return freeze_damage(pow, false);
        case SPELL_SCORCH:
            return scorch_damage(pow, false);
        case SPELL_WATERSTRIKE:
            return waterstrike_damage(hd);
        case SPELL_IOOD:
            return iood_damage(pow, INFINITE_DISTANCE, false);
        case SPELL_IRRADIATE:
            return irradiate_damage(pow, false);
        case SPELL_GLACIATE:
            return glaciate_damage(pow, 3);
        case SPELL_CONJURE_BALL_LIGHTNING:
            return ball_lightning_damage(mons_ball_lightning_hd(pow, false));
        case SPELL_ERUPTION:
            return eruption_damage();
        case SPELL_LRD:
            return base_fragmentation_damage(pow, false);
        case SPELL_AIRSTRIKE:
            return base_airstrike_damage(pow);
        case SPELL_ARCJOLT:
            return arcjolt_damage(pow, false);
        case SPELL_RESONANCE_STRIKE:
            return resonance_strike_base_damage(hd);
        case SPELL_POLAR_VORTEX:
            return polar_vortex_dice(pow, false);
        case SPELL_ELECTROLUNGE:
            return electrolunge_damage(pow);
        case SPELL_FULMINANT_PRISM:
        case SPELL_SHADOW_PRISM:
            return prism_damage(prism_hd(pow, false), true);
        case SPELL_HELLFIRE_MORTAR:
            return hellfire_mortar_damage(pow);

        // This is the per-turn *sticky flame* damage against the player.
        // The spell has no impact damage and otherwise uses different numbers
        // against monsters. This is very unsatisfying, but surely we show the
        // player *something*...
        case SPELL_PYRE_ARROW:
            return dice_def(2, 2 + hd * 12 / 14);
        default:
            break;
    }

    const zap_type zap = spell_to_zap(spell);
    if (zap == NUM_ZAPS)
        return dice_def(0,0);

    return zap_damage(zap, pow, true, false);
}

static int _spell_hd(spell_type spell, const monster_info &mon_owner)
{
    if (spell == SPELL_SEARING_BREATH && mon_owner.type == MONS_XTAHUA)
        return mon_owner.hd * 3 / 2;
    if (spell == SPELL_COLD_BREATH && mons_is_draconian(mon_owner.type))
        return mon_owner.hd * 5 / 6;
    if (mons_spell_is_spell(spell))
        return mon_owner.spell_hd();
    return mon_owner.hd;
}

static colour_t _spell_colour(spell_type spell)
{
    switch (spell)
    {
        case SPELL_FREEZE:
        case SPELL_GLACIATE:
            return WHITE;
        case SPELL_WATERSTRIKE:
            return LIGHTBLUE;
        case SPELL_IOOD:
            return LIGHTMAGENTA;
        case SPELL_SCORCH:
        case SPELL_ERUPTION:
            return RED;
        case SPELL_ELECTROLUNGE:
            return LIGHTCYAN;
        default:
            break;
    }
    const zap_type zap = spell_to_zap(spell);
    if (zap == NUM_ZAPS)
        return COL_UNKNOWN;
    return zap_colour(zap);
}

static string _describe_living_spells(const monster_info &mon_owner)
{
    const spell_type spell = living_spell_type_for(mon_owner.type);
    const int n = living_spell_count(spell, false);
    const string base_desc = _effect_string(spell, &mon_owner);
    const string desc = base_desc[0] == '(' ? base_desc : make_stringf("(%s)",
            base_desc.c_str());
    return make_stringf("%dx%s", n, desc.c_str());
}


static string _effect_string(spell_type spell, const monster_info *mon_owner)
{
    if (!mon_owner)
        return "";

    if (spell == SPELL_CONJURE_LIVING_SPELLS)
        return _describe_living_spells(*mon_owner);

    const int hd = _spell_hd(spell, *mon_owner);
    if (!hd)
        return "";

    if (testbits(get_spell_flags(spell), spflag::WL_check))
    {
        // WL chances only make sense vs a player
        if (!crawl_state.need_save
#ifndef DEBUG_DIAGNOSTICS
            || mon_owner->attitude == ATT_FRIENDLY
#endif
            )
        {
            return "";
        }
        if (you.immune_to_hex(spell))
            return "(immune)";

        const string hex_str = make_stringf("%d%%", hex_chance(spell, mon_owner));

        const dice_def dam = _spell_damage(spell, hd);
        if (!dam.size || !dam.num)
            return make_stringf("(%s)", hex_str.c_str());
        return make_stringf("(%s,%dd%d)", hex_str.c_str(), dam.num, dam.size);
    }

    if (spell == SPELL_SMITING)
        return "7-17"; // sigh

    if (spell == SPELL_BRAIN_BITE)
        return "4-8*"; // >_>

    if (spell == SPELL_DRAINING_GAZE)
    {
        const int pow = mons_power_for_hd(SPELL_DRAINING_GAZE, hd);
        return make_stringf("0-%d MP", pow / 8); // >_> >_>
    }

    if (spell == SPELL_WIND_BLAST)
    {
        const int pow = mons_power_for_hd(SPELL_WIND_BLAST, hd);
        return make_stringf("2d%d*", default_collision_damage(pow, false).size);
    }

    if (spell == SPELL_FORCE_LANCE)
    {
        const int pow = mons_power_for_hd(SPELL_FORCE_LANCE, hd);
            return make_stringf("%dd%d(+%dd%d)",
                _spell_damage(spell, hd).num,
                _spell_damage(spell, hd).size,
                default_collision_damage(pow, false).num,
                default_collision_damage(pow, false).size);
    }

    if (spell == SPELL_HOARFROST_BULLET)
    {
        const int pow = mons_power_for_hd(spell, hd);
        return make_stringf("3d(%d/%d)",
            zap_damage(ZAP_HOARFROST_BULLET, pow, true, false).size,
            zap_damage(ZAP_HOARFROST_BULLET_FINALE, pow, true, false).size);
    }

    const dice_def dam = _spell_damage(spell, hd);
    if (dam.num == 0 || dam.size == 0)
        return "";

    if (spell == SPELL_AIRSTRIKE)
        return describe_airstrike_dam(dam);
    if (spell == SPELL_RESONANCE_STRIKE)
        return describe_resonance_strike_dam(dam);

    if (spell == SPELL_BOLT_OF_DRAINING && mon_owner->type == MONS_LAUGHING_SKULL)
    {
        return make_stringf("%dd(%d-%d)", dam.num, dam.size,
                                          dam.size * 2);
    }

    string mult = "";
    if (spell == SPELL_MARSHLIGHT
        || spell == SPELL_FOXFIRE
        || spell == SPELL_PLASMA_BEAM
        || spell == SPELL_PERMAFROST_ERUPTION)
    {
        mult = "2x";
    }
    else if (spell == SPELL_CONJURE_BALL_LIGHTNING)
        mult = "3x";
    else if (spell == SPELL_ELECTROLUNGE)
        mult = "+";
    const char* suffix = spell == SPELL_LRD ? "*"
                       : spell == SPELL_PYRE_ARROW ? "/turn"
                       : "";
    return make_stringf("(%s%dd%d%s)", mult.c_str(), dam.num, dam.size, suffix);
}

/**
 * Describe a given set of spells.
 *
 * @param book              A labeled set of spells, corresponding to a book
 *                          or monster spellbook.
 * @param spell_map         The letters to use for each spell.
 * @param source_item       The physical item holding the spells. May be null.
 * @param description[out]  An output string to append to.
 * @param mon_owner         If this spellset is being examined from a monster's
 *                          description, 'mon_owner' is that monster. Else,
 *                          it's null.
 */
static void _describe_book(const spellbook_contents &book,
                           vector<pair<spell_type,char>> &spell_map,
                           const item_def* const source_item,
                           formatted_string &description,
                           const monster_info *mon_owner)
{
    description.textcolour(LIGHTGREY);

    description.cprintf("%s", book.label.c_str());

    // only display header for book spells
    if (source_item)
    {
        description.cprintf(
            "\n Spells                              Type                    Level");
        if (crawl_state.need_save)
            description.cprintf("       Known");
    }
    description.cprintf("\n");

    // list spells in two columns, instead of one? (monster books)
    const bool doublecolumn = source_item == nullptr;

    bool first_line_element = true;
    const int hd = mon_owner ? mon_owner->spell_hd() : 0;
    for (auto spell : book.spells)
    {
        description.cprintf(" ");

        if (!mon_owner)
            description.textcolour(_spell_colour(spell, source_item));

        // don't crash if we have more spells than letters.
        auto entry = find_if(spell_map.begin(), spell_map.end(),
                [&spell](const pair<spell_type,char>& e)
                {
                    return e.first == spell;
                });
        const char spell_letter = entry != spell_map.end()
                                            ? entry->second : ' ';

        const string range_str = _range_string(spell, mon_owner, hd);
        string effect_str = _effect_string(spell, mon_owner);

        const string dith_marker = mon_owner
                                   && crawl_state.need_save
                                   && you_worship(GOD_DITHMENOS)
                                        ? !valid_marionette_spell(spell)
                                          ? "<magenta>!</magenta>"
                                          : spell_has_marionette_override(spell)
                                          ? "<lightmagenta>*</lightmagenta>" : ""
                                        : "";

        const int effect_len = effect_str.length();
        const int range_len = range_str.empty() ? 0 : 3;
        const int effect_range_space = effect_len && range_len ? 1 : 0;
        const int chop_len = 32 - effect_len - range_len - effect_range_space
                                - (dith_marker.length() > 0 ? 1 : 0);

        if (effect_len && !testbits(get_spell_flags(spell), spflag::WL_check))
            effect_str = colourize_str(effect_str, _spell_colour(spell));

        string spell_name = spell_title(spell);
        if (spell == SPELL_LEHUDIBS_CRYSTAL_SPEAR
            && chop_len < (int)spell_name.length())
        {
            // looks nicer than Lehudib's Crystal S
            spell_name = "Crystal Spear";
        }
        description += formatted_string::parse_string(
                make_stringf("%c - %s%s%s%s%s", spell_letter,
                             dith_marker.c_str(),
                             chop_string(spell_name, chop_len).c_str(),
                             effect_str.c_str(),
                             effect_range_space ? " " : "",
                             range_str.c_str()));

        // only display type & level for book spells
        if (doublecolumn)
        {
            // print monster spells in two columns
            if (first_line_element)
                description.cprintf("    ");
            else
                description.cprintf("\n");
            first_line_element = !first_line_element;
            continue;
        }

        string schools =
#if TAG_MAJOR_VERSION == 34
            source_item->base_type == OBJ_RODS ? "Evocations"
                                               :
#endif
                         _spell_schools(spell);

        string known = "";
        if (!mon_owner && crawl_state.need_save)
            known = you.spell_library[spell] ? "         yes" : "          no";

        description.cprintf("%s%d%s\n",
                            chop_string(schools, 28).c_str(),
                            spell_difficulty(spell),
                            known.c_str());
    }

    // are we halfway through a column?
    if (doublecolumn && book.spells.size() % 2)
        description.cprintf("\n");
}


/**
 * List a given set of spells.
 *
 * @param spells            The set of spells to be listed.
 * @param source_item       The physical item holding the spells. May be null.
 * @param description[out]  An output string to append to.
 * @param mon_owner         If this spellset is being examined from a monster's
 *                          description, 'mon_owner' is that monster. Else,
 *                          it's null.
 */
void describe_spellset(const spellset &spells,
                       const item_def* const source_item,
                       formatted_string &description,
                       const monster_info *mon_owner)
{
    auto spell_map = map_chars_to_spells(spells);
    for (auto book : spells)
        _describe_book(book, spell_map, source_item, description, mon_owner);
}

#ifdef USE_TILE_WEB
static void _write_book(const spellbook_contents &book,
                           vector<pair<spell_type,char>> &spell_map,
                           const item_def* const source_item,
                           const monster_info *mon_owner)
{
    tiles.json_open_object();
    tiles.json_write_string("label", book.label);
    const int hd = mon_owner ? mon_owner->spell_hd() : 0;
    tiles.json_open_array("spells");
    for (auto spell : book.spells)
    {
        tiles.json_open_object();

        const string dith_marker = mon_owner
                                   && crawl_state.need_save
                                   && you_worship(GOD_DITHMENOS)
                                        ? !valid_marionette_spell(spell)
                                          ? "<magenta>!</magenta>"
                                          : spell_has_marionette_override(spell)
                                          ? "<lightmagenta>*</lightmagenta>" : ""
                                        : "";

        tiles.json_write_string("title", dith_marker + spell_title(spell));
        tiles.json_write_int("colour", _spell_colour(spell, source_item));
        tiles.json_write_name("tile");
        tiles.write_tileidx(tileidx_spell(spell));

        // don't crash if we have more spells than letters.
        auto entry = find_if(spell_map.begin(), spell_map.end(),
                [&spell](const pair<spell_type,char>& e) { return e.first == spell; });
        const char spell_letter = entry != spell_map.end() ? entry->second : ' ';
        tiles.json_write_string("letter", string(1, spell_letter));

        string effect_str = _effect_string(spell, mon_owner);
        if (!testbits(get_spell_flags(spell), spflag::WL_check))
            effect_str = colourize_str(effect_str, _spell_colour(spell));
        tiles.json_write_string("effect", effect_str);

        string range_str = _range_string(spell, mon_owner, hd);
        if (range_str.size() > 0)
            tiles.json_write_string("range_string", range_str);

#if TAG_MAJOR_VERSION == 34
        string schools = (source_item && source_item->base_type == OBJ_RODS) ?
                "Evocations" : _spell_schools(spell);
#else
        string schools = _spell_schools(spell);
#endif
        tiles.json_write_string("schools", schools);
        tiles.json_write_int("level", spell_difficulty(spell));
        tiles.json_close_object();
    }
    tiles.json_close_array();
    tiles.json_close_object();
}

void write_spellset(const spellset &spells,
                       const item_def* const source_item,
                       const monster_info *mon_owner)
{
    auto spell_map = map_chars_to_spells(spells);
    tiles.json_open_array("spellset");
    for (auto book : spells)
        _write_book(book, spell_map, source_item, mon_owner);
    tiles.json_close_array();
}
#endif

/**
 * Return a description of the spells in the given item.
 *
 * @param item      The book in question.
 * @return          A column-and-row listing of the spells in the given item,
 *                  including names, schools & levels.
 */
string describe_item_spells(const item_def &item)
{
    formatted_string description;
    describe_spellset(item_spellset(item), &item, description);
    return description.tostring();
}

/**
 * Return a one-line description of the spells in the given item.
 *
 * @param item      The book in question.
 * @return          A one-line listing of the spells in the given item,
 *                  including names, schools & levels.
 */
string terse_spell_list(const item_def &item)
{
    vector<string> spell_descs;
    for (auto spell : spells_in_book(item))
    {
        spell_descs.push_back(make_stringf("%s (L%d %s)",
                                           spell_title(spell),
                                           spell_difficulty(spell),
                                           _spell_schools(spell).c_str()));
    }
    // could use comma_separated_fn and skip the intervening vec?
    return "Spells: " + comma_separated_line(spell_descs.begin(),
                                             spell_descs.end());
}
