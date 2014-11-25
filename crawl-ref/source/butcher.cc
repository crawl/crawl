/**
 * @file
 * @brief Functions for corpse butchering & bottling.
 **/

#include "AppHdr.h"

#include "butcher.h"

#include "bloodspatter.h"
#include "command.h"
#include "delay.h"
#include "env.h"
#include "english.h"
#include "food.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "options.h"
#include "output.h"
#include "prompt.h"
#include "rot.h"
#include "stash.h"
#include "stepdown.h"
#include "stringutil.h"

#ifdef TOUCH_UI
#include "invent.h"
#include "menu.h"
#endif

static bool _should_butcher(item_def& corpse, bool bottle_blood = false)
{
    if (is_forbidden_food(corpse)
        && (Options.confirm_butcher == CONFIRM_NEVER
            || !yesno("Desecrating this corpse would be a sin. Continue anyway?",
                      false, 'n')))
    {
        if (Options.confirm_butcher != CONFIRM_NEVER)
            canned_msg(MSG_OK);
        return false;
    }

    return true;
}

static bool _corpse_butchery(item_def& corpse,
                             bool first_corpse = true)
{
    ASSERT(corpse.defined());
    ASSERT(corpse.base_type == OBJ_CORPSES);

    if (!_should_butcher(corpse))
        return false;

    const bool was_holy = mons_class_holiness(corpse.mon_type) == MH_HOLY;
    const bool was_intelligent = corpse_intelligence(corpse) >= I_NORMAL;
    const bool was_same_genus = is_player_same_genus(corpse.mon_type);

    if (you.species == SP_VAMPIRE
        && can_bottle_blood_from_corpse(corpse.mon_type))
    {
        mprf("You bottle %s blood.", apostrophise(corpse.name(DESC_THE)).c_str());

        if (mons_skeleton(corpse.mon_type) && one_chance_in(3))
            turn_corpse_into_skeleton_and_blood_potions(corpse);
        else
            turn_corpse_into_blood_potions(corpse);
    }
    else
    {
        mprf("You butcher %s.",
             corpse.name(DESC_THE).c_str());

        butcher_corpse(corpse);

        if (you.berserk() && you.berserk_penalty != NO_BERSERK_PENALTY)
        {
            mpr("You enjoyed that.");
            you.berserk_penalty = 0;
        }

        // Also, don't waste time picking up chunks if you're already
        // starving. (jpeg)
        if (you.hunger_state > HS_STARVING || you.species == SP_VAMPIRE)
            autopickup();
    }

    if (was_same_genus)
        did_god_conduct(DID_CANNIBALISM, 2);
    else if (was_holy)
        did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 4);
    else if (was_intelligent)
        did_god_conduct(DID_DESECRATE_SOULED_BEING, 1);
    StashTrack.update_stash(you.pos()); // Stash-track the generated items.

    you.turn_is_over = true;
    return true;
}

#ifdef TOUCH_UI
static string _butcher_menu_title(const Menu *menu, const string &oldt)
{
    return oldt;
}
#endif

static int _corpse_quality(const item_def &item, bool bottle_blood)
{
    const corpse_effect_type ce = determine_chunk_effect(item);
    // Being almost rotten away has 480 badness.
    int badness = 3 * item.freshness;
    if (ce == CE_POISONOUS)
        badness += 600;
    else if (ce == CE_MUTAGEN)
        badness += 1000;
    else if (ce == CE_ROT)
        badness += 1000;

    // Bottleable corpses first, unless forbidden
    if (bottle_blood && !can_bottle_blood_from_corpse(item.mon_type))
        badness += 4000;

    if (is_forbidden_food(item))
        badness += 10000;
    return -badness;
}

/**
 * Attempt to butcher a corpse.
 *
 * @param which_corpse      The index of the corpse. (index into what...?)
 *                          -1 indicates that the first valid corpse should
 *                          be chosen.
 */
bool butchery(int which_corpse)
{
    if (you.visible_igrd(you.pos()) == NON_ITEM)
    {
        mpr("There isn't anything here!");
        return false;
    }

    // First determine which things there are to butcher.
    int corpse_id   = -1;
    const bool prechosen  = (which_corpse != -1);
    const bool bottle_blood = you.species == SP_VAMPIRE;
    typedef pair<item_def *, int> corpse_quality;
    vector<corpse_quality> corpses;

    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
            continue;

        // If the pre-chosen corpse exists, pretend it was the only one.
        if (prechosen && si->index() == which_corpse)
        {
            corpses = { { &*si, 0 } };
            corpse_id = si->index();
            break;
        }

        corpses.emplace_back(&*si, _corpse_quality(*si, bottle_blood));
    }

    if (corpses.size() == 0)
    {
        mprf("There isn't anything to %s here.",
             bottle_blood ? "bottle or butcher" : "butcher");
        return false;
    }

    stable_sort(begin(corpses), end(corpses), greater_second<corpse_quality>());
    // If we didn't select a pre-chosen corpse, pick the best one.
    if (corpse_id < 0)
        corpse_id = corpses[0].first->index();

    // Butcher pre-chosen corpse, if found, or if there is only one corpse.
    bool success = false;
    if (prechosen && corpse_id == which_corpse
        || corpses.size() == 1 && Options.confirm_butcher != CONFIRM_ALWAYS
        || Options.confirm_butcher == CONFIRM_NEVER)
    {
        if (Options.confirm_butcher == CONFIRM_NEVER
            && !_should_butcher(mitm[corpse_id], bottle_blood))
        {
            mprf("There isn't anything suitable to %s here.",
                 bottle_blood ? "bottle or butcher" : "butcher");
            return false;
        }

        return _corpse_butchery(mitm[corpse_id], true);
    }

    // Now pick what you want to butcher. This is only a problem
    // if there are several corpses on the square.
    bool butcher_all   = false;
    bool first_corpse  = true;
#ifdef TOUCH_UI
    vector<const item_def*> meat;
    for (const auto &entry : corpses)
        meat.push_back(entry.first);

    corpse_id = -1;
    vector<SelItem> selected =
        select_items(meat, bottle_blood ? "Choose a corpse to bottle or butcher"
                                        : "Choose a corpse to butcher",
                     false, MT_ANY, _butcher_menu_title);
    redraw_screen();
    for (int i = 0, count = selected.size(); i < count; ++i)
    {
        corpse_id = selected[i].item->index();
        if (_corpse_butchery(mitm[corpse_id], first_corpse))
        {
            success = true;
            first_corpse = false;
        }
    }

#else
    int keyin;
    bool repeat_prompt = false;
    for (auto &entry : corpses)
    {
        item_def * const it = entry.first;

        if (butcher_all)
            corpse_id = it->index();
        else
        {
            corpse_id = -1;

            string corpse_name = it->name(DESC_A);

            // We don't need to check for undead because
            // * Mummies can't eat.
            // * Ghouls relish the bad things.
            // * Vampires won't bottle bad corpses.
            if (you.undead_state() == US_ALIVE)
                corpse_name = get_menu_colour_prefix_tags(*it, DESC_A);

            // Shall we butcher this corpse?
            do
            {
                const bool can_bottle =
                    can_bottle_blood_from_corpse(it->mon_type);
                mprf(MSGCH_PROMPT, "%s %s? [(y)es/(c)hop/(n)o/(a)ll/(q)uit/?]",
                     can_bottle ? "Bottle" : "Butcher",
                     corpse_name.c_str());
                repeat_prompt = false;

                keyin = toalower(getchm(KMC_CONFIRM));
                switch (keyin)
                {
                case 'y':
                case 'c':
                case 'd':
                case 'a':
                    corpse_id = it->index();

                    if (keyin == 'a')
                        butcher_all = true;
                    break;

                case 'q':
                CASE_ESCAPE
                    canned_msg(MSG_OK);
                    return success;

                case '?':
                    show_butchering_help();
                    clear_messages();
                    redraw_screen();
                    repeat_prompt = true;
                    break;

                default:
                    break;
                }
            }
            while (repeat_prompt);
        }

        if (corpse_id != -1)
        {
            if (_corpse_butchery(mitm[corpse_id], first_corpse))
            {
                success = true;
                first_corpse = false;
            }
        }
    }
#endif
    if (!butcher_all && corpse_id == -1)
    {
        mprf("There isn't anything %s to %s here.",
             Options.confirm_butcher == CONFIRM_NEVER ? "suitable" : "else",
             bottle_blood ? "bottle" : "butcher");
    }

    return success;
}


static void _create_monster_hide(const item_def corpse)
{
    // make certain sources of dragon hides less scummable
    // (kiku's corpse drop, gozag ghoul corpse shops)
    if (corpse.props.exists(MANGLED_CORPSE_KEY))
        return;

    const armour_type type = hide_for_monster(corpse.mon_type);
    ASSERT(type != NUM_ARMOURS);

    int o = items(false, OBJ_ARMOUR, type, 0);
    squash_plusses(o);

    if (o == NON_ITEM)
        return;
    item_def& item = mitm[o];

    do_uncurse_item(item, false);

    // Automatically identify the created hide.
    set_ident_flags(item, ISFLAG_IDENT_MASK);

    const monster_type montype =
    static_cast<monster_type>(corpse.orig_monnum);
    if (!invalid_monster_type(montype) && mons_is_unique(montype))
        item.inscription = mons_type_name(montype, DESC_PLAIN);

    const coord_def pos = item_pos(corpse);
    if (!pos.origin())
        move_item_to_grid(&o, pos);
}

void maybe_drop_monster_hide(const item_def corpse)
{
    if (mons_class_leaves_hide(corpse.mon_type) && !one_chance_in(3))
        _create_monster_hide(corpse);
}

int get_max_corpse_chunks(monster_type mons_class)
{
    return mons_weight(mons_class) / 150;
}

/** Skeletonise this corpse.
 *
 *  @param item the corpse to be turned into a skeleton.
 *  @returns whether a valid skeleton could be made.
 */
bool turn_corpse_into_skeleton(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);

    // Some monsters' corpses lack the structure to leave skeletons
    // behind.
    if (!mons_skeleton(item.mon_type))
        return false;

    item.sub_type = CORPSE_SKELETON;
    item.special  = FRESHEST_CORPSE; // reset rotting counter
    item.rnd = 1 + random2(255); // not sure this is necessary, but...
    item.props.erase(FORCED_ITEM_COLOUR_KEY);
    return true;
}

static void _bleed_monster_corpse(const item_def corpse)
{
    const coord_def pos = item_pos(corpse);
    if (!pos.origin())
    {
        const int max_chunks = get_max_corpse_chunks(corpse.mon_type);
        bleed_onto_floor(pos, corpse.mon_type, max_chunks, true);
    }
}

void turn_corpse_into_chunks(item_def &item, bool bloodspatter,
                             bool make_hide)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);
    const item_def corpse = item;
    const int max_chunks = get_max_corpse_chunks(item.mon_type);

    if (bloodspatter)
        _bleed_monster_corpse(corpse);

    item.base_type = OBJ_FOOD;
    item.sub_type  = FOOD_CHUNK;
    item.quantity  = 1 + random2(max_chunks);
    item.quantity  = stepdown_value(item.quantity, 4, 4, 12, 12);

    // Don't mark it as dropped if we are forcing autopickup of chunks.
    if (you.force_autopickup[OBJ_FOOD][FOOD_CHUNK] <= 0
        && is_bad_food(item))
    {
        item.flags |= ISFLAG_DROPPED;
    }
    else if (you.species != SP_VAMPIRE)
        clear_item_pickup_flags(item);

    // Initialise timer depending on corpse age
    init_perishable_stack(item, item.special * ROT_TIME_FACTOR);

    // Happens after the corpse has been butchered.
    if (make_hide)
        maybe_drop_monster_hide(corpse);
}

static void _turn_corpse_into_skeleton_and_chunks(item_def &item, bool prefer_chunks)
{
    item_def copy = item;

    // Complicated logic, but unless we use the original, both could fail if
    // mitm[] is overstuffed.
    if (prefer_chunks)
    {
        turn_corpse_into_chunks(item);
        turn_corpse_into_skeleton(copy);
    }
    else
    {
        turn_corpse_into_chunks(copy);
        turn_corpse_into_skeleton(item);
    }

    copy_item_to_grid(copy, item_pos(item));
}

void butcher_corpse(item_def &item, maybe_bool skeleton, bool chunks)
{
    item_was_destroyed(item);
    if (!mons_skeleton(item.mon_type))
        skeleton = MB_FALSE;
    if (skeleton == MB_TRUE || skeleton == MB_MAYBE && one_chance_in(3))
    {
        if (chunks)
            _turn_corpse_into_skeleton_and_chunks(item, skeleton != MB_TRUE);
        else
        {
            _bleed_monster_corpse(item);
            maybe_drop_monster_hide(item);
            turn_corpse_into_skeleton(item);
        }
    }
    else
    {
        if (chunks)
            turn_corpse_into_chunks(item);
        else
        {
            _bleed_monster_corpse(item);
            maybe_drop_monster_hide(item);
            destroy_item(item.index());
        }
    }
}

bool can_bottle_blood_from_corpse(monster_type mons_class)
{
    if (you.species != SP_VAMPIRE || !mons_has_blood(mons_class))
        return false;

    return mons_corpse_effect(mons_class) == CE_CLEAN;
}

int num_blood_potions_from_corpse(monster_type mons_class)
{
    // Max. amount is about one third of the max. amount for chunks.
    const int max_chunks = get_max_corpse_chunks(mons_class);

    // Max. amount is about one third of the max. amount for chunks.
    int pot_quantity = max_chunks / 3;
    pot_quantity = stepdown_value(pot_quantity, 2, 2, 6, 6);

    if (pot_quantity < 1)
        pot_quantity = 1;

    return pot_quantity;
}

// If autopickup is active, the potions are auto-picked up after creation.
void turn_corpse_into_blood_potions(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);

    const item_def corpse = item;
    const monster_type mons_class = corpse.mon_type;

    ASSERT(can_bottle_blood_from_corpse(mons_class));

    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_BLOOD;
    item_colour(item);
    clear_item_pickup_flags(item);
    item.props.clear();

    item.quantity = num_blood_potions_from_corpse(mons_class);

    // Initialise timer depending on corpse age
    init_perishable_stack(item,
                          item.special * ROT_TIME_FACTOR + FRESHEST_BLOOD);

    // Happens after the blood has been bottled.
    maybe_drop_monster_hide(corpse);
}

void turn_corpse_into_skeleton_and_blood_potions(item_def &item)
{
    item_def blood_potions = item;

    if (mons_skeleton(item.mon_type))
        turn_corpse_into_skeleton(item);

    int o = get_mitm_slot();
    if (o != NON_ITEM)
    {
        turn_corpse_into_blood_potions(blood_potions);
        copy_item_to_grid(blood_potions, you.pos());
    }
}
