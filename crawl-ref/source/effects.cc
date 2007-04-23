/*
 *  File:       effects.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "effects.h"

#include <cstdlib>
#include <string.h>
#include <stdio.h>

#include "externs.h"

#include "beam.h"
#include "direct.h"
//#include "dungeon.h"
#include "hiscores.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "makeitem.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "newgame.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "skills2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stuff.h"
#include "view.h"

// torment_monsters is called with power 0 because torment is
// UNRESISTABLE except for being undead or having torment
// resistance!  Even if we used maximum power of 1000, high
// level monsters and characters would save too often.  (GDL)

int torment_monsters(int x, int y, int pow, int caster)
{
    UNUSED( pow );

    // is player?
    if (x == you.x_pos && y == you.y_pos)
    {
        // [dshaligram] Switched to using ouch() instead of dec_hp so that
        // notes can also track torment and activities can be interrupted
        // correctly.
        int hploss = 0;
        if (!player_res_torment())
        {
            // negative energy resistance can alleviate torment
            hploss = you.hp * (50 - player_prot_life() * 5) / 100 - 1;
            if (hploss >= you.hp)
                hploss = you.hp - 1;
            if (hploss < 0)
                hploss = 0;
        }

        if (!hploss)
            mpr("You feel a surge of unholy energy.");
        else
        {
            mpr("Your body is wracked with pain!");

            const char *aux = "torment";
            if (caster < 0)
            {
                switch (caster)
                {
                case TORMENT_CARDS:
                case TORMENT_SPELL:
                    aux = "Symbol of Torment";
                    break;
                case TORMENT_SPWLD:
                    // FIXME: If we ever make any other weapon / randart
                    // eligible to torment, this will be incorrect.
                    aux = "Sceptre of Torment";
                    break;
                case TORMENT_SCROLL:
                    aux = "scroll of torment";
                    break;
                }
                caster = TORMENT_GENERIC;
            }
            ouch(hploss, caster, 
                    caster != TORMENT_GENERIC? KILLED_BY_MONSTER 
                                             : KILLED_BY_SOMETHING, 
                    aux);
        }

        return 1;
    }

    // check for monster in cell
    int mon = mgrd[x][y];

    if (mon == NON_MONSTER)
        return 0;

    struct monsters *monster = &menv[mon];

    if (monster->type == -1)
        return 0;

    if (mons_res_negative_energy( monster ) >= 3)
        return 0;

    monster->hit_points = 1 + (monster->hit_points / 2);
    simple_monster_message(monster, " convulses!");

    return 1;
}

void torment(int caster, int tx, int ty)
{
    apply_area_within_radius(torment_monsters, tx, ty, 0, 8, caster);
}                               // end torment()

static std::string who_banished(const std::string &who)
{
    return (who.empty()? who : " (" + who + ")");
}

void banished(int gate_type, const std::string &who)
{
#ifdef DGL_MILESTONES
    if (gate_type == DNGN_ENTER_ABYSS)
        mark_milestone("abyss.enter",
                       "is cast into the Abyss!" + who_banished(who));
    else if (gate_type == DNGN_EXIT_ABYSS)
        mark_milestone("abyss.exit",
                       "escaped from the Abyss!" + who_banished(who));
#endif

    if (gate_type == DNGN_ENTER_ABYSS)
    {
        const std::string what = "Cast into the Abyss" + who_banished(who);
        take_note(Note(NOTE_USER_NOTE, 0, 0, what.c_str()), true);
    }

    down_stairs(true, you.your_level, gate_type);  // heh heh
    untag_followers(); // safety
}                               // end banished()

bool forget_spell(void)
{
    if (!you.spell_no)
        return (false);

    // find a random spell to forget:
    int slot = -1;
    int num  = 0;

    for (int i = 0; i < 25; i++)
    {
        if (you.spells[i] != SPELL_NO_SPELL)
        {
            num++;
            if (one_chance_in( num ))
                slot = i;
        }
    }

    if (slot == -1)              // should never happen though
        return (false);

    del_spell_from_memory_by_slot( slot );

    return (true);
}                               // end forget_spell()

// use player::decrease_stats() instead iff:
// (a) player_sust_abil() should not factor in; and
// (b) there is no floor to the final stat values {dlb}
bool lose_stat(unsigned char which_stat, unsigned char stat_loss, bool force)
{
    bool statLowered = false;   // must initialize to false {dlb}
    char *ptr_stat = 0;         // NULL {dlb}
    char *ptr_redraw = 0;       // NULL {dlb}
    char newValue = 0;          // holds new value, for comparison to old {dlb}

    // begin outputing message: {dlb}
    strcpy(info, "You feel ");

    // set pointers to appropriate variables: {dlb}
    if (which_stat == STAT_RANDOM)
        which_stat = random2(NUM_STATS);

    switch (which_stat)
    {
    case STAT_STRENGTH:
        strcat(info, "weakened");
        ptr_stat = &you.strength;
        ptr_redraw = &you.redraw_strength;
        break;

    case STAT_DEXTERITY:
        strcat(info, "clumsy");
        ptr_stat = &you.dex;
        ptr_redraw = &you.redraw_dexterity;
        break;

    case STAT_INTELLIGENCE:
        strcat(info, "dopey");
        ptr_stat = &you.intel;
        ptr_redraw = &you.redraw_intelligence;
        break;
    }

    // scale modifier by player_sust_abil() - right-shift
    // permissible because stat_loss is unsigned: {dlb}
    if (!force)
        stat_loss >>= player_sust_abil();

    // newValue is current value less modifier: {dlb}
    newValue = *ptr_stat - stat_loss;

    // XXX: Death by stat loss is currently handled in the redraw code. -- bwr
    if (newValue < 0)
        newValue = 0;

    // conceivable that stat was already *at* three
    // or stat_loss zeroed by player_sust_abil(): {dlb}
    //
    // Actually, that code was somewhat flawed.  Several race-class combos
    // can start with a stat lower than three, and this block (which
    // used to say '!=' would actually cause stat gain with the '< 3'
    // check that used to be above.  Crawl has stat-death code and I 
    // don't see why we shouldn't be using it here.  -- bwr
    if (newValue < *ptr_stat)
    {
        *ptr_stat = newValue;
        *ptr_redraw = 1;

        // handle burden change, where appropriate: {dlb}
        if (ptr_stat == &you.strength)
            burden_change();

        statLowered = true;  // that is, stat was lowered (not just changed)
    }

    // a warning to player that s/he cut it close: {dlb}
    if (!statLowered)
        strcat(info, " for a moment");

    // finish outputting message: {dlb}
    strcat(info, ".");
    mpr(info);

    return (statLowered);
}                               // end lose_stat()

void direct_effect(struct bolt &pbolt)
{
    int damage_taken = 0;

    switch (pbolt.type)
    {
    case DMNBM_HELLFIRE:
        pbolt.name = "hellfire";
        pbolt.ex_size = 1;
        pbolt.flavour = BEAM_HELLFIRE;
        pbolt.is_explosion = true;
        pbolt.type = SYM_ZAP;
        pbolt.colour = RED;
        pbolt.thrower = KILL_MON_MISSILE;
        pbolt.aux_source.clear();
        pbolt.is_beam = false;
        pbolt.is_tracer = false;
        pbolt.hit = 20;
        pbolt.damage = dice_def( 3, 20 );
        pbolt.aux_source = "burst of hellfire";
        explosion( pbolt );
        break;

    case DMNBM_SMITING:
        mpr( "Something smites you!" );
        pbolt.name = "smiting";
        pbolt.aux_source = "by divine providence";
        damage_taken = 7 + random2avg(11, 2);
        break;

    case DMNBM_BRAIN_FEED:
        // lose_stat() must come last {dlb}
        if (one_chance_in(3) && lose_stat(STAT_INTELLIGENCE, 1))
            mpr("Something feeds on your intellect!");
        else
            mpr("Something tries to feed on your intellect!");
        break;
    }

    // apply damage and handle death, where appropriate {dlb}
    if (damage_taken > 0)
        ouch(damage_taken, pbolt.beam_source, KILLED_BY_BEAM, 
             pbolt.aux_source.c_str());

    return;
}                               // end direct_effect()

// monster-to-monster
void mons_direct_effect(struct bolt &pbolt, int i)
{
    // note the translation here - important {dlb}
    int o = menv[i].foe;
    struct monsters *monster = &menv[o];
    int damage_taken = 0;

    // annoy the target
    behaviour_event(monster, ME_ANNOY, i);

    switch (pbolt.type)
    {
    case DMNBM_HELLFIRE:
        simple_monster_message(monster, " is engulfed in hellfire.");
        pbolt.name = "hellfire";
        pbolt.flavour = BEAM_LAVA;

        damage_taken = 5 + random2(10) + random2(5);
        damage_taken = mons_adjust_flavoured(monster, pbolt, damage_taken);
        break;

    case DMNBM_SMITING:
        simple_monster_message(monster, " is smitten.");
        pbolt.name = "smiting";
        pbolt.flavour = BEAM_MISSILE;

        damage_taken += 7 + random2avg(11, 2);
        break;

    case DMNBM_BRAIN_FEED:      // not implemented here (nor, probably, can be)
        break;

    case DMNBM_MUTATION:
        if (mons_holiness(monster) != MH_NATURAL ||
            mons_immune_magic(monster))
            simple_monster_message(monster, " is unaffected.");
        else if (check_mons_resist_magic( monster, pbolt.ench_power ))
            simple_monster_message(monster, " resists.");
        else
            monster_polymorph(monster, RANDOM_MONSTER, 100);
        break;
    }

    // apply damage and handle death, where appropriate {dlb}
    if (damage_taken > 0)
    {
        hurt_monster(monster, damage_taken);

        if (monster->hit_points < 1)
            monster_die(monster, KILL_MON_MISSILE, i);
    }

    return;
}                               // end mons_direct_effect()

void random_uselessness(unsigned char ru, unsigned char sc_read_2)
{
    char wc[30];
    int temp_rand = 0;          // probability determination {dlb}

    switch (ru)
    {
    case 0:
        weird_colours(random2(256), wc);
        mprf("The dust glows a %s colour!", wc);
        break;

    case 1:
        mpr("The scroll reassembles itself in your hand!");
        inc_inv_item_quantity( sc_read_2, 1 );
        break;

    case 2:
        if (you.equip[EQ_WEAPON] != -1)
        {
            weird_colours(random2(256), wc);
            mprf("%s glows %s for a moment.",
                 you.inv[you.equip[EQ_WEAPON]].name(DESC_CAP_YOUR).c_str(),
                 wc);
        }
        else
        {
            canned_msg(MSG_NOTHING_HAPPENS);
        }
        break;

    case 3:
        strcpy(info, "You hear the distant roaring of an enraged ");

        temp_rand = random2(8);

        strcat(info, (temp_rand == 0) ? "frog"          :
                     (temp_rand == 1) ? "pill bug"      :
                     (temp_rand == 2) ? "millipede"     :
                     (temp_rand == 3) ? "eggplant"      :
                     (temp_rand == 4) ? "albino dragon" :
                     (temp_rand == 5) ? "dragon"        :
                     (temp_rand == 6) ? "human"
                                      : "slug");

        strcat(info, "!");
        mpr(info);
        break;

    case 4:
        if (player_can_smell())
        {
            strcpy(info, "You smell ");

            temp_rand = random2(8);

            strcat(info, (temp_rand == 0) ? "coffee."          :
                         (temp_rand == 1) ? "salt."            :
                         (temp_rand == 2) ? "burning hair!"    :
                         (temp_rand == 3) ? "baking bread."    :
                         (temp_rand == 4) ? "something weird." :
                         (temp_rand == 5) ? "wet wool."        :
                         (temp_rand == 6) ? "sulphur."
                                          : "fire and brimstone!");
            mpr(info);
        }
        break;

    case 5:
        mpr("You experience a momentary feeling of inescapable doom!");
        break;

    case 6:
        temp_rand = random2(3);
        mprf("Your %s",
             (temp_rand == 0) ? "ears itch."   :
             (temp_rand == 1) ? "brain hurts!"
                              : "nose twitches suddenly!");
        break;

    case 7:
        mpr("You hear the tinkle of a tiny bell.");
        cast_summon_butterflies( 100 );
        break;

    case 8:
        strcpy(info, "You hear ");

        temp_rand = random2(9);

        strcat(info, (temp_rand == 0) ? "snatches of song"                 :
                     (temp_rand == 1) ? "a voice call someone else's name" :
                     (temp_rand == 2) ? "a very strange noise"             :
                     (temp_rand == 3) ? "roaring flame"                    :
                     (temp_rand == 4) ? "a very strange noise indeed"      :
                     (temp_rand == 5) ? "the chiming of a distant gong"    :
                     (temp_rand == 6) ? "the bellowing of a yak"           :
                     (temp_rand == 7) ? "a crunching sound"
                                      : "the tinkle of an enormous bell");
        strcat(info, ".");
        mpr(info);
        break;
    }

    return;
}                               // end random_uselessness()

bool acquirement(unsigned char force_class, int agent)
{
    int thing_created = 0;
    int iteration = 0;

    // Remember lava!
    unsigned char class_wanted = OBJ_RANDOM;
    unsigned char type_wanted = OBJ_RANDOM;

    unsigned char unique = 1;
    unsigned char acqc = 0;

    const int max_has_value = 100;
    FixedVector< int, max_has_value > already_has;

    char best_spell = 99;
    char best_any = 99;
    unsigned char keyin;

    for (acqc = 0; acqc < max_has_value; acqc++)
        already_has[acqc] = 0;

    int spell_skills = 0;
    for (int i = SK_SPELLCASTING; i <= SK_POISON_MAGIC; i++)
        spell_skills += you.skills[i];

    if (force_class == OBJ_RANDOM)
    {
        mpr("This is a scroll of acquirement!");

      query:
        mpr( "[a|A] Weapon  [b|B] Armour  [c|C] Jewellery      [d|D] Book" );
        mpr( "[e|E] Staff   [f|F] Food    [g|G] Miscellaneous  [h|H] Gold" );

        //mpr("[r|R] - Just give me something good.");
        mpr("What kind of item would you like to acquire? ", MSGCH_PROMPT);

        keyin = tolower( get_ch() );

        if (keyin == 'a')
            class_wanted = OBJ_WEAPONS;
        else if (keyin == 'b')
            class_wanted = OBJ_ARMOUR;
        else if (keyin == 'c')
            class_wanted = OBJ_JEWELLERY;
        else if (keyin == 'd')
            class_wanted = OBJ_BOOKS;
        else if (keyin == 'e')
            class_wanted = OBJ_STAVES;
        else if (keyin == 'f')
            class_wanted = OBJ_FOOD;
        else if (keyin == 'g')
            class_wanted = OBJ_MISCELLANY;
        else if (keyin == 'h')
            class_wanted = OBJ_GOLD;
    }
    else
        class_wanted = force_class;

    for (acqc = 0; acqc < ENDOFPACK; acqc++)
    {
        if (is_valid_item( you.inv[acqc] )
                && you.inv[acqc].base_type == class_wanted)
        {
            ASSERT( you.inv[acqc].sub_type < max_has_value );
            already_has[you.inv[acqc].sub_type] += you.inv[acqc].quantity;
        }
    }

    if (class_wanted == OBJ_FOOD)
    {
        // food is a little less predictable now -- bwr

        if (you.species == SP_GHOUL)
        {
            type_wanted = one_chance_in(10) ? FOOD_ROYAL_JELLY
                                            : FOOD_CHUNK;
        }
        else 
        {
            // Meat is better than bread (except for herbivores), and
            // by choosing it as the default we don't have to worry
            // about special cases for carnivorous races (ie kobold)
            type_wanted = FOOD_MEAT_RATION;

            if (you.mutation[MUT_HERBIVOROUS])
                type_wanted = FOOD_BREAD_RATION; 

            // If we have some regular rations, then we're probably be more
            // interested in faster foods (especially royal jelly)... 
            // otherwise the regular rations should be a good enough offer.
            if (already_has[FOOD_MEAT_RATION] 
                    + already_has[FOOD_BREAD_RATION] >= 2 || coinflip())
            {
                type_wanted = one_chance_in(5) ? FOOD_HONEYCOMB
                                               : FOOD_ROYAL_JELLY;
            }
        }

        // quantity is handled by unique for food
        unique = 3 + random2(5);

        // giving more of the lower food value items
        if (type_wanted == FOOD_HONEYCOMB || type_wanted == FOOD_CHUNK)
        {
            unique += random2avg(10, 2);
        }
    }
    else if (class_wanted == OBJ_WEAPONS)
    {
        // Now asking for a weapon is biased towards your skills,
        // although launchers are right out for now. -- bwr
        int count = 0;
        int skill = SK_FIGHTING;

        // Can't do much with launchers, so we'll avoid them for now -- bwr
        for (int i = SK_SHORT_BLADES; i < SK_DARTS; i++)
        {
            if (i == SK_UNUSED_1)
                continue;

            // Adding a small constant allows for the occasional
            // weapon in an untrained skill.

            const int weight = you.skills[i] + 1;
            count += weight;

            if (random2(count) < weight)
                skill = i;
        }

        count = 0;

        item_def item_considered;
        item_considered.base_type = OBJ_WEAPONS;
        for (int i = 0; i < NUM_WEAPONS; ++i)
        {
            item_considered.sub_type = i;

            const int acqweight = property(item_considered, PWPN_ACQ_WEIGHT);
            
            if (!acqweight)
                continue;

            int wskill = range_skill(OBJ_WEAPONS, i);
            if (wskill == SK_RANGED_COMBAT)
                wskill = weapon_skill(OBJ_WEAPONS, i);

            if (wskill == skill && random2(count += acqweight) < acqweight)
                type_wanted = i;
        }
    }
    else if (class_wanted == OBJ_MISSILES)
    {
        int count = 0;
        int skill = SK_RANGED_COMBAT;

        for (int i = SK_SLINGS; i <= SK_DARTS; i++)
        {
            if (you.skills[i])
            {
                count += you.skills[i];
                if (random2(count) < you.skills[i])
                    skill = i;
            }
        }

        switch (skill)
        {
        case SK_SLINGS:
            type_wanted = MI_STONE;
            break;

        case SK_BOWS:
            type_wanted = MI_ARROW;
            break;

        case SK_CROSSBOWS:
            type_wanted = MI_DART;
            for (int i = 0; i < ENDOFPACK; i++) 
            {
                // Assuming that crossbow in inventory means that they
                // want bolts for it (not darts for a hand crossbow)...
                // perhaps we should check for both and compare ammo
                // amounts on hand?
                if (is_valid_item( you.inv[i] )
                    && you.inv[i].base_type == OBJ_WEAPONS
                    && you.inv[i].sub_type == WPN_CROSSBOW)
                {
                    type_wanted = MI_BOLT;
                    break;
                }
            }
            break;

        case SK_DARTS:
            type_wanted = MI_DART;
            for (int i = 0; i < ENDOFPACK; i++) 
            {
                if (is_valid_item( you.inv[i] )
                    && you.inv[i].base_type == OBJ_WEAPONS
                    && you.inv[i].sub_type == WPN_BLOWGUN)
                {
                    // Assuming that blowgun in inventory means that they
                    // may want needles for it (but darts might also be
                    // wanted).  Maybe expand this... see above comment.
                    if (coinflip())
                        type_wanted = MI_NEEDLE;
                    break;
                }
            }
            break;

        default:
            type_wanted = MI_DART;
            break;
        }        
    }
    else if (class_wanted == OBJ_ARMOUR)
    {
        // Increasing the representation of the non-body armour
        // slots here to make up for the fact that there's one
        // one type of item for most of them. -- bwr
        //
        // OBJ_RANDOM is body armour and handled below
        type_wanted = (coinflip()) ? OBJ_RANDOM : ARM_SHIELD + random2(5);

        // mutation specific problems (horns allow caps)
        if ((you.mutation[MUT_HOOVES] && type_wanted == ARM_BOOTS)
            || (you.mutation[MUT_CLAWS] >= 3 && type_wanted == ARM_GLOVES))
        {
            type_wanted = OBJ_RANDOM;
        }

        // some species specific fitting problems
        switch (you.species)
        {
        case SP_OGRE:
        case SP_OGRE_MAGE:
        case SP_TROLL:
        case SP_RED_DRACONIAN:
        case SP_WHITE_DRACONIAN:
        case SP_GREEN_DRACONIAN:
        case SP_GOLDEN_DRACONIAN:
        case SP_GREY_DRACONIAN:
        case SP_BLACK_DRACONIAN:
        case SP_PURPLE_DRACONIAN:
        case SP_MOTTLED_DRACONIAN:
        case SP_PALE_DRACONIAN:
        case SP_UNK0_DRACONIAN:
        case SP_UNK1_DRACONIAN:
        case SP_BASE_DRACONIAN:
        case SP_SPRIGGAN:
            if (type_wanted == ARM_GLOVES || type_wanted == ARM_BOOTS)
            {
                type_wanted = ARM_ROBE;  // no heavy armour
            }
            else if (type_wanted == ARM_SHIELD)
            {
                if (you.species == SP_SPRIGGAN)
                    type_wanted = ARM_BUCKLER;
                else if (coinflip()) // giant races: 50/50 shield/large shield
                    type_wanted = ARM_LARGE_SHIELD;
            }
            else if (type_wanted == OBJ_RANDOM)
            {
                type_wanted = ARM_ROBE;  // no heavy armour, see below
            }
            break;

        case SP_KENKU:
            if (type_wanted == ARM_BOOTS)
                type_wanted = OBJ_RANDOM;
            break;

        default:
            break;
        }

        // Now we'll randomly pick a body armour (light only in the
        // case of ARM_ROBE).  Unlike before, now we're only giving
        // out the finished products here, never the hides.  -- bwr
        if (type_wanted == OBJ_RANDOM || type_wanted == ARM_ROBE)
        {
            // start with normal base armour
            if (type_wanted == ARM_ROBE) 
                type_wanted = coinflip() ? ARM_ROBE : ARM_ANIMAL_SKIN;
            else 
            {
                type_wanted = ARM_ROBE + random2(8);

                if (one_chance_in(10) && you.skills[SK_ARMOUR] >= 10)
                    type_wanted = ARM_CRYSTAL_PLATE_MAIL;

                if (one_chance_in(10))
                    type_wanted = ARM_ANIMAL_SKIN;
            }

            // everyone can wear things made from hides
            if (one_chance_in(20))
            {
                int rnd = random2(20);

                type_wanted = (rnd <  4) ? ARM_TROLL_LEATHER_ARMOUR  :  // 20%
                              (rnd <  8) ? ARM_STEAM_DRAGON_ARMOUR   :  // 20%
                              (rnd < 11) ? ARM_MOTTLED_DRAGON_ARMOUR :  // 15%
                              (rnd < 14) ? ARM_SWAMP_DRAGON_ARMOUR   :  // 15%
                              (rnd < 16) ? ARM_DRAGON_ARMOUR         :  // 10%
                              (rnd < 18) ? ARM_ICE_DRAGON_ARMOUR     :  // 10%
                              (rnd < 19) ? ARM_STORM_DRAGON_ARMOUR      //  5%
                                         : ARM_GOLD_DRAGON_ARMOUR;      //  5%
            }
        }
    }
    else if (class_wanted != OBJ_GOLD)
    {

        do
        {
            unsigned char i;

            switch (class_wanted)
            {
            case OBJ_JEWELLERY:
                // Try for a base type the player hasn't identified
                for (i = 0; i < 10; i++)
                {
                    type_wanted = random2(24);

                    if (one_chance_in(3))
                        type_wanted = AMU_RAGE + random2(10);

                    if (get_ident_type(OBJ_JEWELLERY, type_wanted) == ID_UNKNOWN_TYPE)
                    {
                        break;
                    }
                }
                break;

            case OBJ_BOOKS:
                // remember, put rarer books higher in the list
                iteration = 1;
                type_wanted = NUM_BOOKS;

                best_spell = best_skill( SK_SPELLCASTING, (NUM_SKILLS - 1), 
                                         best_spell );

              which_book:
#if DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "acquirement: iteration = %d, best_spell = %d",
                     iteration, best_spell );
#endif //jmf: debugging

                switch (best_spell)
                {
                default:
                case SK_SPELLCASTING:
                    if (you.skills[SK_SPELLCASTING] <= 3
                        && !you.had_book[BOOK_CANTRIPS])
                    {
                        // Handful of level one spells, very useful for the
                        // new spellcaster who's asking for a book -- bwr
                        type_wanted = BOOK_CANTRIPS;
                    }
                    else if (!you.had_book[BOOK_MINOR_MAGIC_I])
                        type_wanted = BOOK_MINOR_MAGIC_I + random2(3);
                    else if (!you.had_book[BOOK_WIZARDRY])
                        type_wanted = BOOK_WIZARDRY;
                    else if (!you.had_book[BOOK_CONTROL])
                        type_wanted = BOOK_CONTROL;
                    else if (!you.had_book[BOOK_POWER])
                        type_wanted = BOOK_POWER;
                    break;

                case SK_POISON_MAGIC:
                    if (!you.had_book[BOOK_YOUNG_POISONERS])
                        type_wanted = BOOK_YOUNG_POISONERS;
                    else if (!you.had_book[BOOK_ENVENOMATIONS])
                        type_wanted = BOOK_ENVENOMATIONS;
                    break;

                case SK_EARTH_MAGIC:
                    if (!you.had_book[BOOK_GEOMANCY])
                        type_wanted = BOOK_GEOMANCY;
                    else if (!you.had_book[BOOK_EARTH])
                        type_wanted = BOOK_EARTH;
                    break;

                case SK_AIR_MAGIC:
                    // removed the book of clouds... all the other elements
                    // (and most other spell skills) only get two.
                    if (!you.had_book[BOOK_AIR])
                        type_wanted = BOOK_AIR;
                    else if (!you.had_book[BOOK_SKY])
                        type_wanted = BOOK_SKY;
                    break;

                case SK_ICE_MAGIC:
                    if (!you.had_book[BOOK_FROST])
                        type_wanted = BOOK_FROST;
                    else if (!you.had_book[BOOK_ICE])
                        type_wanted = BOOK_ICE;
                    break;

                case SK_FIRE_MAGIC:
                    if (!you.had_book[BOOK_FLAMES])
                        type_wanted = BOOK_FLAMES;
                    else if (!you.had_book[BOOK_FIRE])
                        type_wanted = BOOK_FIRE;
                    break;

                case SK_SUMMONINGS:
                    if (!you.had_book[BOOK_CALLINGS])
                        type_wanted = BOOK_CALLINGS;
                    else if (!you.had_book[BOOK_SUMMONINGS])
                        type_wanted = BOOK_SUMMONINGS;

                    // now a Vehumet special -- bwr
                    // else if (!you.had_book[BOOK_DEMONOLOGY])
                    //     type_wanted = BOOK_DEMONOLOGY;
                    break;

                case SK_ENCHANTMENTS:
                    best_any = best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99);

                    // So many enchantment books!  I really can't feel
                    // guilty at all for dividing out the fighting
                    // books and forcing the player to raise a fighting
                    // skill (or enchantments in the case of Crusaders)
                    // to get the remaining books... enchantments are
                    // much too good (most spells, lots of books here,
                    // id wand charges, gives magic resistance),
                    // something will eventually have to be done.  -- bwr
                    if (best_any >= SK_FIGHTING
                                && best_any <= SK_STAVES)
                    {
                        // Fighter mages get the fighting enchantment books
                        if (!you.had_book[BOOK_WAR_CHANTS])
                            type_wanted = BOOK_WAR_CHANTS;
                        else if (!you.had_book[BOOK_TUKIMA])
                            type_wanted = BOOK_TUKIMA;
                    }
                    else if (!you.had_book[BOOK_CHARMS])
                        type_wanted = BOOK_CHARMS;
                    else if (!you.had_book[BOOK_HINDERANCE])
                        type_wanted = BOOK_HINDERANCE;
                    else if (!you.had_book[BOOK_ENCHANTMENTS])
                        type_wanted = BOOK_ENCHANTMENTS;
                    break;

                case SK_CONJURATIONS:
                    if (!you.had_book[BOOK_CONJURATIONS_I])
                        type_wanted = give_first_conjuration_book();
                    else if (!you.had_book[BOOK_TEMPESTS])
                        type_wanted = BOOK_TEMPESTS;
                    
                    // now a Vehumet special -- bwr
                    // else if (!you.had_book[BOOK_ANNIHILATIONS])
                    //     type_wanted = BOOK_ANNIHILATIONS;
                    break;

                case SK_NECROMANCY:
                    if (!you.had_book[BOOK_NECROMANCY])
                        type_wanted = BOOK_NECROMANCY;
                    else if (!you.had_book[BOOK_DEATH])
                        type_wanted = BOOK_DEATH;
                    else if (!you.had_book[BOOK_UNLIFE])
                        type_wanted = BOOK_UNLIFE;

                    // now a Kikubaaqudgha special -- bwr
                    // else if (!you.had_book[BOOK_NECRONOMICON])
                    //    type_wanted = BOOK_NECRONOMICON;
                    break;

                case SK_TRANSLOCATIONS:
                    if (!you.had_book[BOOK_SPATIAL_TRANSLOCATIONS])
                        type_wanted = BOOK_SPATIAL_TRANSLOCATIONS;
                    else if (!you.had_book[BOOK_WARP])
                        type_wanted = BOOK_WARP;
                    break;

                case SK_TRANSMIGRATION:
                    if (!you.had_book[BOOK_CHANGES])
                        type_wanted = BOOK_CHANGES;
                    else if (!you.had_book[BOOK_TRANSFIGURATIONS])
                        type_wanted = BOOK_TRANSFIGURATIONS;
                    else if (!you.had_book[BOOK_MUTATIONS])
                        type_wanted = BOOK_MUTATIONS;
                    break;

                case SK_DIVINATIONS:    //jmf: added 24mar2000
                    if (!you.had_book[BOOK_SURVEYANCES])
                        type_wanted = BOOK_SURVEYANCES;
                    else if (!you.had_book[BOOK_DIVINATIONS])
                        type_wanted = BOOK_DIVINATIONS;
                    break;
                }
/*
                if (type_wanted == 99 && glof == best_skill(SK_SPELLCASTING, (NUM_SKILLS - 1), 99))
*/
                if (type_wanted == NUM_BOOKS && iteration == 1)
                {
                    best_spell = best_skill( SK_SPELLCASTING, (NUM_SKILLS - 1),
                                             best_skill(SK_SPELLCASTING,
                                                        (NUM_SKILLS - 1), 99) );
                    iteration++;
                    goto which_book;
                }

                // if we don't have a book, try and get a new one.
                if (type_wanted == NUM_BOOKS)
                {
                    do
                    {
                        type_wanted = random2(NUM_BOOKS);
                        if (one_chance_in(500))
                            break;
                    }
                    while (you.had_book[type_wanted]);
                }

                // if the book is invalid find any valid one.
                while (book_rarity(type_wanted) == 100
                                       || type_wanted == BOOK_DESTRUCTION
                                       || type_wanted == BOOK_MANUAL)
                {
                    type_wanted = random2(NUM_BOOKS);
                }
                break;

            case OBJ_STAVES:
                type_wanted = random2(13);

                if (type_wanted >= 10)
                    type_wanted = STAFF_AIR + type_wanted - 10;

                // Elemental preferences -- bwr
                if (type_wanted == STAFF_FIRE || type_wanted == STAFF_COLD)
                {
                    if (you.skills[SK_FIRE_MAGIC] > you.skills[SK_ICE_MAGIC])
                        type_wanted = STAFF_FIRE;
                    else if (you.skills[SK_FIRE_MAGIC] != you.skills[SK_ICE_MAGIC])
                        type_wanted = STAFF_COLD;
                }
                else if (type_wanted == STAFF_AIR || type_wanted == STAFF_EARTH)
                {
                    if (you.skills[SK_AIR_MAGIC] > you.skills[SK_EARTH_MAGIC])
                        type_wanted = STAFF_AIR;
                    else if (you.skills[SK_AIR_MAGIC] != you.skills[SK_EARTH_MAGIC])
                        type_wanted = STAFF_EARTH;
                }

                best_spell = best_skill( SK_SPELLCASTING, (NUM_SKILLS-1), 99 );

                // If we're going to give out an enhancer stave, 
                // we should at least bias things towards the
                // best spell skill. -- bwr
                switch (best_spell)
                {
                case SK_FIRE_MAGIC:
                    if (!already_has[STAFF_FIRE])
                        type_wanted = STAFF_FIRE;
                    break;

                case SK_ICE_MAGIC:
                    if (!already_has[STAFF_COLD])
                        type_wanted = STAFF_COLD;
                    break;

                case SK_AIR_MAGIC:
                    if (!already_has[STAFF_AIR])
                        type_wanted = STAFF_AIR;
                    break;

                case SK_EARTH_MAGIC:
                    if (!already_has[STAFF_EARTH])
                        type_wanted = STAFF_EARTH;
                    break;

                case SK_POISON_MAGIC:
                    if (!already_has[STAFF_POISON])
                        type_wanted = STAFF_POISON;
                    break;

                case SK_NECROMANCY:
                    if (!already_has[STAFF_DEATH])
                        type_wanted = STAFF_DEATH;
                    break;

                case SK_CONJURATIONS:
                    if (!already_has[STAFF_CONJURATION])
                        type_wanted = STAFF_CONJURATION;
                    break;

                case SK_ENCHANTMENTS:
                    if (!already_has[STAFF_ENCHANTMENT])
                        type_wanted = STAFF_ENCHANTMENT;
                    break;

                case SK_SUMMONINGS:
                    if (!already_has[STAFF_SUMMONING])
                        type_wanted = STAFF_SUMMONING;
                    break;

                case SK_EVOCATIONS:
                    if (!one_chance_in(4))
                        type_wanted = STAFF_SMITING + random2(10);
                    break;

                default: // invocations and leftover spell schools
                    switch (random2(5))
                    {
                    case 0:
                        type_wanted = STAFF_WIZARDRY;
                        break;

                    case 1:
                        type_wanted = STAFF_POWER;
                        break;

                    case 2:
                        type_wanted = STAFF_ENERGY;
                        break;

                    case 3:
                        type_wanted = STAFF_CHANNELING;
                        break;

                    case 4:
                        break;
                    }
                    break;
                }

                // Increased chance of getting spell staff for new or
                // non-spellcasters.  -- bwr
                if (one_chance_in(20)
                    || (spell_skills <= 1               // short on spells
                        && (type_wanted < STAFF_SMITING // already making one
                            || type_wanted >= STAFF_AIR)
                        && !one_chance_in(4)))
                {
                    type_wanted = (coinflip() ? STAFF_STRIKING 
                                              : STAFF_SMITING + random2(10));
                }
                break;

            case OBJ_MISCELLANY: 
                do
                    type_wanted = random2(NUM_MISCELLANY);
                while (type_wanted == MISC_HORN_OF_GERYON
                    || type_wanted == MISC_RUNE_OF_ZOT
                    || type_wanted == MISC_PORTABLE_ALTAR_OF_NEMELEX
                    || type_wanted == MISC_CRYSTAL_BALL_OF_FIXATION
                    || type_wanted == MISC_EMPTY_EBONY_CASKET);
                break;

            default:
                mesclr();
                goto query;
            }

            ASSERT( type_wanted < max_has_value );
        }
        while (already_has[type_wanted] && !one_chance_in(200));
    }

    if (grid_destroys_items(grd[you.x_pos][you.y_pos]))
    {
        // how sad (and stupid)
        mprf(MSGCH_SOUND, 
             grid_item_destruction_message(grd[you.x_pos][you.y_pos]));
    }
    else
    {
        randart_properties_t  proprt;

        for (int item_tries = 0; item_tries < 50; item_tries++)
        {
            // BCR - unique is now used for food quantity.
            thing_created = items( unique, class_wanted, type_wanted, true, 
                    MAKE_GOOD_ITEM, 250 );

            if (thing_created == NON_ITEM)
                continue;

            // MT - Check: god-gifted weapons and armor shouldn't kill you.
            // Except Xom.
            if ((agent == GOD_TROG || agent == GOD_OKAWARU)
                && is_random_artefact(mitm[thing_created]))
            {
                randart_wpn_properties( mitm[thing_created], proprt );

                // check vs stats. positive stats will automatically fall
                // through.  As will negative stats that won't kill you.
                if (-proprt[RAP_STRENGTH] >= you.strength ||
                    -proprt[RAP_INTELLIGENCE] >= you.intel ||
                    -proprt[RAP_DEXTERITY] >= you.dex)
                {
                    // try again
                    destroy_item(thing_created);
                    thing_created = NON_ITEM;
                    continue;
                }
            }
            break;
        }

        if (thing_created == NON_ITEM)
        {
            mpr("The demon of the infinite void smiles at you.");
            return (false);
        }

        // easier to read this way
        item_def& thing(mitm[thing_created]);

        // give some more gold
        if ( class_wanted == OBJ_GOLD )
            thing.quantity += 150;
    
        // remove curse flag from item
        do_uncurse_item( thing );

        if (thing.base_type == OBJ_BOOKS)
        {
            if (thing.base_type == BOOK_MINOR_MAGIC_I
                || thing.base_type == BOOK_MINOR_MAGIC_II
                || thing.base_type == BOOK_MINOR_MAGIC_III)
            {
                you.had_book[ BOOK_MINOR_MAGIC_I ] = 1;    
                you.had_book[ BOOK_MINOR_MAGIC_II ] = 1;    
                you.had_book[ BOOK_MINOR_MAGIC_III ] = 1;    
            }
            else if (thing.base_type == BOOK_CONJURATIONS_I
                || thing.base_type == BOOK_CONJURATIONS_II)
            {
                you.had_book[ BOOK_CONJURATIONS_I ] = 1;    
                you.had_book[ BOOK_CONJURATIONS_II ] = 1;    
            }
            else
            {
                you.had_book[ thing.sub_type ] = 1;    
            }
        }
        else if (thing.base_type == OBJ_JEWELLERY)
        {
            switch (thing.sub_type)
            {
            case RING_SLAYING:
                // make sure plus to damage is >= 1
                thing.plus2 = abs( thing.plus2 );
                if (thing.plus2 == 0)
                    thing.plus2 = 1;
                // fall through... 

            case RING_PROTECTION:
            case RING_STRENGTH:
            case RING_INTELLIGENCE:
            case RING_DEXTERITY:
            case RING_EVASION:
                // make sure plus is >= 1
                thing.plus = abs( thing.plus );
                if (thing.plus == 0)
                    thing.plus = 1;
                break;

            case RING_HUNGER:
            case AMU_INACCURACY:
                // these are the only truly bad pieces of jewellery
                if (!one_chance_in(9))
                    make_item_randart( thing );
                break;

            default:
                break;
            }
        }
        else if (thing.base_type == OBJ_WEAPONS
                    && !is_fixed_artefact( thing ))
        {
            // HACK: make unwieldable weapons wieldable 
            // Note: messing with fixed artefacts is probably very bad.
            switch (you.species)
            {
            case SP_DEMONSPAWN:
            case SP_MUMMY:
            case SP_GHOUL:
                {
                    int brand = get_weapon_brand( thing );
                    if (brand == SPWPN_HOLY_WRATH 
                            || brand == SPWPN_DISRUPTION)
                    {
                        if (!is_random_artefact( thing ))
                        {
                            set_item_ego_type( thing, 
                                               OBJ_WEAPONS, SPWPN_VORPAL );
                        }
                        else
                        {
                            // keep resetting seed until it's good:
                            for (; brand == SPWPN_HOLY_WRATH 
                                      || brand == SPWPN_DISRUPTION; 
                                  brand = get_weapon_brand(thing))
                            {
                                make_item_randart( thing );    
                            }
                        }
                    }
                }
                break;

            case SP_HALFLING:
            case SP_GNOME:
            case SP_KOBOLD:
            case SP_SPRIGGAN:
                switch (thing.sub_type)
                {
                case WPN_LONGBOW:
                    thing.sub_type = WPN_BOW;
                    break;

                case WPN_GREAT_SWORD:
                case WPN_TRIPLE_SWORD:
                    thing.sub_type = 
                            (coinflip() ? WPN_FALCHION : WPN_LONG_SWORD);
                    break;

                case WPN_GREAT_MACE:
                case WPN_DIRE_FLAIL:
                    thing.sub_type = 
                            (coinflip() ? WPN_MACE : WPN_FLAIL);
                    break;

                case WPN_BATTLEAXE:
                case WPN_EXECUTIONERS_AXE:
                    thing.sub_type = 
                            (coinflip() ? WPN_HAND_AXE : WPN_WAR_AXE);
                    break;

                case WPN_HALBERD:
                case WPN_GLAIVE:
                case WPN_SCYTHE:
                case WPN_LOCHABER_AXE:
                    thing.sub_type = 
                            (coinflip() ? WPN_SPEAR : WPN_TRIDENT);
                    break;
                }
                break;

            default:
                break;
            }
        }
        else if (thing.base_type == OBJ_ARMOUR
                    && !is_fixed_artefact( thing ))
        {
            // HACK: make unwearable hats and boots wearable
            // Note: messing with fixed artefacts is probably very bad.
            switch (thing.sub_type)
            {
            case ARM_HELMET:
                if ((get_helmet_type(thing) == THELM_HELM
                        || get_helmet_type(thing) == THELM_HELMET)
                    && ((you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE)
                        || player_genus(GENPC_DRACONIAN)
                        || you.species == SP_KENKU
                        || you.species == SP_SPRIGGAN
                        || you.mutation[MUT_HORNS]))
                {
                    // turn it into a cap or wizard hat
                    set_helmet_type(thing, 
                                    coinflip() ? THELM_CAP : THELM_WIZARD_HAT);

                    thing.colour = random_colour();
                }
                break;

            case ARM_BOOTS:
                if (you.species == SP_NAGA)
                    thing.sub_type = ARM_NAGA_BARDING;
                else if (you.species == SP_CENTAUR)
                    thing.sub_type = ARM_CENTAUR_BARDING;

                // fix illegal barding ego types caused by above hack
                if (thing.sub_type != ARM_BOOTS &&
                    get_armour_ego_type(thing) == SPARM_RUNNING)
                {
                    set_item_ego_type( thing, OBJ_ARMOUR, SPARM_NORMAL );
                }
                break;

            case ARM_NAGA_BARDING:
            case ARM_CENTAUR_BARDING:
                // make barding appropriate
                if (you.species == SP_NAGA )
                    thing.sub_type = ARM_NAGA_BARDING;
                else if ( you.species == SP_CENTAUR )
                    thing.sub_type = ARM_CENTAUR_BARDING;
                else {
                    thing.sub_type = ARM_BOOTS;
                    // Fix illegal ego types
                    if (get_armour_ego_type(thing) == SPARM_COLD_RESISTANCE ||
                        get_armour_ego_type(thing) == SPARM_FIRE_RESISTANCE)
                        set_item_ego_type(thing, OBJ_ARMOUR, SPARM_NORMAL);
                }
                break;

            default:
                break;
            }
        }

        move_item_to_grid( &thing_created, you.x_pos, you.y_pos );

        // This should never actually be NON_ITEM because of the way
        // move_item_to_grid works (doesn't create a new item ever), 
        // but we're checking it anyways. -- bwr
        if (thing_created != NON_ITEM)
        {
            canned_msg(MSG_SOMETHING_APPEARS);
            origin_acquired(mitm[thing_created], agent);
        }
    }

    // Well, the item may have fallen in the drink, but the intent is 
    // that acquirement happened. -- bwr
    return (true);
}                               // end acquirement()

// Remove the "empty" autoinscription, if present.
// Return true if the inscription was there, false otherwise.
bool remove_empty_inscription( item_def& item )
{
    const char* empty_inscription = "empty";
    size_t p = item.inscription.find(empty_inscription);
    if ( p != std::string::npos )
    {
        // found it, delete it
        size_t prespace = 0;
        // if preceded by a space, delete that too
        if ( p != 0 && item.inscription[p-1] == ' ' )
            prespace = 1;
        item.inscription =
            item.inscription.substr(0, p - prespace) +
            item.inscription.substr(p + strlen(empty_inscription),
                                    std::string::npos);
        return true;
    }
    else
        return false;
}

bool recharge_wand(void)
{
    if (you.equip[EQ_WEAPON] == -1)
        return (false);

    item_def &wand = you.inv[ you.equip[EQ_WEAPON] ];

    if (wand.base_type != OBJ_WANDS && !item_is_rod(wand))
        return (false);

    int charge_gain = 0;
    if (wand.base_type == OBJ_WANDS)
    {
        remove_empty_inscription(wand);
        switch (wand.sub_type)
        {
        case WAND_INVISIBILITY:
        case WAND_FIREBALL:
        case WAND_HEALING:
        case WAND_HASTING:
            charge_gain = 3;
            break;

        case WAND_LIGHTNING:
        case WAND_DRAINING:
            charge_gain = 4;
            break;

        case WAND_FIRE:
        case WAND_COLD:
            charge_gain = 5;
            break;

        default:
            charge_gain = 8;
            break;
        }

        mprf("%s glows for a moment.", wand.name(DESC_CAP_YOUR).c_str());

        wand.plus += 1 + random2avg( ((charge_gain - 1) * 3) + 1, 3 );

        if (wand.plus > charge_gain * 3)
            wand.plus = charge_gain * 3;
    }
    else 
    {
        // This is a rod.
        bool work = false;

        if (wand.plus2 <= MAX_ROD_CHARGE * ROD_CHARGE_MULT)
        {
            wand.plus2 += ROD_CHARGE_MULT;

            if (wand.plus2 > MAX_ROD_CHARGE * ROD_CHARGE_MULT)
                wand.plus2 = MAX_ROD_CHARGE * ROD_CHARGE_MULT;

            work = true;
        }

        if (wand.plus < wand.plus2)
        {
            wand.plus = wand.plus2;
            work = true;
        }

        if (!work) 
            return (false);

        mprf("%s glows for a moment.", wand.name(DESC_CAP_YOUR).c_str());
    }

    you.wield_change = true;
    return (true);
}                               // end recharge_wand()

void yell(void)
{
    bool targ_prev = false;
    int mons_targd = MHITNOT;
    struct dist targ;

    if (silenced(you.x_pos, you.y_pos) || you.cannot_speak())
    {
        mpr("You are unable to make a sound!");
        return;
    }

    const std::string shout_verb = you.shout_verb();
    std::string cap_shout = shout_verb;
    cap_shout[0] = toupper(cap_shout[0]);

    int noise_level = 12;

    // Tweak volume for different kinds of vocalisation.
    if (shout_verb == "roar")
        noise_level = 18;
    else if (shout_verb == "hiss")
        noise_level = 8;

    mpr("What do you say?", MSGCH_PROMPT);
    mprf(" ! - %s", cap_shout.c_str());
    mpr(" a - Order allies to attack a monster");

    if (!(you.prev_targ == MHITNOT || you.prev_targ == MHITYOU))
    {
        struct monsters *target = &menv[you.prev_targ];

        if (mons_near(target) && player_monster_visible(target))
        {
            mpr(" p - Order allies to attack your previous target");
            targ_prev = true;
        }
    }

    mprf(" Anything else - Stay silent%s",
         one_chance_in(20)? " (and be thought a fool)" : "");

    unsigned char keyn = get_ch();

    switch (keyn)
    {
    case '!':
        mprf(MSGCH_SOUND, "You %s for attention!", shout_verb.c_str());
        you.turn_is_over = true;
        noisy( noise_level, you.x_pos, you.y_pos );
        return;

    case 'a':
        mpr("Gang up on whom?", MSGCH_PROMPT);
        direction( targ, DIR_TARGET, TARG_ENEMY );

        if (targ.isCancel)
        {
            canned_msg(MSG_OK);
            return;
        }

        if (!targ.isValid || mgrd[targ.tx][targ.ty] == NON_MONSTER)
        {
            mpr("Yeah, whatever.");
            return;
        }

        mons_targd = mgrd[targ.tx][targ.ty];
        break;

    case 'p':
        if (targ_prev)
        {
            mons_targd = you.prev_targ;
            break;
        }
        /* fall through... */
    default:
        mpr("Okely-dokely.");
        return;
    }

    you.pet_target = mons_targd;

    noisy( 10, you.x_pos, you.y_pos );
    mpr("Attack!");
}                               // end yell()
