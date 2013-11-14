#include "AppHdr.h"

#ifdef USE_TILE
#include "tilemcache.h"

#include "env.h"
#include "mon-util.h"
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

    MCACHE_NULL,
};

struct demon_data
{
    demon_data() { head = body = wings = 0; }

    tileidx_t head;
    tileidx_t body;
    tileidx_t wings;
};

// Internal mcache classes.  The mcache_manager creates these internally.
// The only access external clients need is through the virtual
// info function.

class mcache_monster : public mcache_entry
{
public:
    mcache_monster(const monster_info& mon);

    virtual int info(tile_draw_info *dinfo) const;

    static bool valid(const monster_info& mon);

    static bool get_weapon_offset(tileidx_t mon_tile, int *ofs_x, int *ofs_y);
    static bool get_shield_offset(tileidx_t mon_tile, int *ofs_x, int *ofs_y);

protected:
    tileidx_t m_mon_tile;
    tileidx_t m_equ_tile;
    tileidx_t m_shd_tile;
};

class mcache_draco : public mcache_entry
{
public:
    mcache_draco(const monster_info& mon);

    virtual int info(tile_draw_info *dinfo) const;

    static bool valid(const monster_info& mon);

protected:
    tileidx_t m_mon_tile;
    tileidx_t m_job_tile;
    tileidx_t m_equ_tile;
    tileidx_t m_shd_tile;
};

class mcache_ghost : public mcache_entry
{
public:
    mcache_ghost(const monster_info& mon);

    virtual const dolls_data *doll() const;

    static bool valid(const monster_info& mon);

    virtual bool transparent() const;

protected:
    dolls_data m_doll;
};

class mcache_demon : public mcache_entry
{
public:
    mcache_demon(const monster_info& minf);

    virtual int info(tile_draw_info *dinfo) const;

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

    if (mcache_demon::valid(minf))
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
    for (unsigned int i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i] || m_entries[i]->ref_count() > 0)
            continue;

        delete m_entries[i];
        m_entries[i] = NULL;
    }
}

void mcache_manager::clear_all()
{
    for (unsigned int i = 0; i < m_entries.size(); i++)
        delete m_entries[i];

    m_entries.resize(0);
}

mcache_entry *mcache_manager::get(tileidx_t tile)
{
    tileidx_t idx = tile & TILE_FLAG_MASK;
    if (idx < TILEP_MCACHE_START)
        return NULL;

    if (idx >= TILEP_MCACHE_START + m_entries.size())
        return NULL;

    mcache_entry *entry = m_entries[idx - TILEP_MCACHE_START];
    return entry;
}

/////////////////////////////////////////////////////////////////////////////
// mcache_monster

mcache_monster::mcache_monster(const monster_info& mon)
{
    ASSERT(mcache_monster::valid(mon));

    m_mon_tile = tileidx_monster(mon) & TILE_FLAG_MASK;

    const item_info* mon_weapon = mon.inv[MSLOT_WEAPON].get();
    m_equ_tile = (mon_weapon != NULL) ? tilep_equ_weapon(*mon_weapon) : 0;
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
                case TILEP_HAND1_SABRE:
                    m_shd_tile = TILEP_HAND2_SABRE;
                    break;
                default:
                case TILEP_HAND1_SHORT_SWORD_SLANT:
                    m_shd_tile = TILEP_HAND2_SHORT_SWORD_SLANT;
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
        m_shd_tile = (mon_shield != NULL) ? tilep_equ_shield(*mon_shield) : 0;
    }
}

// Returns the amount of pixels necessary to shift a wielded weapon
// from its default placement. Tiles showing monsters already wielding
// a weapon should not be listed here.
bool mcache_monster::get_weapon_offset(tileidx_t mon_tile,
                                       int *ofs_x, int *ofs_y)
{
    switch (mon_tile)
    {
    // No shift necessary.
    case TILEP_MONS_DEEP_ELF_MASTER_ARCHER:
    case TILEP_MONS_DEEP_ELF_BLADEMASTER:
    case TILEP_MONS_CRIMSON_IMP:
    case TILEP_MONS_IRON_IMP:
    case TILEP_MONS_SHADOW_IMP:
    case TILEP_MONS_NORRIS:
    case TILEP_MONS_MAUD:
    case TILEP_MONS_FRANCES:
    case TILEP_MONS_HAROLD:
    case TILEP_MONS_JOSEPHINE:
    case TILEP_MONS_JOSEPH:
    case TILEP_MONS_TERENCE:
    case TILEP_MONS_WIGLAF:
    case TILEP_MONS_RAKSHASA:
    case TILEP_MONS_VAMPIRE_KNIGHT:
    case TILEP_MONS_SERAPH:
    case TILEP_MONS_CHERUB:
    case TILEP_MONS_MENNAS:
    case TILEP_MONS_PROFANE_SERVITOR:
    case TILEP_MONS_SPRIGGAN:
    case TILEP_MONS_SPRIGGAN_ASSASSIN:
    case TILEP_MONS_DEEP_DWARF_DEATH_KNIGHT:
    case TILEP_MONS_KOBOLD:
    case TILEP_MONS_OCTOPODE:
    case TILEP_MONS_ZOMBIE_OCTOPODE:
    case TILEP_MONS_NIKOLA:
    case TILEP_MONS_FORMICID:
    case TILEP_MONS_FORMICID_DRONE:
    case TILEP_MONS_FORMICID_VENOM_MAGE:
        *ofs_x = 0;
        *ofs_y = 0;
        break;
    // Shift to the left.
    case TILEP_MONS_GNOLL:
    case TILEP_MONS_GNOLL_SHAMAN:
    case TILEP_MONS_GNOLL_SERGEANT:
    case TILEP_MONS_GRUM:
    case TILEP_MONS_CRAZY_YIUF:
    case TILEP_MONS_DEEP_ELF_DEATH_MAGE:
    case TILEP_MONS_SPRIGGAN_DEFENDER:
    case TILEP_MONS_SPRIGGAN_BERSERKER:
    case TILEP_MONS_BIG_KOBOLD:
    case TILEP_MONS_MERFOLK:
    case TILEP_MONS_MERFOLK_WATER:
    case TILEP_MONS_MERFOLK_JAVELINEER:
    case TILEP_MONS_MERFOLK_JAVELINEER_WATER:
        *ofs_x = -1;
        *ofs_y = 0;
        break;
    case TILEP_MONS_HOBGOBLIN:
    case TILEP_MONS_TIAMAT:
    case TILEP_MONS_TIAMAT+1:
    case TILEP_MONS_TIAMAT+2:
    case TILEP_MONS_TIAMAT+3:
    case TILEP_MONS_TIAMAT+4:
    case TILEP_MONS_TIAMAT+5:
    case TILEP_MONS_TIAMAT+6:
    case TILEP_MONS_TIAMAT+7:
    case TILEP_MONS_TIAMAT+8:
    case TILEP_MONS_MERFOLK_IMPALER:
    case TILEP_MONS_MERFOLK_IMPALER_WATER:
    case TILEP_MONS_TENGU:
    case TILEP_MONS_TENGU_CONJURER:
    case TILEP_MONS_TENGU_WARRIOR:
    case TILEP_MONS_TENGU_REAVER:
    case TILEP_MONS_SOJOBO:
        *ofs_x = -2;
        *ofs_y = 0;
        break;
    case TILEP_MONS_SKELETON_LARGE:
        *ofs_x = -3;
        *ofs_y = 0;
        break;
    // Shift to the right.
    case TILEP_MONS_VAULT_GUARD:
    case TILEP_MONS_VAULT_WARDEN:
    case TILEP_MONS_VAULT_SENTINEL:
    case TILEP_MONS_IRONBRAND_CONVOKER:
    case TILEP_MONS_IRONHEART_PRESERVER:
    case TILEP_MONS_DEMONSPAWN:
    case TILEP_MONS_DONALD:
        *ofs_x = 1;
        *ofs_y = 0;
        break;
    case TILEP_MONS_YAKTAUR_MELEE:
    case TILEP_MONS_WIGHT:
        *ofs_x = 2;
        *ofs_y = 0;
        break;
    case TILEP_MONS_YAKTAUR_CAPTAIN_MELEE:
        *ofs_x = 4;
        *ofs_y = 0;
        break;
    case TILEP_MONS_FIRE_GIANT:
        *ofs_x = 5;
        *ofs_y = 0;
        break;
    // Shift upwards.
    case TILEP_MONS_CENTAUR_WARRIOR_MELEE:
    case TILEP_MONS_DEEP_ELF_SORCERER:
    case TILEP_MONS_DEEP_ELF_HIGH_PRIEST:
    case TILEP_MONS_RUPERT:
    case TILEP_MONS_HELL_KNIGHT:
    case TILEP_MONS_MUMMY:
    case TILEP_MONS_MUMMY_PRIEST:
    case TILEP_MONS_GUARDIAN_MUMMY:
    case TILEP_MONS_SKELETON_SMALL:
    case TILEP_MONS_PSYCHE:
        *ofs_x = 0;
        *ofs_y = -1;
        break;
    case TILEP_MONS_FREDERICK:
        *ofs_x = 0;
        *ofs_y = -2;
        break;
    case TILEP_MONS_RED_DEVIL:
    case TILEP_MONS_LAMIA:
        *ofs_x = 0;
        *ofs_y = -3;
        break;
    // Shift downwards.
    case TILEP_MONS_DEEP_ELF_KNIGHT:
    case TILEP_MONS_NAGA:
    case TILEP_MONS_GREATER_NAGA:
    case TILEP_MONS_NAGA_WARRIOR:
    case TILEP_MONS_GUARDIAN_SERPENT:
    case TILEP_MONS_NAGA_MAGE:
    case TILEP_MONS_THE_ENCHANTRESS:
    case TILEP_MONS_DEEP_DWARF:
    case TILEP_MONS_DEEP_DWARF_BERSERKER:
        *ofs_x = 0;
        *ofs_y = 1;
        break;
    case TILEP_MONS_ORC:
    case TILEP_MONS_URUG:
    case TILEP_MONS_BLORK_THE_ORC:
    case TILEP_MONS_SAINT_ROKA:
    case TILEP_MONS_ORC_WARRIOR:
    case TILEP_MONS_ORC_KNIGHT:
    case TILEP_MONS_ORC_WARLORD:
    case TILEP_MONS_BOGGART:
    case TILEP_MONS_DEEP_ELF_FIGHTER:
    case TILEP_MONS_UNBORN:
    case TILEP_MONS_JORGRUN:
    case TILEP_MONS_MERMAID:
    case TILEP_MONS_MERMAID_WATER:
    case TILEP_MONS_SIREN:
    case TILEP_MONS_SIREN_WATER:
    case TILEP_MONS_ILSUIW:
    case TILEP_MONS_ILSUIW_WATER:
    case TILEP_MONS_SUCCUBUS:
    case TILEP_MONS_HALFLING:
        *ofs_x = 0;
        *ofs_y = 2;
        break;
    // Shift upwards and to the left.
    case TILEP_MONS_DEEP_ELF_MAGE:
    case TILEP_MONS_DEEP_ELF_SUMMONER:
    case TILEP_MONS_DEEP_ELF_CONJURER:
    case TILEP_MONS_DEEP_ELF_PRIEST:
    case TILEP_MONS_DEEP_ELF_DEMONOLOGIST:
    case TILEP_MONS_DEEP_ELF_ANNIHILATOR:
        *ofs_x = -1;
        *ofs_y = -2;
        break;
    case TILEP_MONS_CENTAUR_MELEE:
    case TILEP_MONS_DEMIGOD:
        *ofs_x = -1;
        *ofs_y = -3;
        break;
    case TILEP_MONS_SONJA:
    case TILEP_MONS_ANCIENT_CHAMPION:
    case TILEP_MONS_SKELETAL_WARRIOR:
        *ofs_x = -2;
        *ofs_y = -2;
        break;
    case TILEP_MONS_SPRIGGAN_AIR_MAGE:
        *ofs_x = -3;
        *ofs_y = -5;
        break;
    case TILEP_MONS_DEEP_ELF_ELEMENTALIST:
    case TILEP_MONS_MERFOLK_AQUAMANCER:
    case TILEP_MONS_MERFOLK_AQUAMANCER_WATER:
        *ofs_x = -2;
        *ofs_y = -1;
        break;
    case TILEP_MONS_FANNAR:
        *ofs_x = -2;
        *ofs_y = -4;
        break;
    case TILEP_MONS_ARACHNE_STAVELESS:
        *ofs_x = -1;
        *ofs_y = -5;
        break;
    // Shift upwards and to the right.
    case TILEP_MONS_NECROMANCER:
    case TILEP_MONS_WIZARD:
    case TILEP_MONS_CLOUD_MAGE:
    case TILEP_MONS_MASTER_ELEMENTALIST:
    case TILEP_MONS_JESSICA:
    case TILEP_MONS_ANGEL:
        *ofs_x = 1;
        *ofs_y = -1;
        break;
    case TILEP_MONS_AGNES:
        *ofs_x = 0;
        *ofs_y = 1;
        break;
    case TILEP_MONS_TRAINING_DUMMY:
        *ofs_x = 3;
        *ofs_y = -1;
        break;
    case TILEP_MONS_MARGERY:
    case TILEP_MONS_FAUN:
        *ofs_x = 1;
        *ofs_y = -3;
        break;
    case TILEP_MONS_LOUISE:
    case TILEP_MONS_SATYR:
        *ofs_x = 1;
        *ofs_y = -4;
        break;
    case TILEP_MONS_HELL_WIZARD:
    case TILEP_MONS_HELL_WIZARD + 1:
    case TILEP_MONS_HELL_WIZARD + 2:
        *ofs_x = 2;
        *ofs_y = -2;
        break;
    case TILEP_MONS_HUMAN:
    case TILEP_MONS_ELF:
        *ofs_x = 2;
        *ofs_y = -3;
        break;
    // Shift downwards and to the left.
    case TILEP_MONS_GOBLIN:
    case TILEP_MONS_IJYB:
        *ofs_x = -2;
        *ofs_y = 4;
        break;
    case TILEP_MONS_SPRIGGAN_ENCHANTER:
        *ofs_x = -1;
        *ofs_y = 1;
        break;
    // Shift downwards and to the right.
    case TILEP_MONS_OGRE:
        *ofs_x = 1;
        *ofs_y = 1;
        break;
    case TILEP_MONS_TWO_HEADED_OGRE:
        *ofs_x = 1;
        *ofs_y = 2;
        break;
    case TILEP_MONS_ETTIN:
        *ofs_x = 2;
        *ofs_y = 1;
        break;
    case TILEP_MONS_FROST_GIANT:
        *ofs_x = 2;
        *ofs_y = 3;
        break;
    case TILEP_MONS_ZOMBIE_LARGE:
        *ofs_x = 4;
        *ofs_y = 1;
        break;
    case TILEP_MONS_ZOMBIE_SMALL:
        *ofs_x = 4;
        *ofs_y = 3;
        break;
    case TILEP_MONS_HILL_GIANT:
        *ofs_x = 6;
        *ofs_y = 2;
        break;
    default:
        // This monster cannot be displayed with a weapon.
        return false;
    }

    return true;
}

// Returns the amount of pixels necessary to shift a worn shield, like
// it's done with weapon.  No monster should have a shield hard-drawn
// on the tile.
bool mcache_monster::get_shield_offset(tileidx_t mon_tile,
                                       int *ofs_x, int *ofs_y)
{
    switch (mon_tile)
    {
    case TILEP_MONS_ORC:
    case TILEP_MONS_URUG:
    case TILEP_MONS_BLORK_THE_ORC:
    case TILEP_MONS_SAINT_ROKA:
    case TILEP_MONS_ORC_WARRIOR:
    case TILEP_MONS_ORC_KNIGHT:
    case TILEP_MONS_ORC_WARLORD:
        *ofs_x = 1;
        *ofs_y = 2;
        break;

    case TILEP_MONS_ARACHNE_STAVELESS:
        *ofs_x = -6;
        *ofs_y = 1;
        break;

    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_SKELETON_SMALL:
        *ofs_x = -2;
        *ofs_y = 1;
        break;

    case TILEP_MONS_HUMAN:
    case TILEP_MONS_DEEP_ELF_BLADEMASTER: // second weapon
    case TILEP_MONS_LOUISE:
    case TILEP_MONS_TENGU:
    case TILEP_MONS_TENGU_CONJURER:
    case TILEP_MONS_TENGU_WARRIOR:
    case TILEP_MONS_TENGU_REAVER:
    case TILEP_MONS_SOJOBO:
    case TILEP_MONS_FORMICID:
    case TILEP_MONS_FORMICID_DRONE:
        *ofs_x = 0;
        *ofs_y = 0;
        break;

    case TILEP_MONS_DONALD:
        *ofs_x = -1;
        *ofs_y = -1;
        break;

    case TILEP_MONS_TWO_HEADED_OGRE: // second weapon
        *ofs_x = 0;
        *ofs_y = 2;
        break;

    case TILEP_MONS_ETTIN: // second weapon
        *ofs_x = -2;
        *ofs_y = 1;
        break;

    case TILEP_MONS_SPRIGGAN:
    case TILEP_MONS_SPRIGGAN_DEFENDER:
    case TILEP_MONS_SPRIGGAN_BERSERKER:
    case TILEP_MONS_SPRIGGAN_ASSASSIN:
    case TILEP_MONS_SPRIGGAN_ENCHANTER:
        *ofs_x = 2;
        *ofs_y = 3;
        break;

    case TILEP_MONS_SPRIGGAN_DRUID:
        *ofs_x = 2;
        *ofs_y = -4;
        break;

    case TILEP_MONS_SPRIGGAN_AIR_MAGE:
        *ofs_x = 2;
        *ofs_y = -9;
        break;

    case TILEP_MONS_THE_ENCHANTRESS:
        *ofs_x = 6;
        *ofs_y = 3;
        break;

    case TILEP_MONS_SPRIGGAN_RIDER: // shield covered, out of picture
    default:
        // This monster cannot be displayed with a shield.
        return false;
    }

    return true;
}

int mcache_monster::info(tile_draw_info *dinfo) const
{
    int count = 0;
    dinfo[count++].set(m_mon_tile);

    int ofs_x, ofs_y;
    if (m_equ_tile && get_weapon_offset(m_mon_tile, &ofs_x, &ofs_y))
        dinfo[count++].set(m_equ_tile, ofs_x, ofs_y);

    if (m_shd_tile && get_shield_offset(m_mon_tile, &ofs_x, &ofs_y))
        dinfo[count++].set(m_shd_tile, ofs_x, ofs_y);

    return count;
}

bool mcache_monster::valid(const monster_info& mon)
{
    tileidx_t mon_tile = tileidx_monster(mon) & TILE_FLAG_MASK;

    int ox, oy;
    return (mon.inv[MSLOT_WEAPON].get() != NULL
            && get_weapon_offset(mon_tile, &ox, &oy))
        || (mon.inv[MSLOT_SHIELD].get() != NULL
            && get_shield_offset(mon_tile, &ox, &oy));
}

/////////////////////////////////////////////////////////////////////////////
// mcache_draco

mcache_draco::mcache_draco(const monster_info& mon)
{
    ASSERT(mcache_draco::valid(mon));

    m_mon_tile = tileidx_draco_base(mon);
    const item_info* mon_wep = mon.inv[MSLOT_WEAPON].get();
    m_equ_tile = (mon_wep != NULL) ? tilep_equ_weapon(*mon_wep) : 0;
    mon_wep = mon.inv[MSLOT_SHIELD].get();
    m_shd_tile = (mon_wep != NULL) ? tilep_equ_shield(*mon_wep) : 0;
    m_job_tile = tileidx_draco_job(mon);
}

int mcache_draco::info(tile_draw_info *dinfo) const
{
    int i = 0;

    dinfo[i++].set(m_mon_tile);
    if (m_job_tile)
        dinfo[i++].set(m_job_tile);
    if (m_equ_tile)
        dinfo[i++].set(m_equ_tile, -2, 0);
    if (m_shd_tile)
        dinfo[i++].set(m_shd_tile, 2, 0);

    return i;
}

bool mcache_draco::valid(const monster_info& mon)
{
    return mons_is_draconian(mon.type);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_ghost

mcache_ghost::mcache_ghost(const monster_info& mon)
{
    ASSERT(mcache_ghost::valid(mon));

    const uint32_t seed = hash32(&mon.mname[0], mon.mname.size())
                        ^ hash32(&mon.u.ghost, sizeof(mon.u.ghost));

    tilep_race_default(mon.u.ghost.species, 0, &m_doll);
    tilep_job_default(mon.u.ghost.job, &m_doll);

    for (int p = TILEP_PART_CLOAK; p < TILEP_PART_MAX; p++)
    {
        if (m_doll.parts[p] == TILEP_SHOW_EQUIP)
        {
            int part_offset = hash_rand(tile_player_part_count[p], seed, p);
            m_doll.parts[p] = tile_player_part_start[p] + part_offset;
        }
    }

    int ac = mon.u.ghost.ac;
    ac *= (5 + hash_rand(11, seed, 1000));
    ac /= 10;

    if (ac > 25)
        m_doll.parts[TILEP_PART_BODY] = TILEP_BODY_PLATE_BLACK;
    else if (ac > 18)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_CHAINMAIL;
    else if (ac > 12)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_SCALEMAIL;
    else if (ac > 5)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_LEATHER_HEAVY;
    else
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_ROBE_BLUE;

    int sk = mon.u.ghost.best_skill;
    int dam = mon.u.ghost.damage;
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
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SABRE;
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
        else if (dam > 10)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_HAMMER;
        else
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SPEAR;
        break;

    case SK_BOWS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_BOW2;
        break;

    case SK_CROSSBOWS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_CROSSBOW;
        break;

    case SK_SLINGS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SLING;
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

    m_demon.head = TILEP_DEMON_HEAD
                   + hash_rand(tile_player_count(TILEP_DEMON_HEAD), seed, 1);
    m_demon.body = TILEP_DEMON_BODY
                   + hash_rand(tile_player_count(TILEP_DEMON_BODY), seed, 2);

    if (minf.fly)
    {
        m_demon.wings = TILEP_DEMON_WINGS
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
