/**
 * @file
 * @brief Stuff related to conducts.
**/

#ifndef GODCONDUCT_H
#define GODCONDUCT_H

// Calls did_god_conduct() when the object goes out of scope.
struct god_conduct_trigger
{
    conduct_type conduct;
    int pgain;
    bool known;
    bool enabled;
    unique_ptr<monster> victim;

    god_conduct_trigger(conduct_type c = NUM_CONDUCTS,
                        int pg = 0,
                        bool kn = true,
                        const monster* vict = NULL);

    void set(conduct_type c = NUM_CONDUCTS,
             int pg = 0,
             bool kn = true,
             const monster* vict = NULL);

    ~god_conduct_trigger();
};

void did_god_conduct(conduct_type thing_done, int level, bool known = true,
                     const monster* victim = NULL);
void set_attack_conducts(god_conduct_trigger conduct[3], const monster* mon,
                         bool known = true);
void enable_attack_conducts(god_conduct_trigger conduct[3]);
void disable_attack_conducts(god_conduct_trigger conduct[3]);

void god_conduct_turn_start();

#endif
