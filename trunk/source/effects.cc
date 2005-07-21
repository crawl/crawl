/*
 *  File:       effects.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "effects.h"

#include <string.h>
#include <stdio.h>

#include "externs.h"

#include "beam.h"
#include "direct.h"
#include "dungeon.h"
#include "itemname.h"
#include "items.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "newgame.h"
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
#include "wpn-misc.h"

// torment_monsters is called with power 0 because torment is
// UNRESISTABLE except for being undead or having torment
// resistance!   Even if we used maximum power of 1000,  high
// level monsters and characters would save too often.  (GDL)

static int torment_monsters(int x, int y, int pow, int garbage)
{
    UNUSED( pow );
    UNUSED( garbage );

    // is player?
    if (x == you.x_pos && y == you.y_pos)
    {
        if (you.is_undead || you.mutation[MUT_TORMENT_RESISTANCE])
            mpr("You feel a surge of unholy energy.");
        else
        {
            mpr("Your body is wracked with pain!");
            dec_hp((you.hp / 2) - 1, false);
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

void torment(int tx, int ty)
{
    apply_area_within_radius(torment_monsters, tx, ty, 0, 8, 0);
}                               // end torment()

void banished(unsigned char gate_type)
{
    you_teleport2( false );

    // this is to ensure that you're standing on a suitable space (67)
    grd[you.x_pos][you.y_pos] = gate_type;

    down_stairs(true, you.your_level);  // heh heh
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
        mpr( "You are engulfed in a burst of hellfire!" );
        strcpy( pbolt.beam_name, "hellfire" );
        pbolt.ex_size = 1;
        pbolt.flavour = BEAM_EXPLOSION;
        pbolt.type = SYM_ZAP;
        pbolt.colour = RED;
        pbolt.thrower = KILL_MON_MISSILE;
        pbolt.aux_source = NULL;
        pbolt.isBeam = false;
        pbolt.isTracer = false;
        pbolt.hit = 20;
        pbolt.damage = dice_def( 3, 20 );
        pbolt.aux_source = "burst of hellfire";
        explosion( pbolt );
        break;

    case DMNBM_SMITING:
        mpr( "Something smites you!" );
        strcpy( pbolt.beam_name, "smiting" );    // for ouch
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

    case DMNBM_MUTATION:
        mpr("Strange energies course through your body.");
        if (one_chance_in(5))
            mutate(100);
        else
            give_bad_mutation();
        break;
    }

    // apply damage and handle death, where appropriate {dlb}
    if (damage_taken > 0)
        ouch(damage_taken, pbolt.beam_source, KILLED_BY_BEAM, pbolt.aux_source);

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
        strcpy(pbolt.beam_name, "hellfire");
        pbolt.flavour = BEAM_LAVA;

        damage_taken = 5 + random2(10) + random2(5);
        damage_taken = mons_adjust_flavoured(monster, pbolt, damage_taken);
        break;

    case DMNBM_SMITING:
        simple_monster_message(monster, " is smitten.");
        strcpy(pbolt.beam_name, "smiting");
        pbolt.flavour = BEAM_MISSILE;

        damage_taken += 7 + random2avg(11, 2);
        break;

    case DMNBM_BRAIN_FEED:      // not implemented here (nor, probably, can be)
        break;

    case DMNBM_MUTATION:
        if (mons_holiness( monster->type ) != MH_NATURAL)
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
        strcpy(info, "The dust glows a ");
        weird_colours(random2(256), wc);
        strcat(info, wc);
        strcat(info, " colour!");
        mpr(info);
        break;

    case 1:
        mpr("The scroll reassembles itself in your hand!");
        inc_inv_item_quantity( sc_read_2, 1 );
        break;

    case 2:
        if (you.equip[EQ_WEAPON] != -1)
        {
            char str_pass[ ITEMNAME_SIZE ];
            in_name(you.equip[EQ_WEAPON], DESC_CAP_YOUR, str_pass);
            strcpy(info, str_pass);
            strcat(info, " glows ");
            weird_colours(random2(256), wc);
            strcat(info, wc);
            strcat(info, " for a moment.");
            mpr(info);
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
        // josh declares mummies can't smell {dlb}
        if (you.species != SP_MUMMY)
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
        strcpy(info, "Your ");

        temp_rand = random2(3);

        strcat(info, (temp_rand == 0) ? "ears itch."   :
                     (temp_rand == 1) ? "brain hurts!"
                                      : "nose twitches suddenly!");
        mpr(info);
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

bool acquirement(unsigned char force_class)
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

        keyin = get_ch();

        if (keyin == 'a' || keyin == 'A')
            class_wanted = OBJ_WEAPONS;
        else if (keyin == 'b' || keyin == 'B')
            class_wanted = OBJ_ARMOUR;
        else if (keyin == 'c' || keyin == 'C')
            class_wanted = OBJ_JEWELLERY;
        else if (keyin == 'd' || keyin == 'D')
            class_wanted = OBJ_BOOKS;
        else if (keyin == 'e' || keyin == 'E')
            class_wanted = OBJ_STAVES;
        else if (keyin == 'f' || keyin == 'F')
            class_wanted = OBJ_FOOD;
        else if (keyin == 'g' || keyin == 'G')
            class_wanted = OBJ_MISCELLANY;
        else if (keyin == 'h' || keyin == 'H')
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
        // food is a little less predicatable now -- bwr

        if (you.species == SP_GHOUL)
        {
            type_wanted = one_chance_in(10) ? FOOD_ROYAL_JELLY
                                            : FOOD_CHUNK;
        }
        else 
        {
            // Meat is better than bread (except for herbivors), and
            // by choosing it as the default we don't have to worry
            // about special cases for carnivorous races (ie kobold)
            type_wanted = FOOD_MEAT_RATION;

            if (you.mutation[MUT_HERBIVOROUS])
                type_wanted = FOOD_BREAD_RATION; 

            // If we have some regular rations, then we're probably be more
            // interested in faster foods (escpecially royal jelly)... 
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
        for (int i = SK_SHORT_BLADES; i <= SK_STAVES; i++)
        {
            if (i == SK_UNUSED_1)
                continue;

            // Adding a small constant allows for the occasional
            // weapon in an untrained skill.
            count += (you.skills[i] + 1);

            if (random2(count) < you.skills[i] + 1)
                skill = i;
        }

        if (skill == SK_STAVES)
            type_wanted = WPN_QUARTERSTAFF;    // only one in this class
        else
        {
            count = 0;

            // skipping clubs, knives, blowguns
            for (int i = WPN_MACE; i < NUM_WEAPONS; i++)
            {
                // skipping launchers
                if (i == WPN_SLING)
                    i = WPN_GLAIVE;

                // skipping giant clubs
                if (i == WPN_GIANT_CLUB)
                    i = WPN_EVENINGSTAR;

                // skipping knife and blowgun
                if (i == WPN_KNIFE)
                    i = WPN_FALCHION;

                // "rare" weapons are only considered some of the time...
                // still, the chance is higher than actual random creation
                if (weapon_skill( OBJ_WEAPONS, i ) == skill
                    && (i < WPN_EVENINGSTAR || i > WPN_BROAD_AXE 
                        || (i >= WPN_HAMMER && i <= WPN_SABRE) 
                        || one_chance_in(4)))
                {
                    count++;
                    if (one_chance_in( count ))
                        type_wanted = i;
                }
            }
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
        case SP_SPRIGGAN:
            if (type_wanted == ARM_GLOVES || type_wanted == ARM_BOOTS)
            {
                type_wanted = OBJ_RANDOM;
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
                snprintf( info, INFO_SIZE, 
                          "acquirement: iteration = %d, best_spell = %d",
                          iteration, best_spell );

                mpr( info, MSGCH_DIAGNOSTICS );
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
                        // Fighter mage's get the fighting enchantment books
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

    if (grd[you.x_pos][you.y_pos] == DNGN_LAVA
                        || grd[you.x_pos][you.y_pos] == DNGN_DEEP_WATER)
    {
        mpr("You hear a splash.");      // how sad (and stupid)
    }
    else
    {
        // BCR - unique is now used for food quantity.
        thing_created = items( unique, class_wanted, type_wanted, true, 
                               MAKE_GOOD_ITEM, 250 );

        if (thing_created == NON_ITEM)
        {
            mpr("The demon of the infinite void smiles at you.");
            return (false);
        }

        // remove curse flag from item
        do_uncurse_item( mitm[thing_created] );

        if (mitm[thing_created].base_type == OBJ_BOOKS)
        {
            if (mitm[thing_created].base_type == BOOK_MINOR_MAGIC_I
                || mitm[thing_created].base_type == BOOK_MINOR_MAGIC_II
                || mitm[thing_created].base_type == BOOK_MINOR_MAGIC_III)
            {
                you.had_book[ BOOK_MINOR_MAGIC_I ] = 1;    
                you.had_book[ BOOK_MINOR_MAGIC_II ] = 1;    
                you.had_book[ BOOK_MINOR_MAGIC_III ] = 1;    
            }
            else if (mitm[thing_created].base_type == BOOK_CONJURATIONS_I
                || mitm[thing_created].base_type == BOOK_CONJURATIONS_II)
            {
                you.had_book[ BOOK_CONJURATIONS_I ] = 1;    
                you.had_book[ BOOK_CONJURATIONS_II ] = 1;    
            }
            else
            {
                you.had_book[ mitm[thing_created].sub_type ] = 1;    
            }
        }
        else if (mitm[thing_created].base_type == OBJ_JEWELLERY)
        {
            switch (mitm[thing_created].sub_type)
            {
            case RING_SLAYING:
                // make sure plus to damage is >= 1
                mitm[thing_created].plus2 = abs( mitm[thing_created].plus2 );
                if (mitm[thing_created].plus2 == 0)
                    mitm[thing_created].plus2 = 1;
                // fall through... 

            case RING_PROTECTION:
            case RING_STRENGTH:
            case RING_INTELLIGENCE:
            case RING_DEXTERITY:
            case RING_EVASION:
                // make sure plus is >= 1
                mitm[thing_created].plus = abs( mitm[thing_created].plus );
                if (mitm[thing_created].plus == 0)
                    mitm[thing_created].plus = 1;
                break;

            case RING_HUNGER:
            case AMU_INACCURACY:
                // these are the only truly bad pieces of jewellery
                if (!one_chance_in(9))
                    make_item_randart( mitm[thing_created] );
                break;

            default:
                break;
            }
        }
        else if (mitm[thing_created].base_type == OBJ_WEAPONS
                    && !is_fixed_artefact( mitm[thing_created] ))
        {
            // HACK: make unwieldable weapons wieldable 
            // Note: messing with fixed artefacts is probably very bad.
            switch (you.species)
            {
            case SP_DEMONSPAWN:
            case SP_MUMMY:
            case SP_GHOUL:
                {
                    int brand = get_weapon_brand( mitm[thing_created] );
                    if (brand == SPWPN_HOLY_WRATH 
                            || brand == SPWPN_DISRUPTION)
                    {
                        if (!is_random_artefact( mitm[thing_created] ))
                        {
                            set_item_ego_type( mitm[thing_created], 
                                               OBJ_WEAPONS, SPWPN_VORPAL );
                        }
                        else
                        {
                            // keep reseting seed until it's good:
                            for (; brand == SPWPN_HOLY_WRATH 
                                      || brand == SPWPN_DISRUPTION; 
                                  brand = get_weapon_brand(mitm[thing_created]))
                            {
                                make_item_randart( mitm[thing_created] );    
                            }
                        }
                    }
                }
                break;

            case SP_HALFLING:
            case SP_GNOME:
            case SP_KOBOLD:
            case SP_SPRIGGAN:
                switch (mitm[thing_created].sub_type)
                {
                case WPN_GREAT_SWORD:
                case WPN_TRIPLE_SWORD:
                    mitm[thing_created].sub_type = 
                            (coinflip() ? WPN_FALCHION : WPN_LONG_SWORD);
                    break;

                case WPN_GREAT_MACE:
                case WPN_GREAT_FLAIL:
                    mitm[thing_created].sub_type = 
                            (coinflip() ? WPN_MACE : WPN_FLAIL);
                    break;

                case WPN_BATTLEAXE:
                case WPN_EXECUTIONERS_AXE:
                    mitm[thing_created].sub_type = 
                            (coinflip() ? WPN_HAND_AXE : WPN_WAR_AXE);
                    break;

                case WPN_HALBERD:
                case WPN_GLAIVE:
                case WPN_SCYTHE:
                    mitm[thing_created].sub_type = 
                            (coinflip() ? WPN_SPEAR : WPN_TRIDENT);
                    break;
                }
                break;

            default:
                break;
            }
        }
        else if (mitm[thing_created].base_type == OBJ_ARMOUR
                    && !is_fixed_artefact( mitm[thing_created] ))
        {
            // HACK: make unwearable hats and boots wearable
            // Note: messing with fixed artefacts is probably very bad.
            switch (mitm[thing_created].sub_type)
            {
            case ARM_HELMET:
                if ((cmp_helmet_type( mitm[thing_created], THELM_HELM )
                        || cmp_helmet_type( mitm[thing_created], THELM_HELMET ))
                    && ((you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE)
                        || player_genus(GENPC_DRACONIAN)
                        || you.species == SP_MINOTAUR
                        || you.species == SP_KENKU
                        || you.species == SP_SPRIGGAN
                        || you.mutation[MUT_HORNS]))
                {
                    // turn it into a cap or wizard hat
                    set_helmet_type( mitm[thing_created], 
                                    coinflip() ? THELM_CAP : THELM_WIZARD_HAT );

                    mitm[thing_created].colour = random_colour();
                }
                break;

            case ARM_BOOTS:
                if (you.species == SP_NAGA)
                    mitm[thing_created].plus2 = TBOOT_NAGA_BARDING;
                else if (you.species == SP_CENTAUR)
                    mitm[thing_created].plus2 = TBOOT_CENTAUR_BARDING;
                else 
                    mitm[thing_created].plus2 = TBOOT_BOOTS;

                // fix illegal barding ego types caused by above hack
                if (mitm[thing_created].plus2 != TBOOT_BOOTS
                    && get_armour_ego_type( mitm[thing_created] ) == SPARM_RUNNING)
                {
                    set_item_ego_type( mitm[thing_created], OBJ_ARMOUR, SPARM_NORMAL );
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
            canned_msg(MSG_SOMETHING_APPEARS);
    }

    // Well, the item may have fallen in the drink, but the intent is 
    // that acquirement happened. -- bwr
    return (true);
}                               // end acquirement()

bool recharge_wand(void)
{
    if (you.equip[EQ_WEAPON] == -1
            || you.inv[you.equip[EQ_WEAPON]].base_type != OBJ_WANDS)
    {
        return (false);
    }

    unsigned char charge_gain = 0;

    switch (you.inv[you.equip[EQ_WEAPON]].sub_type)
    {
    case WAND_INVISIBILITY:
    case WAND_FIREBALL:
    case WAND_HEALING:
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

    char str_pass[ ITEMNAME_SIZE ];
    in_name(you.equip[EQ_WEAPON], DESC_CAP_YOUR, str_pass);
    strcpy(info, str_pass);
    strcat(info, " glows for a moment.");
    mpr(info);

    you.inv[you.equip[EQ_WEAPON]].plus +=
                            1 + random2avg( ((charge_gain - 1) * 3) + 1, 3 );

    if (you.inv[you.equip[EQ_WEAPON]].plus > charge_gain * 3)
        you.inv[you.equip[EQ_WEAPON]].plus = charge_gain * 3;

    you.wield_change = true;
    return (true);
}                               // end recharge_wand()

void yell(void)
{
    bool targ_prev = false;
    int mons_targd = MHITNOT;
    struct dist targ;

    if (silenced(you.x_pos, you.y_pos))
    {
        mpr("You are unable to make a sound!");
        return;
    }

    mpr("What do you say?", MSGCH_PROMPT);
    mpr(" ! - Yell");
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

    strcpy(info, " Anything else - Stay silent");

    if (one_chance_in(20))
        strcat(info, " (and be thought a fool)");

    mpr(info);

    unsigned char keyn = get_ch();

    switch (keyn)
    {
    case '!':
        mpr("You yell for attention!");
        you.turn_is_over = 1;
        noisy( 12, you.x_pos, you.y_pos );
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
