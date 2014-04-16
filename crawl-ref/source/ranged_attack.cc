/**
 * @file
 * @brief ranged_attack class and associated ranged_attack methods
 */

#include "AppHdr.h"

#include "ranged_attack.h"

ranged_attack::ranged_attack(actor *attk, actor *defn, item_def *proj) :
                             ::attack(attk, defn), projectile(proj)
{
    range_used = 0;
}

bool ranged_attack::attack()
{
    return false;
}

int ranged_attack::calc_to_hit(bool)
{
    return 0;
}

bool ranged_attack::handle_phase_attempted()
{
    return false;
}

bool ranged_attack::handle_phase_dodged()
{
    return false;
}

bool ranged_attack::handle_phase_blocked()
{
    return false;
}

bool ranged_attack::handle_phase_hit()
{
    return false;
}

bool ranged_attack::handle_phase_damaged()
{
    return false;
}

bool ranged_attack::handle_phase_killed()
{
    return false;
}

bool ranged_attack::handle_phase_end()
{
    return false;
}

bool ranged_attack::attack_shield_blocked(bool verbose)
{
    return false;
}

bool ranged_attack::apply_damage_brand()
{
    return false;
}

bool ranged_attack::check_unrand_effects()
{
    return false;
}
