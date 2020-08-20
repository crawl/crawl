#include "AppHdr.h"

#include "god-prayer.h"

#include <cmath>

#include "areas.h"
#include "artefact.h"
#include "butcher.h"
#include "coordit.h"
#include "database.h"
#include "decks.h"
#include "describe-god.h"
#include "english.h"
#include "env.h"
#include "food.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-item.h"
#include "god-passive.h"
#include "hiscores.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "makeitem.h"
#include "message.h"
#include "notes.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "spl-goditem.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#include "unwind.h"
#include "view.h"

string god_prayer_reaction()
{
    string result = uppercase_first(god_name(you.religion));
    const int rank = god_favour_rank(you.religion);
    if (crawl_state.player_is_dead())
        result += " was ";
    else
        result += " is ";
    result +=
        (rank == 7) ? "exalted by your worship" :
        (rank == 6) ? "extremely pleased with you" :
        (rank == 5) ? "greatly pleased with you" :
        (rank == 4) ? "most pleased with you" :
        (rank == 3) ? "pleased with you" :
        (rank == 2) ? "aware of your devotion"
                    : "noncommittal";
    result += ".";

    return result;
}

/**
 * Determine the god this game's ecumenical altar is for.
 * Replaces the ecumenical altar with the God's real altar.
 * Assumes you can worship at least one god (ie are not a
 * demigod), and that you're standing on the altar.
 *
 * @return The god this altar is for.
 */
static god_type _altar_identify_ecumenical_altar()
{
    god_type god;
    do
    {
        god = random_god();
    }
    while (!player_can_join_god(god));
    dungeon_terrain_changed(you.pos(), altar_for_god(god));
    return god;
}

static bool _pray_ecumenical_altar()
{
    if (yesno("You cannot tell which god this altar belongs to. Convert to "
              "them anyway?", false, 'n'))
    {
        {
            // Don't check for or charge a Gozag service fee.
            unwind_var<int> fakepoor(you.attribute[ATTR_GOLD_GENERATED], 0);

            god_type altar_god = _altar_identify_ecumenical_altar();
            mprf(MSGCH_GOD, "%s accepts your prayer!",
                            god_name(altar_god).c_str());
            you.turn_is_over = true;
            if (!you_worship(altar_god))
                join_religion(altar_god);
            else
                return true;
        }

        if (you_worship(GOD_RU))
            you.props[RU_SACRIFICE_PROGRESS_KEY] = 9999;
        else
            gain_piety(20, 1, false);

        mark_milestone("god.ecumenical", "prayed at an ecumenical altar.");
        return true;
    }
    else
    {
        canned_msg(MSG_OK);
        return false;
    }
}


//Elyvilon

// Is the destroyed weapon valuable enough to gain piety by doing so?
// Unholy and evil weapons are handled specially.
static bool _destroyed_valuable_weapon(int value, int type)
{
    // value/500 chance of piety normally
    if (value > random2(500))
        return true;

    // But all non-missiles are acceptable if you've never reached *.
    if (you.piety_max[GOD_ELYVILON] < piety_breakpoint(0)
        && type != OBJ_MISSILES)
    {
        return true;
    }

    return false;
}

/**
 * Attempt to convert to the given god.
 *
 * @return True if the conversion happened, false otherwise.
 */
void try_god_conversion(god_type god)
{
    ASSERT(god != GOD_NO_GOD);

    if (you.species == SP_DEMIGOD)
    {
        mpr("A being of your status worships no god.");
        return;
    }

    if (you.species == SP_ANGEL)
    {
        if (god != you.religion && 
            !(god == GOD_ELYVILON || 
                god == GOD_ZIN ||
                god == GOD_SHINING_ONE)) {
            mpr("You cannot abandon your faith. You can only convert to good god.");

            return;
        }
        else {
            mprf(MSGCH_GOD, "You offer a %sprayer to %s.",
                you.cannot_speak() ? "silent " : "",
                god_name(god).c_str());
        }
    }

    if (god == GOD_ECUMENICAL)
    {
        _pray_ecumenical_altar();
        return;
    }

    if (you_worship(GOD_NO_GOD) || god != you.religion)
    {
        // consider conversion
        you.turn_is_over = true;
        // But if we don't convert then god_pitch
        // makes it not take a turn after all.
        god_pitch(god);
    }
    else
    {
        // Already worshipping this god - just print a message.
        mprf(MSGCH_GOD, "You offer a %sprayer to %s.",
             you.cannot_speak() ? "silent " : "",
             god_name(god).c_str());
    }
}

int zin_tithe(const item_def& item, int quant, bool converting)
{
    if (item.tithe_state == TS_NO_TITHE)
        return 0;

    int taken = 0;
    int due = quant += you.attribute[ATTR_TITHE_BASE];
    if (due > 0)
    {
        int tithe = due / 10;
        due -= tithe * 10;
        // Those high enough in the hierarchy get to reap the benefits.
        // You're never big enough to be paid, the top is not having to pay
        // (and even that at 200 piety, for a brief moment until it decays).
        tithe = min(tithe,
                    (you.penance[GOD_ZIN] + MAX_PIETY - you.piety) * 2 / 3);
        if (tithe <= 0)
        {
            // update the remainder anyway
            you.attribute[ATTR_TITHE_BASE] = due;
            return 0;
        }
        taken = tithe;
        you.attribute[ATTR_DONATIONS] += tithe;
        mprf("You pay a tithe of %d gold.", tithe);

        if (item.tithe_state == TS_NO_PIETY) // seen before worshipping Zin
        {
            tithe = 0;
            simple_god_message(" ignores your late donation.");
        }
        // A single scroll can give you more than D:1-18, Lair and Orc
        // together, limit the gains. You're still required to pay from
        // your sudden fortune, yet it's considered your duty to the Church
        // so piety is capped. If you want more piety, donate more!
        //
        // Note that the stepdown is not applied to other gains: it would
        // be simpler, yet when a monster combines a number of gold piles
        // you shouldn't be penalized.
        int denom = 2;
        if (item.props.exists(ACQUIRE_KEY)) // including "acquire any" in vaults
        {
            tithe = stepdown_value(tithe, 10, 10, 50, 50);
            dprf("Gold was acquired, reducing gains to %d.", tithe);
        }
        else
        {
            if (player_in_branch(BRANCH_ORC) && !converting)
            {
                // Another special case: Orc gives simply too much compared to
                // other branches.
                denom *= 2;
            }
            // Avg gold pile value: 10 + depth/2.
            tithe *= 47;
            denom *= 20 + env.absdepth0;
        }
        gain_piety(tithe * 3, denom);
    }
    you.attribute[ATTR_TITHE_BASE] = due;
    return taken;
}

enum class jiyva_slurp_result
{
    none = 0,
    food = 1 << 0,
    hp   = 1 << 1,
    mp   = 1 << 2,
};
DEF_BITFIELD(jiyva_slurp_results, jiyva_slurp_result);

struct slurp_gain
{
    jiyva_slurp_results jiyva_bonus;
    piety_gain_t piety_gain;
};

// God effects of sacrificing one item from a stack (e.g., a weapon, one
// out of 20 arrows, etc.). Does not modify the actual item in any way.
static slurp_gain _sacrifice_one_item_noncount(const item_def& item)
{
    // item_value() multiplies by quantity.
    const int shop_value = item_value(item, true) / item.quantity;
    // Since the god is taking the items as a sacrifice, they must have at
    // least minimal value, otherwise they wouldn't be taken.
    const int value = (item.base_type == OBJ_CORPSES ?
                          50 * stepdown_value(max(1,
                          max_corpse_chunks(item.mon_type)), 4, 4, 12, 12) :
                      (is_worthless_consumable(item) ? 1 : shop_value));

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE)
        mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d", value);
#endif

    slurp_gain gain { jiyva_slurp_result::none, PIETY_NONE };

    //piety_gain_t relative_piety_gain = PIETY_NONE;
    switch (you.religion)
    {
    case GOD_NEMELEX_XOBEH:
    {
        if (you.attribute[ATTR_CARD_COUNTDOWN] && x_chance_in_y(value, 800))
        {
            you.attribute[ATTR_CARD_COUNTDOWN]--;
            mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                you.attribute[ATTR_CARD_COUNTDOWN]);
        }
        // Nemelex piety gain is fairly fast... at least when you
        // have low piety.
        int piety_change = value / 2 + 1;
        if (is_artefact(item))
            piety_change *= 2;
        int piety_denom = 30 + you.piety / 2;

        gain_piety(piety_change, piety_denom);

        // Preserving the old behaviour of giving the big message for
        // artefacts and artefacts only.
        gain.piety_gain = x_chance_in_y(piety_change, piety_denom) ?
           is_artefact(item) ?
            PIETY_LOTS : PIETY_SOME : PIETY_NONE;

        if (item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK
            || is_blood_potion(item))
        {
            // Count chunks and blood potions towards decks of
            // Summoning.
            you.sacrifice_value[OBJ_CORPSES] += value;
        }
        else
            you.sacrifice_value[item.base_type] += value;
        break;
    }

        case GOD_ELYVILON:
    {
        const bool valuable_weapon =
            _destroyed_valuable_weapon(value, item.base_type);
        const bool evil_weapon = is_evil_item(item);

        if (valuable_weapon || evil_weapon)
        {
            if (evil_weapon)
            {
                const char *desc_weapon = "evil";

                // Print this in addition to the above!
                simple_god_message(make_stringf(
                            " welcomes the destruction of %s %s weapon%s.",
                            item.quantity == 1 ? "this" : "these",
                            desc_weapon,
                            item.quantity == 1 ? ""     : "s").c_str(),
                            GOD_ELYVILON);

                gain_piety(1); // Bonus for evil.
            }

            gain.piety_gain = PIETY_SOME;
        }
        break;
    }

    case GOD_JIYVA:
    {
        // compress into range 0..250
        const int stepped = stepdown_value(value, 50, 50, 200, 250);
        gain_piety(stepped, 50);
        gain.piety_gain = (piety_gain_t)min(2, div_rand_round(stepped, 50));

        if (player_under_penance(GOD_JIYVA))
            return gain;

        int item_value = div_rand_round(stepped, 50);
        if (have_passive(passive_t::slime_feed)
            && x_chance_in_y(you.piety, MAX_PIETY)
            && !you_foodless())
        {
            //same as a sultana
            lessen_hunger(70, true);
            gain.jiyva_bonus |= jiyva_slurp_result::food;
        }

        if (have_passive(passive_t::slime_mp)
            && x_chance_in_y(you.piety, MAX_PIETY)
            && you.magic_points < you.max_magic_points)
        {
            inc_mp(max(random2(item_value), 1));
            gain.jiyva_bonus |= jiyva_slurp_result::mp;
        }

        if (have_passive(passive_t::slime_hp)
            && x_chance_in_y(you.piety, MAX_PIETY)
            && you.hp < you.hp_max
            && !you.duration[DUR_DEATHS_DOOR])
        {
            inc_hp(max(random2(item_value), 1));
            gain.jiyva_bonus |= jiyva_slurp_result::hp;
        }
    }
    default:
        break;
    }

    return gain;
}

void jiyva_slurp_item_stack(const item_def& item, int quantity)
{
    ASSERT(you_worship(GOD_JIYVA));
    if (quantity <= 0)
        quantity = item.quantity;
    slurp_gain gain { jiyva_slurp_result::none, PIETY_NONE };
    for (int j = 0; j < quantity; ++j)
    {
        const slurp_gain new_gain = _sacrifice_one_item_noncount(item);

        gain.piety_gain = max(gain.piety_gain, new_gain.piety_gain);
        gain.jiyva_bonus |= new_gain.jiyva_bonus;
    }

    if (gain.piety_gain > PIETY_NONE)
        simple_god_message(" appreciates your sacrifice.");
    if (gain.jiyva_bonus & jiyva_slurp_result::food)
        mpr("You feel a little less hungry.");
    if (gain.jiyva_bonus & jiyva_slurp_result::mp)
        canned_msg(MSG_GAIN_MAGIC);
    if (gain.jiyva_bonus & jiyva_slurp_result::hp)
        canned_msg(MSG_GAIN_HEALTH);
}



/*
static bool _give_nemelex_gift(bool forced = false)
{
    // But only if you're not levitating over deep water.
    // Merfolk don't get gifts in deep water. {due}
    if (!feat_has_solid_floor(grd(you.pos())))
        return false;

    // Nemelex will give at least one gift early.
    if (forced
        || !you.num_total_gifts[GOD_NEMELEX_XOBEH]
        && x_chance_in_y(you.piety + 1, piety_breakpoint(1))
        || one_chance_in(3) && x_chance_in_y(you.piety + 1, MAX_PIETY)
        && !you.attribute[ATTR_CARD_COUNTDOWN])
    {
        misc_item_type gift_type;

        // Make a pure deck.
        int weights[4];
        _get_pure_deck_weights(weights);
        const int choice = choose_random_weighted(weights, weights + 4);
        gift_type = _gift_type_to_deck(choice);
        _show_pure_deck_chances();

        {
            _update_sacrifice_weights(choice);

            canned_msg(MSG_SOMETHING_APPEARS);

            you.attribute[ATTR_CARD_COUNTDOWN] = 5;
            //_inc_gift_timeout(5 + random2avg(9, 2));
            you.num_current_gifts[you.religion]++;
            you.num_total_gifts[you.religion]++;
            take_note(Note(NOTE_GOD_GIFT, you.religion));
        }
        return true;
    }

    return false;
}
*/

static int _leading_sacrifice_group()
{
    int weights[4];
    //_get_pure_deck_weights(weights);
    int best_i = -1, maxweight = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (best_i == -1 || weights[i] > maxweight)
        {
            maxweight = weights[i];
            best_i = i;
        }
    }
    return best_i;
}

static void _give_sac_group_feedback(int which)
{
    ASSERT(which >= 0 && which < 4);
    const char* names[] = {
        "Escape", "Destruction", "Summoning", "Wonder"
    };
    mprf(MSGCH_GOD, "A symbol of %s coalesces before you, then vanishes.",
        names[which]);
}

static bool _god_likes_items(god_type god)
{
    switch (god)
    {
    case GOD_NEMELEX_XOBEH:
        return true;
    case GOD_ELYVILON:
        return true;
    default:
        return false;

    }
}

static bool _god_likes_item(god_type god, const item_def& item)
{
    if (!_god_likes_items(god))
        return false;


    switch (god)
    {
    case GOD_NEMELEX_XOBEH:
        return (!item.is_critical()
            && !item_is_rune(item)
            && item.base_type != OBJ_GOLD
            && (item.base_type != OBJ_MISCELLANY
                || item.sub_type != MISC_BAG));

    case GOD_ELYVILON:
        if (item_is_stationary(item)) // Held in a net?
            return false;
        return (item.base_type == OBJ_WEAPONS
                || item.base_type == OBJ_STAVES
                || item.base_type == OBJ_RODS
                || item.base_type == OBJ_MISSILES)
               // Once you've reached *** once, don't accept mundane weapon
               // sacrifices ever again just because of value.
               && (is_evil_item(item)
                   || you.piety_max[GOD_ELYVILON] < piety_breakpoint(2));

    default:
        return false;
    }
}

static bool _check_nemelex_sacrificing_item_type(const item_def& item)
{
    if (!you.props.exists(NEMELEX_SACRIFICING_KEY)) {
        return true;
    }

    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        return you.props[NEMELEX_SACRIFICING_KEY].get_vector()[NEM_GIFT_ESCAPE].get_bool();
    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_RODS:
    case OBJ_MISSILES:
        return you.props[NEMELEX_SACRIFICING_KEY].get_vector()[NEM_GIFT_DESTRUCTION].get_bool();

    case OBJ_CORPSES:
        return you.props[NEMELEX_SACRIFICING_KEY].get_vector()[NEM_GIFT_SUMMONING].get_bool();

    case OBJ_POTIONS:
        if (is_blood_potion(item))
            return you.props[NEMELEX_SACRIFICING_KEY].get_vector()[NEM_GIFT_SUMMONING].get_bool();
        return you.props[NEMELEX_SACRIFICING_KEY].get_vector()[NEM_GIFT_WONDERS].get_bool();

    case OBJ_FOOD:
        if (item.sub_type == FOOD_CHUNK)
            return you.props[NEMELEX_SACRIFICING_KEY].get_vector()[NEM_GIFT_SUMMONING].get_bool();
        // else fall through
    case OBJ_WANDS:
    case OBJ_SCROLLS:
    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_MISCELLANY:
        return you.props[NEMELEX_SACRIFICING_KEY].get_vector()[NEM_GIFT_WONDERS].get_bool();
    default:
        return false;
    }
}


static piety_gain_t _sacrifice_item_stack(const item_def& item)
{
    piety_gain_t relative_gain = PIETY_NONE;
    for (int j = 0; j < item.quantity; ++j)
    {
        slurp_gain sgain = _sacrifice_one_item_noncount(item);
        const piety_gain_t gain = sgain.piety_gain;

        // Update piety gain if necessary.
        if (gain != PIETY_NONE)
        {
            if (relative_gain == PIETY_NONE)
                relative_gain = gain;
            else            // some + some = lots
                relative_gain = PIETY_LOTS;
        }
    }
    return relative_gain;
}

static void _replace(std::string& s,
    const std::string& find,
    const std::string& repl)
{
    std::string::size_type start = 0;
    std::string::size_type found;

    while ((found = s.find(find, start)) != std::string::npos)
    {
        s.replace(found, find.length(), repl);
        start = found + repl.length();
    }
}

static void _erase_between(std::string& s,
    const std::string& left,
    const std::string& right)
{
    std::string::size_type left_pos;
    std::string::size_type right_pos;

    while ((left_pos = s.find(left)) != std::string::npos
        && (right_pos = s.find(right, left_pos + left.size())) != std::string::npos)
        s.erase(s.begin() + left_pos, s.begin() + right_pos + right.size());
}
static std::string _sacrifice_message(std::string msg,
    const std::string& itname,
    bool glowing, bool plural,
    piety_gain_t piety_gain)
{
    if (glowing)
    {
        _replace(msg, "[", "");
        _replace(msg, "]", "");
    }
    else
        _erase_between(msg, "[", "]");
    _replace(msg, "%", (plural ? "" : "s"));
    _replace(msg, "&", (plural ? "are" : "is"));

    const char* tag_start, * tag_end;
    switch (piety_gain)
    {
    case PIETY_NONE:
        tag_start = "<lightgrey>";
        tag_end = "</lightgrey>";
        break;
    default:
    case PIETY_SOME:
        tag_start = tag_end = "";
        break;
    case PIETY_LOTS:
        tag_start = "<white>";
        tag_end = "</white>";
        break;
    }

    msg.insert(0, itname);
    msg = tag_start + msg + tag_end;

    return msg;
}

static void _print_nemelex_sacrifice_message(const item_def& item,
    piety_gain_t piety_gain, bool your)
{
    static const char* _Sacrifice_Messages[NUM_PIETY_GAIN] =
    {
        " disappear% without a[dditional] glow.",
        " glow% slightly [brighter ]and disappear%.",
        " glow% with a rainbow of weird colours and disappear%.",
    };

    const std::string itname = item.name(your ? DESC_YOUR : DESC_THE);
    mpr(_sacrifice_message(_Sacrifice_Messages[piety_gain], itname,
        itname.find("glowing") != std::string::npos,
        item.quantity > 1,
        piety_gain));
}

static bool _confirm_pray_sacrifice(god_type god)
{
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        bool temp_;
        if (_god_likes_item(god, *si)
            && needs_handle_warning(*si, OPER_PRAY, temp_))
        {
            std::string prompt = "Really sacrifice stack with ";
            prompt += si->name(DESC_A);
            prompt += " in it?";

            if (!yesno(prompt.c_str(), false, 'n'))
                return false;
        }
    }

    return true;
}

static bool _offer_items()
{
    if (!_god_likes_items(you.religion))
        return false;

    if (!_confirm_pray_sacrifice(you.religion))
        return false;

    int i = you.visible_igrd(you.pos());

    god_acting gdact;

    int num_sacced = 0;
    int num_disliked = 0;
    item_def* disliked_item = 0;

    const int old_leading = _leading_sacrifice_group();

    while (i != NON_ITEM)
    {
        item_def& item(mitm[i]);
        const int next = item.link;  // in case we can't get it later.
        const bool disliked = !_god_likes_item(you.religion, item);

        if (disliked)
        {
            i = next;
            if (disliked)
            {
                num_disliked++;
                disliked_item = &item;
            }
            continue;
        }

        // Skip items you don't want to sacrifice right now.
        if (you_worship(GOD_NEMELEX_XOBEH)
            && !_check_nemelex_sacrificing_item_type(item))
        {
            i = next;
            continue;
        }


        // Ignore {!D} inscribed items.
        if (!check_warning_inscriptions(item, OPER_DESTROY))
        {
            mpr("Won't sacrifice {!D} inscribed item.");
            i = next;
            continue;
        }

        if (_god_likes_item(you.religion, item)
            && (item.inscription.find("=p") != std::string::npos))
        {
            const std::string msg =
                "Really sacrifice " + item.name(DESC_A) + "?";

            if (!yesno(msg.c_str(), false, 'n'))
            {
                i = next;
                continue;
            }
        }

        piety_gain_t relative_gain = _sacrifice_item_stack(item);
        _print_nemelex_sacrifice_message(mitm[i], relative_gain, true);
        item_was_destroyed(mitm[i]);
        destroy_item(i);
        i = next;
        num_sacced++;
    }

    if (num_sacced > 0 && you.religion == GOD_NEMELEX_XOBEH)
    {
        const int new_leading = _leading_sacrifice_group();
        if (old_leading != new_leading || one_chance_in(50))
            _give_sac_group_feedback(new_leading);
    }

    // Explanatory messages if nothing the god likes is sacrificed.
    else if (num_sacced == 0 && num_disliked > 0)
    {
        ASSERT(disliked_item);

        if (item_is_orb(*disliked_item))
            simple_god_message(" wants the Orb's power used on the surface!");
        else if (item_is_rune(*disliked_item))
            simple_god_message(" wants the runes to be proudly displayed.");
        else if (you.religion == GOD_NEMELEX_XOBEH)
            if (disliked_item->base_type == OBJ_GOLD)
                simple_god_message(" does not care about gold!");
            else
                simple_god_message(" not offer them!");
    }
    
    if (num_sacced == 0 && you_worship(GOD_ELYVILON))
    {
        mprf("There are no %sweapons here to destroy!",
             you.piety_max[GOD_ELYVILON] < piety_breakpoint(2) ? "" : "unholy or evil ");
    }
    return (num_sacced > 0);
}

void pray()
{
    if (silenced(you.pos()))
    {
        mpr("You are unable to make a sound!");
        return;
    }

    // only successful prayer takes time
    you.turn_is_over = false;

    bool something_happened = false;

    if (you.religion == GOD_NO_GOD)
    {
        const mon_holy_type holi = you.holiness();

        mprf(
            "You spend a moment contemplating the meaning of %s.",
            holi == MH_NONLIVING || holi == MH_UNDEAD ? "existence" : "life");

        // Zen meditation is timeless.
        return;
    }

    mprf("You offer a prayer to %s.", god_name(you.religion).c_str());


    // All sacrifices affect items you're standing on.
    something_happened |= _offer_items();

    if (player_under_penance())
        simple_god_message(" demands penance!");
    else
        mpr(god_prayer_reaction().c_str());

    if (something_happened)
        you.turn_is_over = true;
    dprf("piety: %d (-%d)", you.piety, you.piety_hysteresis);
}

