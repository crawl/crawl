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
#include "food.h"
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
#include "stepdown.h"
#include "stringutil.h"

static bool _should_butcher(int corpse_id, bool bottle_blood = false)
{
    const item_def &corpse = mitm[corpse_id];

    if (is_forbidden_food(corpse)
        && (Options.confirm_butcher == CONFIRM_NEVER
            || !yesno("Desecrating this corpse would be a sin. Continue anyway?",
                      false, 'n')))
    {
        if (Options.confirm_butcher != CONFIRM_NEVER)
            canned_msg(MSG_OK);
        return false;
    }
    else if (!bottle_blood && you.species == SP_VAMPIRE
             && (can_bottle_blood_from_corpse(corpse.mon_type)
                 || mons_has_blood(corpse.mon_type) && !is_bad_food(corpse)))
    {
        bool can_bottle = can_bottle_blood_from_corpse(corpse.mon_type);
        const string msg = make_stringf("You could drain this corpse's blood with <w>%s</w> instead%s. Continue anyway?",
                                        command_to_string(CMD_EAT).c_str(),
                                        can_bottle ? ", or drain it" : "");
        if (Options.confirm_butcher != CONFIRM_NEVER)
        {
            if (!yesno(msg.c_str(), true, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }
    }

    return true;
}

static bool _corpse_butchery(int corpse_id,
                             bool first_corpse = true,
                             bool bottle_blood = false)
{
    ASSERT(corpse_id != -1);

    if (!_should_butcher(corpse_id, bottle_blood))
        return false;

    // Start work on the first corpse we butcher.
    if (first_corpse)
        mitm[corpse_id].butcher_amount++;

    int work_req = max(0, 4 - mitm[corpse_id].butcher_amount);

    delay_type dtype = DELAY_BUTCHER;
    // Sanity checks.
    if (bottle_blood
        && can_bottle_blood_from_corpse(mitm[corpse_id].mon_type))
    {
        dtype = DELAY_BOTTLE_BLOOD;
    }

    start_delay(dtype, work_req, corpse_id, mitm[corpse_id].special);

    you.turn_is_over = true;
    return true;
}

#ifdef TOUCH_UI
static string _butcher_menu_title(const Menu *menu, const string &oldt)
{
    return oldt;
}
#endif

bool butchery(int which_corpse, bool bottle_blood)
{
    if (you.visible_igrd(you.pos()) == NON_ITEM)
    {
        mpr("There isn't anything here!");
        return false;
    }

    // First determine how many things there are to butcher.
    int num_corpses = 0;
    int corpse_id   = -1;
    int best_badness = INT_MAX;
    bool prechosen  = (which_corpse != -1);
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
        {
            if (bottle_blood && !can_bottle_blood_from_corpse(si->mon_type))
                continue;

            // Return pre-chosen corpse if it exists.
            if (prechosen && si->index() == which_corpse)
            {
                corpse_id = si->index();
                num_corpses = 1;
                break;
            }

            const corpse_effect_type ce = determine_chunk_effect(*si);
            // Being almost rotten away has 480 badness.
            int badness = 3 * si->special;
            if (ce == CE_POISONOUS)
                badness += 600;
            else if (ce == CE_MUTAGEN)
                badness += 1000;
            else if (ce == CE_ROT)
                badness += 1000;

            if (is_forbidden_food(*si))
                badness += 10000;

            if (badness < best_badness)
                corpse_id = si->index(), best_badness = badness;
            num_corpses++;
        }
    }

    if (num_corpses == 0)
    {
        mprf("There isn't anything to %s here.",
             bottle_blood ? "bottle" : "butcher");
        return false;
    }

    // Butcher pre-chosen corpse, if found, or if there is only one corpse.
    bool success = false;
    if (prechosen && corpse_id == which_corpse
        || num_corpses == 1 && Options.confirm_butcher != CONFIRM_ALWAYS
        || Options.confirm_butcher == CONFIRM_NEVER)
    {
        if (Options.confirm_butcher == CONFIRM_NEVER
            && !_should_butcher(corpse_id, bottle_blood))
        {
            mprf("There isn't anything suitable to %s here.",
                 bottle_blood ? "bottle" : "butcher");
            return false;
        }

        return _corpse_butchery(corpse_id, true, bottle_blood);
    }

    // Now pick what you want to butcher. This is only a problem
    // if there are several corpses on the square.
    bool butcher_all   = false;
    bool first_corpse  = true;
#ifdef TOUCH_UI
    vector<const item_def*> meat;
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
            continue;

        if (bottle_blood && !can_bottle_blood_from_corpse(si->mon_type))
            continue;
        meat.push_back(& (*si));
    }

    corpse_id = -1;
    vector<SelItem> selected =
        select_items(meat, bottle_blood ? "Choose a corpse to bottle"
                                        : "Choose a corpse to butcher",
                     false, MT_ANY, _butcher_menu_title);
    redraw_screen();
    for (int i = 0, count = selected.size(); i < count; ++i)
    {
        corpse_id = selected[i].item->index();
        if (_corpse_butchery(corpse_id, first_corpse, bottle_blood))
        {
            success = true;
            first_corpse = false;
        }
    }

#else
    int keyin;
    bool repeat_prompt = false;
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
            continue;

        if (bottle_blood && !can_bottle_blood_from_corpse(si->mon_type))
            continue;

        if (butcher_all)
            corpse_id = si->index();
        else
        {
            corpse_id = -1;

            string corpse_name = si->name(DESC_A);

            // We don't need to check for undead because
            // * Mummies can't eat.
            // * Ghouls relish the bad things.
            // * Vampires won't bottle bad corpses.
            if (you.undead_state() == US_ALIVE)
                corpse_name = get_menu_colour_prefix_tags(*si, DESC_A);

            // Shall we butcher this corpse?
            do
            {
                mprf(MSGCH_PROMPT, "%s %s? [(y)es/(c)hop/(n)o/(a)ll/(q)uit/?]",
                     bottle_blood ? "Bottle" : "Butcher",
                     corpse_name.c_str());
                repeat_prompt = false;

                keyin = toalower(getchm(KMC_CONFIRM));
                switch (keyin)
                {
                case 'y':
                case 'c':
                case 'd':
                case 'a':
                    corpse_id = si->index();

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
            if (_corpse_butchery(corpse_id, first_corpse, bottle_blood))
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
    // kiku_receive_corpses() creates corpses that are easily scummed
    // for hides.  We prevent this by setting "never_hide" as an item
    // property of corpses it creates.
    if (corpse.props.exists(NEVER_HIDE_KEY))
        return;

    monster_type mons_class = corpse.mon_type;
    armour_type type;

    // These values cannot be set by a reasonable formula: {dlb}
    if (mons_genus(mons_class) == MONS_TROLL)
        mons_class = MONS_TROLL;
    switch (mons_class)
    {
        case MONS_FIRE_DRAGON:    type = ARM_FIRE_DRAGON_HIDE;    break;
        case MONS_TROLL:          type = ARM_TROLL_HIDE;          break;
        case MONS_ICE_DRAGON:     type = ARM_ICE_DRAGON_HIDE;     break;
        case MONS_STEAM_DRAGON:   type = ARM_STEAM_DRAGON_HIDE;   break;
        case MONS_MOTTLED_DRAGON: type = ARM_MOTTLED_DRAGON_HIDE; break;
        case MONS_STORM_DRAGON:   type = ARM_STORM_DRAGON_HIDE;   break;
        case MONS_GOLDEN_DRAGON:  type = ARM_GOLD_DRAGON_HIDE;    break;
        case MONS_SWAMP_DRAGON:   type = ARM_SWAMP_DRAGON_HIDE;   break;
        case MONS_PEARL_DRAGON:   type = ARM_PEARL_DRAGON_HIDE;   break;
        case MONS_SHADOW_DRAGON:  type = ARM_SHADOW_DRAGON_HIDE;  break;
        case MONS_QUICKSILVER_DRAGON:
            type = ARM_QUICKSILVER_DRAGON_HIDE;
            break;
        default:
            die("an unknown hide drop");
    }

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
    item.props.clear();
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
    // specifically do not clear the props, so that we keep silly colour
    // overrides from weirdly-coloured monsters

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
    if (you.species != SP_VAMPIRE || you.experience_level < 6
        || !mons_has_blood(mons_class))
    {
        return false;
    }

    int chunk_type = mons_corpse_effect(mons_class);
    if (chunk_type == CE_CLEAN)
        return true;

    return false;
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
