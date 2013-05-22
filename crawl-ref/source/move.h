/**
 * @file
 * @brief Abstracted movement
**/

#ifndef MOVE_H
#define MOVE_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <cmath>

#include "externs.h"
#include "beam.h"
#include "cloud.h"

// Base class for movement types
class MovementHandler
{
public:
    MovementHandler(actor *_subject) :
        subject(_subject),
        currently_moving(false),
        catching_up(false),
        need_another_move(false),
        kill_after_move(false)
        {};
    virtual ~MovementHandler();

    static MovementHandler* handler_for(actor *act);

    virtual bool do_override(void) { return true; };

    bool move();
    void catchup(int turns);
    void killed();
    virtual void stop(bool show_message = true) { };
    virtual void impulse(float ix, float iy) { };

    // Called when the monster has been moved by another force,
    // e.g. tele, so we can update our information about it
    virtual void moved_by_other(const coord_def& new_pos) { };

protected:
    actor *subject;
    bool currently_moving;
    bool catching_up;
    bool need_another_move;
    bool kill_after_move;

    virtual void move_again();
    virtual void setup() = 0;
    virtual void save() = 0;

    virtual coord_def get_move_pos();
    virtual coord_def get_move_dir();
    virtual bool check_pos(const coord_def& new_pos);

    virtual bool on_moving(const coord_def& new_pos);
    virtual bool on_moved(const coord_def& old_pos) { return true; };
    virtual void post_move(const coord_def& new_pos) { };
    virtual bool on_catchup(int moves) { return true; };

    virtual void hit_solid(const coord_def& pos) { };
    virtual bool hit_player(const coord_def& pos) { return false; };
    virtual bool hit_monster(const coord_def& pos, monster* victim) { return false; };
};

// Default movement doesn't override; lets 'classic'
// movement happen
class DefaultMovement : public MovementHandler
{
public:
    DefaultMovement(actor *_subject) :
        MovementHandler(_subject)
        {};
    bool do_override() { return false; };

protected:
    void setup() { };
    void save() { };
};

class ProjectileMovement : public MovementHandler
{
public:
    ProjectileMovement (actor *_subject) :
        MovementHandler(_subject)
        { };
    void setup();

protected:
    short pow;
    float x;
    float y;
    float vx;
    float vy;
    float nx;
    float ny;
    int distance; // Distance travelled
    char kc;
    mid_t caster_mid;
    string caster_name;
    int tpos;

    void save();
    void save_all();

    coord_def get_move_pos();
    virtual void aim() = 0;

    void post_move(const coord_def& new_pos);
    bool on_moved(const coord_def& old_pos);
    void moved_by_other(const coord_def& new_pos);

    actor *get_caster();
    actor *get_target();
    string get_caster_name();

    virtual void hit_solid(const coord_def& pos);
    virtual bool hit_player(const coord_def& pos);
    virtual bool hit_monster(const coord_def& pos, monster* victim);
    virtual bool hit_own_kind(const coord_def& pos, monster* victim);
    virtual bool hit_actor(const coord_def& pos, actor *victim);

    bool victim_shielded(actor *victim);
    int get_hit_power();

    bool has_trail();
    virtual cloud_type trail_type();

    virtual void normalise();
    static void _normalise(float &x, float &y);
    static bool _in_front(float vx, float vy, float dx, float dy, float angle);
};

class OrbMovement : public ProjectileMovement
{
public:
    OrbMovement (actor *_subject) :
        ProjectileMovement(_subject),
        flawed(false)
        { };
    void setup();
    void save();
    void save_all();
    void aim();
    void stop(bool show_message = true);

    // Static spellcasting functions
    static spret_type cast_iood(actor *caster, int pow, bolt *beam,
                                float vx = 0, float vy = 0, int foe = MHITNOT,
                                bool fail = false);
    static void cast_iood_burst(int pow, coord_def target);

protected:
    bool flawed;

    bool check_pos(const coord_def& new_pos);

    void hit_solid(const coord_def& pos);
    bool hit_actor(const coord_def& pos, actor *actor);
    bool hit_own_kind(const coord_def& pos, monster *victim);
    void strike(const coord_def &pos, bool big_boom = false);

    bool on_catchup(int moves);

    // Helper function for spellcasting
    static void _fuzz_direction(const actor *caster, monster& mon, int pow);
};

// Base class for boulder projectiles
class BoulderMovement : public ProjectileMovement
{
public:
    BoulderMovement (actor *_subject) :
        ProjectileMovement(_subject)
        { };

protected:
    void aim() { };
    void hit_solid(const coord_def& pos);
    bool hit_actor(const coord_def& pos, actor *actor);
    bool hit_own_kind(const coord_def& pos, monster *victim);
    bool strike(actor *victim);

    cloud_type trail_type();
};

// For boulder beetle movement
class MonsterBoulderMovement : public BoulderMovement
{
public:
    MonsterBoulderMovement (actor *_subject) :
        BoulderMovement(_subject)
        { };
    void stop(bool show_message);
    static void start_rolling(monster *mon, bolt *beam);

protected:
    int get_hit_power();
    bool check_pos(const coord_def& new_pos);
};

class PlayerBoulderMovement : public BoulderMovement
{
public:
    PlayerBoulderMovement (actor *_subject) :
        BoulderMovement(_subject)
        { };
    void stop(bool show_message);
    void impulse(float ix, float iy);
    static void start_rolling();

protected:
    void normalise();
    int get_hit_power();
};

#endif
