#pragma once

#include <map>
#include "conduct-type.h"
#include "god-type.h"

enum PietyGainEvent
{
    PG_EVENT_PENANCE_PAYMENT,
    PG_EVENT_GIFT_PENALTY,
    PG_EVENT_STEPDOWN,
    PG_EVENT_TRUE_GAIN,
};

// Tracks piety info for a "visit" to a piety rank.
struct RankPietyInfo
{
    god_type god;
    unsigned int initial_piety;
    unsigned int start_time;
    unsigned int piety_lost;
    unsigned int piety_gained;
    unsigned int piety_decayed;
    unsigned int piety_on_penance;
    unsigned int piety_on_gifts;
    unsigned int piety_on_stepdowns;

    RankPietyInfo();
};

// Tracks piety stats for conducts; tracked by XL and god, so
// that we can see how sources of piety vary during a game.
struct ConductPietyInfo
{
    map<conduct_type, int> conducts_count;
    map<conduct_type, float> piety_from_conducts;
    const ConductPietyInfo &operator += (const ConductPietyInfo &other);

    ConductPietyInfo();
};

class PietyInfo
{
public:
    PietyInfo();

    void register_join();
    void register_excommunication();
    // Lost through any means (spend, faith removal, decay, penance, ...).
    void register_piety_loss(unsigned int amount);
    void register_piety_decay();
    void register_piety_gain(PietyGainEvent event);
    vector<RankPietyInfo> rank_info;
    // We always work in raw piety, and calculate ranks ourselves, ignoring
    // ostracism. This is because piety stepdowns also work off raw piety,
    // and gift timeouts don't reset.
    int rank;

    void record_conduct_like(conduct_type thing, int gain, int denom);
    map<god_type, map<int, ConductPietyInfo>> conduct_info_by_god;

private:
    void update_rank();
};
