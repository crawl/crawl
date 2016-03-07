#include "AppHdr.h"

#ifdef USE_TILE
#include "tilemcache.h"

#include "colour.h"
#include "env.h"
#include "libutil.h"
#include "misc.h"
#include "mon-info.h"
#include "mon-util.h"
#include "options.h"
#include "tiledef-player.h"
#include "tilepick.h"
#include "tilepick-p.h"

mcache_manager mcache;

// Used internally for streaming.
enum mcache_type
{
    MCACHE_MONSTER,
    MCACHE_DRACO,
    MCACHE_GHOST,
    MCACHE_DEMON,
    MCACHE_MAX,

    MCACHE_nullptr,
};

struct demon_data
{
    demon_data() { head = body = wings = 0; }

    tileidx_t head;
    tileidx_t body;
    tileidx_t wings;
};

// Internal mcache classes. The mcache_manager creates these internally.
// The only access external clients need is through the virtual
// info function.

class mcache_monster : public mcache_entry
{
public:
    mcache_monster(const monster_info& mon);

    virtual int info(tile_draw_info *dinfo) const override;

    static bool valid(const monster_info& mon);

    static bool get_weapon_offset(monster_type mtype, tileidx_t mon_tile,
                                  int *ofs_x, int *ofs_y);
    static bool get_shield_offset(monster_type mtype, tileidx_t mon_tile,
                                  int *ofs_x, int *ofs_y);

protected:
    monster_type mtype;
    tileidx_t m_mon_tile;
    tileidx_t m_equ_tile;
    tileidx_t m_shd_tile;
};

class mcache_draco : public mcache_entry
{
public:
    mcache_draco(const monster_info& mon);

    virtual int info(tile_draw_info *dinfo) const override;

    static bool valid(const monster_info& mon);

protected:
    tileidx_t m_mon_tile;
    tileidx_t m_job_tile;
    tileidx_t m_equ_tile;
    tileidx_t m_shd_tile;
    bool draco;
};

class mcache_ghost : public mcache_entry
{
public:
    mcache_ghost(const monster_info& mon);

    virtual const dolls_data *doll() const override;

    static bool valid(const monster_info& mon);

    virtual bool transparent() const override;

protected:
    dolls_data m_doll;
};

class mcache_demon : public mcache_entry
{
public:
    mcache_demon(const monster_info& minf);

    virtual int info(tile_draw_info *dinfo) const override;

    static bool valid(const monster_info& mon);

protected:
    demon_data m_demon;
};

/////////////////////////////////////////////////////////////////////////////
// tile_fg_store

tileidx_t tile_fg_store::operator=(tileidx_t tile)
{
    if ((tile & TILE_FLAG_MASK) == (m_tile & TILE_FLAG_MASK))
    {
        // Update, as flags may have changed.
        m_tile = tile;
        return m_tile;
    }

    mcache_entry *old_entry = mcache.get(m_tile);
    if (old_entry)
        old_entry->dec_ref();

    m_tile = tile;

    mcache_entry *new_entry = mcache.get(m_tile);
    if (new_entry)
        new_entry->inc_ref();

    return m_tile;
}

/////////////////////////////////////////////////////////////////////////////
// mcache_manager

mcache_manager::~mcache_manager()
{
    clear_all();
}

unsigned int mcache_manager::register_monster(const monster_info& minf)
{
    // TODO enne - is it worth it to search against all mcache entries?
    // TODO enne - pool mcache types to avoid too much alloc/dealloc?

    mcache_entry *entry;

    if (minf.props.exists("monster_tile"))
    {
        if (mcache_monster::valid(minf))
            entry = new mcache_monster(minf);
        else
            return 0;
    }
    else if (mcache_demon::valid(minf))
        entry = new mcache_demon(minf);
    else if (mcache_ghost::valid(minf))
        entry = new mcache_ghost(minf);
    else if (mcache_draco::valid(minf))
        entry = new mcache_draco(minf);
    else if (mcache_monster::valid(minf))
        entry = new mcache_monster(minf);
    else
        return 0;

    tileidx_t idx = ~0;

    for (unsigned int i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i])
        {
            m_entries[i] = entry;
            idx = i;
            break;
        }
    }

    if (idx > m_entries.size())
    {
        idx = m_entries.size();
        m_entries.push_back(entry);
    }

    return TILEP_MCACHE_START + idx;
}

void mcache_manager::clear_nonref()
{
    for (mcache_entry *&entry : m_entries)
    {
        if (!entry || entry->ref_count() > 0)
            continue;

        delete entry;
        entry = nullptr;
    }
}

void mcache_manager::clear_all()
{
    deleteAll(m_entries);
}

mcache_entry *mcache_manager::get(tileidx_t tile)
{
    tileidx_t idx = tile & TILE_FLAG_MASK;
    if (idx < TILEP_MCACHE_START)
        return nullptr;

    if (idx >= TILEP_MCACHE_START + m_entries.size())
        return nullptr;

    mcache_entry *entry = m_entries[idx - TILEP_MCACHE_START];
    return entry;
}

/////////////////////////////////////////////////////////////////////////////
// mcache_monster

mcache_monster::mcache_monster(const monster_info& mon)
{
    ASSERT(mcache_monster::valid(mon));

    mtype = mon.type;
    m_mon_tile = tileidx_monster(mon) & TILE_FLAG_MASK;

    const item_info* mon_weapon = mon.inv[MSLOT_WEAPON].get();
    m_equ_tile = (mon_weapon != nullptr) ? tilep_equ_weapon(*mon_weapon) : 0;
    if (mons_class_wields_two_weapons(mon.type))
    {
        const item_info* mon_weapon2 = mon.inv[MSLOT_ALT_WEAPON].get();
        if (mon_weapon2)
        {
            switch (tilep_equ_weapon(*mon_weapon2))
            {
                case TILEP_HAND1_DAGGER:
                    m_shd_tile = TILEP_HAND2_DAGGER;
                    break;
                case TILEP_DAGGER_1:
                    m_shd_tile = TILEP_HAND2_DAGGER_1;
                    break;
                case TILEP_HAND1_RAPIER:
                    m_shd_tile = TILEP_HAND2_RAPIER;
                    break;
                case TILEP_RAPIER_1:
                    m_shd_tile = TILEP_HAND2_RAPIER_1;
                    break;
                default:
                case TILEP_HAND1_SHORT_SWORD_SLANT:
                    m_shd_tile = TILEP_HAND2_SHORT_SWORD_SLANT;
                    break;
                case TILEP_SHORT_SWORD_SLANT_1:
                    m_shd_tile = TILEP_HAND2_SHORT_SWORD_SLANT_1;
                    break;
                case TILEP_HAND1_GREAT_FLAIL:
                    m_shd_tile = TILEP_HAND2_GREAT_FLAIL;
                    break;
                case TILEP_GREAT_FLAIL_1:
                    m_shd_tile = TILEP_HAND2_GREAT_FLAIL_1;
                    break;
                case TILEP_HAND1_GREAT_MACE:
                    m_shd_tile = TILEP_HAND2_GREAT_MACE;
                    break;
                case TILEP_GREAT_MACE_1:
                    m_shd_tile = TILEP_HAND2_GREAT_MACE_1;
                    break;
                case TILEP_HAND1_GIANT_CLUB:
                    m_shd_tile = TILEP_HAND2_GIANT_CLUB;
                    break;
                case TILEP_HAND1_GIANT_CLUB_SLANT:
                    m_shd_tile = TILEP_HAND2_GIANT_CLUB_SLANT;
                    break;
                case TILEP_HAND1_GIANT_CLUB_SPIKE:
                    m_shd_tile = TILEP_HAND2_GIANT_CLUB_SPIKE;
                    break;
                case TILEP_HAND1_GIANT_CLUB_SPIKE_SLANT:
                    m_shd_tile = TILEP_HAND2_GIANT_CLUB_SPIKE_SLANT;
                    break;
                case TILEP_HAND1_GIANT_CLUB_PLAIN:
                    m_shd_tile = TILEP_HAND2_GIANT_CLUB_PLAIN;
                    break;
            };
        }
        else
            m_shd_tile = 0;
    }
    else
    {
        const item_info* mon_shield = mon.inv[MSLOT_SHIELD].get();
        m_shd_tile = (mon_shield != nullptr) ? tilep_equ_shield(*mon_shield) : 0;
    }
}

// Returns the amount of pixels necessary to shift a wielded weapon
// from its default placement. Tiles showing monsters already wielding
// a weapon should not be listed here.
bool mcache_monster::get_weapon_offset(monster_type mtype, tileidx_t mon_tile,
                                       int *ofs_x, int *ofs_y)
{
    switch (mon_tile)
    {
        case TILEP_MONS_ZOMBIE_OCTOPODE:
            *ofs_x = 0;
            *ofs_y = 0;
            break;

        case TILEP_MONS_YAKTAUR_MELEE:
            *ofs_x = 2;
            *ofs_y = 0;
            break;

        case TILEP_MONS_YAKTAUR_CAPTAIN_MELEE:
            *ofs_x = 4;
            *ofs_y = 0;
            break;

        case TILEP_MONS_CENTAUR_WARRIOR_MELEE:
            *ofs_x = 0;
            *ofs_y = -1;
            break;

        case TILEP_MONS_DIMME:
            *ofs_x = 0;
            *ofs_y = 2;
            break;

        case TILEP_MONS_CENTAUR_MELEE:
            *ofs_x = -3;
            *ofs_y = -3;
            break;

        case TILEP_MONS_ARACHNE_STAVELESS:
        case TILEP_MONS_MERFOLK_WATER:
        case TILEP_MONS_MERFOLK_AQUAMANCER_WATER:
        case TILEP_MONS_MERFOLK_IMPALER_WATER:
        case TILEP_MONS_MERFOLK_JAVELINEER_WATER:
        case TILEP_MONS_SIREN_WATER:
        case TILEP_MONS_MERFOLK_AVATAR_WATER:
        case TILEP_MONS_ILSUIW_WATER:
            *ofs_x = -1;
            *ofs_y = -5;
            break;

        case TILEP_MONS_MASTER_ELEMENTALIST:
            *ofs_x = 1;
            *ofs_y = -1;
            break;

        case TILEP_MONS_HELL_WIZARD:
        case TILEP_MONS_HELL_WIZARD_1:
        case TILEP_MONS_HELL_WIZARD_2:
            *ofs_x = 2;
            *ofs_y = -2;
            break;

        case TILEP_MONS_MERFOLK:
        case TILEP_MONS_MERFOLK_AQUAMANCER:
        case TILEP_MONS_MERFOLK_IMPALER:
        case TILEP_MONS_MERFOLK_JAVELINEER:
        case TILEP_MONS_SIREN:
        case TILEP_MONS_MERFOLK_AVATAR:
        case TILEP_MONS_ILSUIW:
            *ofs_x = 1;
            *ofs_y = -5;
            break;

        case TILEP_MONS_CIGOTUVIS_MONSTER:
            *ofs_x = -4;
            *ofs_y = 2;
            break;

        case TILEP_MONS_ABOMINATION_LARGE_1:
            *ofs_x = -2;
            *ofs_y = -8;
            break;
        case TILEP_MONS_ABOMINATION_LARGE_2:
            *ofs_x = -3;
            *ofs_y = 0;
            break;
        case TILEP_MONS_ABOMINATION_LARGE_3:
            *ofs_x = -4;
            *ofs_y = -5;
            break;
        case TILEP_MONS_ABOMINATION_LARGE_4:
            *ofs_x = -3;
            *ofs_y = -10;
            break;
        case TILEP_MONS_ABOMINATION_LARGE_6:
            *ofs_x = -4;
            *ofs_y = -11;
            break;

        case TILEP_MONS_CHAOS_SPAWN:
            *ofs_x = 0;
            *ofs_y = -5;
            break;
        case TILEP_MONS_CHAOS_SPAWN_2:
            *ofs_x = 2;
            *ofs_y = -3;
            break;
        case TILEP_MONS_CHAOS_SPAWN_3:
            *ofs_x = -2;
            *ofs_y = -1;
            break;

        case TILEP_MONS_ORB_GUARDIAN_FETUS:
            *ofs_x = 4;
            *ofs_y = -7;
            break;

        case TILEP_MONS_JORMUNGANDR:
            *ofs_x = 9;
            *ofs_y = -4;
            break;

        case TILEP_MONS_GIAGGOSTUONO:
            *ofs_x = -5;
            *ofs_y = 5;
            break;

        default:
            equipment_display* display_data = mon_weapon_display(mtype);
            if (display_data->disable)
            {
                // This monster cannot be displayed with a weapon.
                return false;
            }

            *ofs_x = display_data->x;
            *ofs_y = display_data->y;
            break;
    }

    return true;
}

// Returns the amount of pixels necessary to shift a worn shield, like
// it's done with weapon. No monster should have a shield hard-drawn
// on the tile.
bool mcache_monster::get_shield_offset(monster_type mtype, tileidx_t mon_tile,
                                       int *ofs_x, int *ofs_y)
{
    switch (mon_tile)
    {
        case TILEP_MONS_DIMME:
            *ofs_x = 1;
            *ofs_y = 0;
            break;

        case TILEP_MONS_ARACHNE_STAVELESS:
            *ofs_x = -6;
            *ofs_y = 1;
            break;

        case TILEP_MONS_MERFOLK_AQUAMANCER:
        case TILEP_MONS_MERFOLK_IMPALER:
        case TILEP_MONS_MERFOLK_JAVELINEER:
        case TILEP_MONS_SIREN:
        case TILEP_MONS_MERFOLK_AVATAR:
        case TILEP_MONS_ILSUIW:
            *ofs_x = -3;
            *ofs_y = -2;
            break;

        case TILEP_MONS_MERFOLK_WATER:
        case TILEP_MONS_MERFOLK_AQUAMANCER_WATER:
        case TILEP_MONS_MERFOLK_IMPALER_WATER:
        case TILEP_MONS_MERFOLK_JAVELINEER_WATER:
        case TILEP_MONS_SIREN_WATER:
        case TILEP_MONS_MERFOLK_AVATAR_WATER:
        case TILEP_MONS_ILSUIW_WATER:
            *ofs_x = -5;
            *ofs_y = -2;
            break;

        case TILEP_MONS_CENTAUR_MELEE:
            *ofs_x = -8;
            *ofs_y = -4;
            break;

        case TILEP_MONS_CIGOTUVIS_MONSTER:
            *ofs_x = 4;
            *ofs_y = -1;
            break;

        case TILEP_MONS_ABOMINATION_LARGE_1:
            *ofs_x = 2;
            *ofs_y = -8;
            break;

        case TILEP_MONS_ABOMINATION_LARGE_2:
            *ofs_x = 2;
            *ofs_y = 0;
            break;

        case TILEP_MONS_ABOMINATION_LARGE_3:
            *ofs_x = -2;
            *ofs_y = -2;
            break;

        case TILEP_MONS_ABOMINATION_LARGE_4:
            *ofs_x = 2;
            *ofs_y = -7;
            break;

        case TILEP_MONS_ABOMINATION_LARGE_6:
            *ofs_x = 3;
            *ofs_y = -10;
            break;

        case TILEP_MONS_CHAOS_SPAWN:
            *ofs_x = 0;
            *ofs_y = -5;
            break;

        case TILEP_MONS_CHAOS_SPAWN_2:
            *ofs_x = 0;
            *ofs_y = -2;
            break;

        case TILEP_MONS_CHAOS_SPAWN_3:
            *ofs_x = 2;
            *ofs_y = -1;
            break;

        case TILEP_MONS_ORB_GUARDIAN_FETUS:
            *ofs_x = -8;
            *ofs_y = 5;
            break;

        case TILEP_MONS_JORMUNGANDR:
            *ofs_x = -2;
            *ofs_y = 2;
            break;

        case TILEP_MONS_LAMIA:
            *ofs_x = -2;
            *ofs_y = -5;
            break;

        case TILEP_MONS_GIAGGOSTUONO:
            *ofs_x = 2;
            *ofs_y = -7;
            break;

        default:
            equipment_display* display_data = mon_shield_display(mtype);
            if (display_data->disable)
            {
                // This monster cannot be displayed with a shield.
                return false;
            }

            *ofs_x = display_data->x;
            *ofs_y = display_data->y;
            break;
    }

    return true;
}

int mcache_monster::info(tile_draw_info *dinfo) const
{
    int count = 0;
    dinfo[count++].set(m_mon_tile);

    int ofs_x, ofs_y;

    if (m_equ_tile)
    {
        bool have_offs = false;
        // This mcache is for the player using a custom tile.
        if (mtype == MONS_PLAYER
            && Options.tile_weapon_offsets.first != INT_MAX)
        {
            ofs_x = Options.tile_weapon_offsets.first;
            ofs_y = Options.tile_weapon_offsets.second;
            have_offs = true;
        }
        else if (get_weapon_offset(mtype, m_mon_tile, &ofs_x, &ofs_y))
            have_offs = true;

        if (have_offs)
            dinfo[count++].set(m_equ_tile, ofs_x, ofs_y);
    }
    if (m_shd_tile)
    {
        bool have_offs = false;
        if (mtype == MONS_PLAYER
            && Options.tile_shield_offsets.first != INT_MAX)
        {
            ofs_x = Options.tile_shield_offsets.first;
            ofs_y = Options.tile_shield_offsets.second;
            have_offs = true;
        }
        else if (get_shield_offset(mtype, m_mon_tile, &ofs_x, &ofs_y))
            have_offs = true;

        if (have_offs)
            dinfo[count++].set(m_shd_tile, ofs_x, ofs_y);
    }
    return count;
}

bool mcache_monster::valid(const monster_info& mon)
{
    tileidx_t mon_tile = tileidx_monster(mon) & TILE_FLAG_MASK;

    int ox, oy;
    bool have_weapon_offs = (mon.type == MONS_PLAYER
                             && Options.tile_weapon_offsets.first != INT_MAX)
        || get_weapon_offset(mon.type, mon_tile, &ox, &oy);
    bool have_shield_offs = (mon.type == MONS_PLAYER
                             && Options.tile_shield_offsets.first != INT_MAX)
        || get_shield_offset(mon.type, mon_tile, &ox, &oy);
    return (mon.inv[MSLOT_WEAPON].get() != nullptr && have_weapon_offs)
        || (mon.inv[MSLOT_SHIELD].get() != nullptr && have_shield_offs);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_draco

mcache_draco::mcache_draco(const monster_info& mon)
{
    ASSERT(mcache_draco::valid(mon));
    draco = mons_is_draconian(mon.type);

    m_mon_tile = draco ? tileidx_draco_base(mon)
                       : tileidx_demonspawn_base(mon);
    const item_info* mon_wep = mon.inv[MSLOT_WEAPON].get();
    m_equ_tile = (mon_wep != nullptr) ? tilep_equ_weapon(*mon_wep) : 0;
    mon_wep = mon.inv[MSLOT_SHIELD].get();
    m_shd_tile = (mon_wep != nullptr) ? tilep_equ_shield(*mon_wep) : 0;
    m_job_tile = draco ? tileidx_draco_job(mon)
                       : tileidx_demonspawn_job(mon);
}

int mcache_draco::info(tile_draw_info *dinfo) const
{
    int i = 0;

    dinfo[i++].set(m_mon_tile);
    if (m_job_tile)
        dinfo[i++].set(m_job_tile);
    if (m_equ_tile)
    {
        if (draco)
            dinfo[i++].set(m_equ_tile, -3, -1);
        else
            dinfo[i++].set(m_equ_tile, -1, 0);
    }
    if (m_shd_tile)
        dinfo[i++].set(m_shd_tile, 2, 0);

    return i;
}

bool mcache_draco::valid(const monster_info& mon)
{
    return mons_is_draconian(mon.type) || mons_is_demonspawn(mon.type);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_ghost

mcache_ghost::mcache_ghost(const monster_info& mon)
{
    ASSERT(mcache_ghost::valid(mon));

    const uint32_t seed = hash32(&mon.mname[0], mon.mname.size())
                        ^ hash32(&mon.i_ghost, sizeof(mon.i_ghost));

    tilep_race_default(mon.i_ghost.species, 0, &m_doll);
    tilep_job_default(mon.i_ghost.job, &m_doll);

    for (int p = TILEP_PART_CLOAK; p < TILEP_PART_MAX; p++)
    {
        if (m_doll.parts[p] == TILEP_SHOW_EQUIP)
        {
            int part_offset = hash_rand(tile_player_part_count[p], seed, p);
            m_doll.parts[p] = tile_player_part_start[p] + part_offset;
        }
    }

    int ac = mon.i_ghost.ac;
    ac *= (5 + hash_rand(11, seed, 1000));
    ac /= 10;

    // Become uncannily spooky!
    if (today_is_halloween())
        m_doll.parts[TILEP_PART_HELM] = TILEP_HELM_PUMPKIN;

    if (ac > 25)
        m_doll.parts[TILEP_PART_BODY] = TILEP_BODY_PLATE_BLACK;
    else if (ac > 18)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_CHAINMAIL;
    else if (ac > 12)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_SCALEMAIL;
    else if (ac > 5)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_LEATHER_ARMOUR;
    else
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_ROBE_BLUE;

    int sk = mon.i_ghost.best_skill;
    int dam = mon.i_ghost.damage;
    dam *= (5 + hash_rand(11, seed, 1001));
    dam /= 10;

    switch (sk)
    {
    case SK_MACES_FLAILS:
        if (dam > 30)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_GREAT_FLAIL;
        else if (dam > 25)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_GREAT_MACE;
        else if (dam > 20)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_EVENINGSTAR;
        else if (dam > 15)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_MORNINGSTAR;
        else if (dam > 10)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_FLAIL;
        else if (dam > 5)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_MACE;
        else
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_CLUB_SLANT;
        break;

    case SK_SHORT_BLADES:
        if (dam > 20)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_RAPIER;
        else if (dam > 10)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SHORT_SWORD_SLANT;
        else
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_DAGGER_SLANT;
        break;

    case SK_LONG_BLADES:
        if (dam > 25)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_GREAT_SWORD_SLANT;
        else if (dam > 20)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_KATANA_SLANT;
        else if (dam > 15)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SCIMITAR;
        else if (dam > 10)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_LONG_SWORD_SLANT;
        else
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_FALCHION;
        break;

    case SK_AXES:
        if (dam > 30)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_EXECUTIONERS_AXE;
        else if (dam > 20)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_BATTLEAXE;
        else if (dam > 15)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_BROAD_AXE;
        else if (dam > 10)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_WAR_AXE;
        else
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_HAND_AXE;
        break;

    case SK_POLEARMS:
        if (dam > 30)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_GLAIVE;
        else if (dam > 20)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SCYTHE;
        else if (dam > 15)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_HALBERD;
        else if (dam > 10)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_TRIDENT2;
        else
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SPEAR;
        break;

    case SK_BOWS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_BOW2;
        break;

    case SK_CROSSBOWS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_ARBALEST;
        break;

    case SK_SLINGS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_HUNTING_SLING;
        break;

    case SK_UNARMED_COMBAT:
    default:
        m_doll.parts[TILEP_PART_HAND1] = m_doll.parts[TILEP_PART_HAND2] = 0;
        break;
    }
}

const dolls_data *mcache_ghost::doll() const
{
    return &m_doll;
}

bool mcache_ghost::valid(const monster_info& mon)
{
    return mons_is_pghost(mon.type);
}

bool mcache_ghost::transparent() const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// mcache_demon

mcache_demon::mcache_demon(const monster_info& minf)
{
    ASSERT(minf.type == MONS_PANDEMONIUM_LORD);

    const uint32_t seed = hash32(&minf.mname[0], minf.mname.size());

    m_demon.head = tile_player_coloured(TILEP_DEMON_HEAD,
                                        element_colour(minf.colour()))
        + hash_rand(tile_player_count(TILEP_DEMON_HEAD), seed, 1);
    m_demon.body = tile_player_coloured(TILEP_DEMON_BODY,
                                        element_colour(minf.colour()))
        + hash_rand(tile_player_count(TILEP_DEMON_BODY), seed, 2);

    if (minf.is(MB_AIRBORNE))
    {
        m_demon.wings = tile_player_coloured(TILEP_DEMON_WINGS,
                                             element_colour(minf.colour()))
            + hash_rand(tile_player_count(TILEP_DEMON_WINGS), seed, 3);
    }
    else
        m_demon.wings = 0;
}

int mcache_demon::info(tile_draw_info *dinfo) const
{
    if (m_demon.wings)
    {
        dinfo[0].set(m_demon.wings);
        dinfo[1].set(m_demon.body);
        dinfo[2].set(m_demon.head);
        return 3;
    }
    else
    {
        dinfo[0].set(m_demon.body);
        dinfo[1].set(m_demon.head);
        return 2;
    }
}

bool mcache_demon::valid(const monster_info& mon)
{
    return mon.type == MONS_PANDEMONIUM_LORD;
}

#endif
