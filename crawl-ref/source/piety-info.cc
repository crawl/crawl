#include "AppHdr.h"

#include "god-conduct.h"
#include "mpr.h"
#include "piety-info.h"
#include "player.h"
#include "religion.h"

static bool god_is_tracked(god_type god)
{
    // Gods who have funny piety mechanics and so aren't tracked.
    if (god == GOD_NO_GOD
        || god == GOD_ASHENZARI
        || god == GOD_GOZAG
        || god == GOD_IGNIS
        || god == GOD_RU
        || god == GOD_USKAYAW
        || god == GOD_XOM)
    {
        return false;
    }
    return true;
}

RankPietyInfo::RankPietyInfo()
    : god(you.religion), initial_piety(you.raw_piety), start_time(you.elapsed_time),
      piety_lost(0), piety_gained(0), piety_decayed(0), piety_on_penance(0),
      piety_on_gifts(0), piety_on_stepdowns(0)
{
}

ConductPietyInfo::ConductPietyInfo()
    : conducts_count({}), piety_from_conducts({})
{
}

const ConductPietyInfo &ConductPietyInfo::operator += (const ConductPietyInfo &other)
{
    for (const auto &pair : other.conducts_count)
    {
        conduct_type ct = pair.first;
        int count = pair.second;
        if (conducts_count.count(ct))
            conducts_count[ct] += count;
        else
            conducts_count[ct] = count;
    }
    for (const auto &pair : other.piety_from_conducts)
    {
        conduct_type ct = pair.first;
        float piety = pair.second;
        if (piety_from_conducts.count(ct))
            piety_from_conducts[ct] += piety;
        else
            piety_from_conducts[ct] = piety;
    }
    return *this;
}

PietyInfo::PietyInfo() : rank_info({}), rank(0), conduct_info_by_god({})
{
}

void PietyInfo::update_rank()
{
    int new_rank = piety_rank(you.raw_piety);
    if (new_rank == rank)
        return;
    rank = new_rank;
    rank_info.push_back(RankPietyInfo());
}

void PietyInfo::register_piety_gain(PietyGainEvent event)
{
    if (!god_is_tracked(you.religion) || rank_info.empty())
        return;
    switch (event)
    {
        case PG_EVENT_PENANCE_PAYMENT:
            rank_info.back().piety_on_penance += 1;
            break;
        case PG_EVENT_GIFT_PENALTY:
            rank_info.back().piety_on_gifts += 1;
            break;
        case PG_EVENT_STEPDOWN:
            rank_info.back().piety_on_stepdowns += 1;
            break;
        case PG_EVENT_TRUE_GAIN:
            rank_info.back().piety_gained += 1;
            break;
    }
    update_rank();
}

void PietyInfo::register_piety_loss(unsigned int amount)
{
    if (!god_is_tracked(you.religion) || rank_info.empty())
        return;
    rank_info.back().piety_lost += amount;
    update_rank();
}

void PietyInfo::register_piety_decay()
{
    if (!god_is_tracked(you.religion) || rank_info.empty())
        return;
    rank_info.back().piety_decayed += 1;
}

void PietyInfo::record_conduct_like(conduct_type thing, int gain, int denom)
{
    if (!god_is_tracked(you.religion) || denom <= 0)
        return;

    if (!conduct_info_by_god.count(you.religion))
        conduct_info_by_god[you.religion] = {};

    map<int, ConductPietyInfo> &info_by_xl = conduct_info_by_god[you.religion];

    int xl = you.get_experience_level();

    if (!info_by_xl.count(xl))
        info_by_xl[xl] = ConductPietyInfo();

    map<conduct_type, int> &conduct_map = info_by_xl[xl].conducts_count;
    if (!conduct_map.count(thing))
        conduct_map[thing] = 0;
    conduct_map[thing] += 1;

    map<conduct_type, float> &piety_map = info_by_xl[xl].piety_from_conducts;
    if (!piety_map.count(thing))
        piety_map[thing] = 0.0f;
    piety_map[thing] += static_cast<float>(gain) / static_cast<float>(denom);
}

void PietyInfo::register_join()
{
    rank_info.push_back(RankPietyInfo());
    rank = piety_rank(you.raw_piety);
}

void PietyInfo::register_excommunication()
{
    rank_info.push_back(RankPietyInfo());
}
