/**
 * @file
 * @brief Potion and potion-like effects.
**/

#ifndef POTION_H
#define POTION_H

static const string POTION_QUEUE_KEY = "potion_queue"; // semipermeable skin

class PotionEffect
{
private:
    PotionEffect() = delete;
    DISALLOW_COPY_AND_ASSIGN(PotionEffect);
protected:
    PotionEffect(potion_type);
    bool check_known_quaff() const;
    void begin_quaff(bool was_known) const;
public:
    virtual bool can_quaff(string *reason = nullptr) const;

    /**
     * Elsewhere in the code there are things that can have the effect
     * effect of a potion without actually being potions, there
     * are even some legacy 'potions' around in this code that
     * nowadays only provide the potion-like effect.
     *
     * Doesn't currently handle conducts, but possibly it should.
     *
     * @param was_known     Whether the player should be held responsible.
     * @param pow           The 'power' of the effect. Mostly disused.
     * @param is_potion     Whether to apply the effects of MUT_NO_POTION_HEAL.
     * @return              Whether or not the potion had an effect.
     */
    virtual bool effect(bool was_known = true, int pow = 40,
                        bool is_potion = true) const = 0;

    /**
     * Effects that should occur after quaffing an actual potion.
     * XXX: can this be moved into effect()?
     */
    virtual void post_quaff_effect(bool was_known) const { };

    // Quaff also handles god-conduct and potion-specific
    // uncancellability
    // Returns whether or not the potion was actually quaffed
    virtual bool quaff(bool was_known) const;

    const string potion_name;
    const potion_type kind;
};

const PotionEffect* get_potion_effect(potion_type pot);

bool quaff_potion(item_def &potion);
void potionlike_effect(potion_type effect, int pow, bool was_known = true);

#endif
