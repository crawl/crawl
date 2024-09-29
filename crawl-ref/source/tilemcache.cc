#include "AppHdr.h"

#ifdef USE_TILE
#include "tilemcache.h"

#include "colour.h"
#include "env.h"
#include "libutil.h"
#include "misc.h"
#include "mon-info.h"
#include "mon-util.h"
#include "mutant-beast.h"
#include "options.h"
#include "tile-flags.h"
#include "rltiles/tiledef-player.h"
#include "tiledoll.h"
#include "tilepick.h"
#include "tilepick-p.h"

mcache_manager mcache;

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

    static bool get_weapon_offset(tileidx_t mon_tile, int *ofs_x, int *ofs_y);
    static bool get_shield_offset(tileidx_t mon_tile, int *ofs_x, int *ofs_y);

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
};

class mcache_mbeast : public mcache_entry
{
public:
    mcache_mbeast(const monster_info& mon);

    virtual int info(tile_draw_info *dinfo) const override;

    static bool valid(const monster_info& mon);

protected:
    vector<tileidx_t> layers;
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

class mcache_armour : public mcache_entry
{
public:
    mcache_armour(const monster_info& minf);

    virtual int info(tile_draw_info *dinfo) const override;

    static bool valid(const monster_info& mon);

protected:
    tileidx_t m_mon_tile;
    tileidx_t m_arm_tile;
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

    if (minf.props.exists(MONSTER_TILE_KEY))
    {
        if (mcache_monster::valid(minf))
            entry = new mcache_monster(minf);
        else
            return 0;
    }
    else if (mcache_demon::valid(minf))
        entry = new mcache_demon(minf);
    else if (mcache_mbeast::valid(minf))
        entry = new mcache_mbeast(minf);
    else if (mcache_ghost::valid(minf))
        entry = new mcache_ghost(minf);
    else if (mcache_draco::valid(minf))
        entry = new mcache_draco(minf);
    else if (mcache_armour::valid(minf))
        entry = new mcache_armour(minf);
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

    const item_def* mon_weapon = mon.inv[MSLOT_WEAPON].get();
    m_equ_tile = (mon_weapon != nullptr) ? tilep_equ_weapon(*mon_weapon) : 0;
    if (!mons_class_wields_two_weapons(mon.type))
    {
        const item_def* mon_shield = mon.inv[MSLOT_SHIELD].get();
        m_shd_tile = (mon_shield != nullptr) ? tilep_equ_shield(*mon_shield) : 0;
        return;
    }
    const item_def* mon_weapon2 = mon.inv[MSLOT_ALT_WEAPON].get();
    if (!mon_weapon2)
    {
        m_shd_tile = 0;
        return;
    }
    m_shd_tile = mirror_weapon(*mon_weapon2);
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
    case TILEP_MONS_DEEP_ELF_ARCHER:
    case TILEP_MONS_DEEP_ELF_MASTER_ARCHER:
    case TILEP_MONS_DEEP_ELF_BLADEMASTER:
    case TILEP_MONS_HEADMASTER:
    case TILEP_MONS_CRIMSON_IMP:
    case TILEP_MONS_CERULEAN_IMP:
    case TILEP_MONS_IRON_IMP:
    case TILEP_MONS_SHADOW_IMP:
    case TILEP_MONS_HAROLD:
    case TILEP_MONS_JOSEPHINE:
    case TILEP_MONS_JOSEPH:
    case TILEP_MONS_TERENCE:
    case TILEP_MONS_CHERUB:
    case TILEP_MONS_MENNAS:
    case TILEP_MONS_PROFANE_SERVITOR:
    case TILEP_MONS_OCTOPODE:
    case TILEP_MONS_PLAYER_SHADOW_OCTOPODE:
    case TILEP_MONS_ZOMBIE_OCTOPODE:
    case TILEP_MONS_ANCESTOR:
    case TILEP_MONS_ANCESTOR_KNIGHT:
    case TILEP_MONS_ANCESTOR_BATTLEMAGE:
    case TILEP_MONS_RAGGED_HIEROPHANT:
    case TILEP_MONS_VAMPIRE_MAGE:
    case TILEP_MONS_NORRIS:
    case TILEP_MONS_BURIAL_ACOLYTE:
    case TILEP_MONS_WIGLAF:
    case TILEP_MONS_PLAYER_SHADOW_DRACONIAN:
    case TILEP_MONS_PLAYER_SHADOW_ONI:
        *ofs_x = 0;
        *ofs_y = 0;
        break;
    // Shift to the left.
    case TILEP_MONS_DEEP_ELF_DEATH_MAGE:
    case TILEP_MONS_DEMONSPAWN:
    case TILEP_MONS_PLAYER_SHADOW_DEMONSPAWN:
    case TILEP_MONS_DEMONSPAWN_BLOOD_SAINT:
    case TILEP_MONS_DEMONSPAWN_WARMONGER:
    case TILEP_MONS_DEMONSPAWN_CORRUPTER:
    case TILEP_MONS_DEMONSPAWN_SOUL_SCHOLAR:
    case TILEP_MONS_NEKOMATA:
    case TILEP_MONS_VAMPIRE_KNIGHT:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_JORGRUN:
        *ofs_x = -1;
        *ofs_y = 0;
        break;
    case TILEP_MONS_HOBGOBLIN:
    case TILEP_MONS_ROBIN:
    case TILEP_MONS_GNOLL_SERGEANT:
    case TILEP_MONS_JOSEPHINA:
    case TILEP_MONS_BALRUG:
        *ofs_x = -2;
        *ofs_y = 0;
        break;
    case TILEP_MONS_SKELETON_LARGE:
    case TILEP_MONS_ORC_WARLORD:
    case TILEP_MONS_FALLEN_ORC_WARLORD:
    case TILEP_MONS_KOBOLD_BRIGAND:
    case TILEP_MONS_KOBOLD:
    case TILEP_MONS_PLAYER_SHADOW_KOBOLD:
    case TILEP_MONS_EFREET:
    case TILEP_MONS_PLAYER_SHADOW_DJINN:
    case TILEP_MONS_GNOLL:
    case TILEP_MONS_PLAYER_SHADOW_GNOLL:
    case TILEP_MONS_CRAZY_YIUF:
    case TILEP_MONS_GRUM:
    case TILEP_MONS_GRUNN:
    case TILEP_MONS_GNOLL_BOUDA:
    case TILEP_MONS_NESSOS_BOWLESS:
    case TILEP_MONS_ARMATAUR:
    case TILEP_MONS_PLAYER_SHADOW_ARMATAUR:
    case TILEP_MONS_TENGU:
    case TILEP_MONS_PLAYER_SHADOW_TENGU:
    case TILEP_MONS_TENGU_CONJURER:
    case TILEP_MONS_TENGU_WARRIOR:
    case TILEP_MONS_TENGU_REAVER:
    case TILEP_MONS_SOJOBO:
    case TILEP_MONS_RAKSHASA:
    case TILEP_MONS_FENSTRIDER_WITCH:
        *ofs_x = -3;
        *ofs_y = 0;
        break;
    case TILEP_MONS_MAURICE:
        *ofs_x = -5;
        *ofs_y = 0;
        break;
    case TILEP_MONS_JACK_O_LANTERN:
        *ofs_x = -6;
        *ofs_y = 0;
        break;
    // Shift to the right.
    case TILEP_MONS_VAULT_GUARD:
    case TILEP_MONS_VAULT_WARDEN:
    case TILEP_MONS_VAULT_SENTINEL:
    case TILEP_MONS_IRONBOUND_CONVOKER:
    case TILEP_MONS_IRONBOUND_PRESERVER:
    case TILEP_MONS_GARGOYLE:
    case TILEP_MONS_PLAYER_SHADOW_GARGOYLE:
    case TILEP_MONS_MOLTEN_GARGOYLE:
    case TILEP_MONS_WAR_GARGOYLE:
    case TILEP_MONS_SERVANT_OF_WHISPERS:
        *ofs_x = 1;
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
    case TILEP_MONS_MARA:
        *ofs_x = 4;
        *ofs_y = -5;
        break;
    // Shift upwards.
    case TILEP_MONS_CENTAUR_WARRIOR_MELEE:
    case TILEP_MONS_DEEP_ELF_SORCERER:
    case TILEP_MONS_DEEP_ELF_HIGH_PRIEST:
    case TILEP_MONS_HELL_KNIGHT:
    case TILEP_MONS_DEATH_KNIGHT:
    case TILEP_MONS_MUMMY:
    case TILEP_MONS_PLAYER_SHADOW_MUMMY:
    case TILEP_MONS_MUMMY_PRIEST:
    case TILEP_MONS_GUARDIAN_MUMMY:
    case TILEP_MONS_SKELETON_SMALL:
    case TILEP_MONS_PSYCHE:
    case TILEP_MONS_DUVESSA:
    case TILEP_MONS_DUVESSA_1:
    case TILEP_MONS_JEREMIAH:
        *ofs_x = 0;
        *ofs_y = -1;
        break;
    case TILEP_MONS_MELIAI:
        *ofs_x = 1;
        *ofs_y = -1;
        break;
    case TILEP_MONS_SALAMANDER_MYSTIC:
        *ofs_x = 0;
        *ofs_y = -2;
        break;
    case TILEP_MONS_RED_DEVIL:
        *ofs_x = 0;
        *ofs_y = -3;
        break;
    case TILEP_MONS_NAGA:
    case TILEP_MONS_PLAYER_SHADOW_NAGA:
    case TILEP_MONS_NAGA_WARRIOR:
    case TILEP_MONS_NAGA_MAGE:
    case TILEP_MONS_NAGA_RITUALIST:
    case TILEP_MONS_NAGA_SHARPSHOOTER:
    case TILEP_MONS_VASHNIA:
        *ofs_x = 0;
        *ofs_y = -4;
        break;
    // Shift downwards.
    case TILEP_MONS_DEEP_ELF_KNIGHT:
    case TILEP_MONS_GUARDIAN_SERPENT:
    case TILEP_MONS_DEEP_DWARF:
    case TILEP_MONS_PLAYER_SHADOW_DWARF:
    case TILEP_MONS_RUST_DEVIL:
        *ofs_x = 0;
        *ofs_y = 1;
        break;
    case TILEP_MONS_DIMME:
    case TILEP_MONS_HALFLING:
    case TILEP_MONS_IMPERIAL_MYRMIDON:
        *ofs_x = 0;
        *ofs_y = 2;
        break;
    // Shift upwards and to the left.
    case TILEP_MONS_DEEP_ELF_AIR_MAGE:
    case TILEP_MONS_DEEP_ELF_FIRE_MAGE:
    case TILEP_MONS_DEEP_ELF_DEMONOLOGIST:
    case TILEP_MONS_DEEP_ELF_ANNIHILATOR:
    case TILEP_MONS_FRAVASHI:
    case TILEP_MONS_MINOTAUR:
    case TILEP_MONS_PLAYER_SHADOW_MINOTAUR:
    case TILEP_MONS_VAMPIRE:
    case TILEP_MONS_PLAYER_SHADOW_VAMPIRE:
        *ofs_x = -1;
        *ofs_y = -2;
        break;
    case TILEP_MONS_CENTAUR_MELEE:
    case TILEP_MONS_EUSTACHIO:
        *ofs_x = -3;
        *ofs_y = -3;
        break;
    case TILEP_MONS_DEMIGOD:
    case TILEP_MONS_PLAYER_SHADOW_DEMIGOD:
    case TILEP_MONS_KIRKE:
    case TILEP_MONS_FREDERICK:
        *ofs_x = -1;
        *ofs_y = -3;
        break;
    case TILEP_MONS_SONJA:
    case TILEP_MONS_ANCIENT_CHAMPION:
    case TILEP_MONS_ANTIQUE_CHAMPION:
    case TILEP_MONS_SKELETAL_WARRIOR:
    case TILEP_MONS_SALAMANDER:
    case TILEP_MONS_VINE_STALKER:
    case TILEP_MONS_PLAYER_SHADOW_VINE_STALKER:
    case TILEP_MONS_NECROPHAGE:
    case TILEP_MONS_WIGHT:
        *ofs_x = -2;
        *ofs_y = -2;
        break;
    case TILEP_MONS_SPRIGGAN_BERSERKER:
    case TILEP_MONS_BARACHI:
    case TILEP_MONS_PLAYER_SHADOW_BARACHI:
        *ofs_x = -3;
        *ofs_y = -2;
        break;
    case TILEP_MONS_RUPERT:
        *ofs_x = -3;
        *ofs_y = -3;
        break;
    case TILEP_MONS_SPRIGGAN_AIR_MAGE:
        *ofs_x = -3;
        *ofs_y = -5;
        break;
    case TILEP_MONS_AMAEMON:
        *ofs_x = -1;
        *ofs_y = -1;
        break;
    case TILEP_MONS_BOGGART:
    case TILEP_MONS_NECROMANCER:
    case TILEP_MONS_DEEP_ELF_ELEMENTALIST:
    case TILEP_MONS_DEEP_ELF_ELEMENTALIST_1:
    case TILEP_MONS_DEEP_ELF_ELEMENTALIST_2:
    case TILEP_MONS_DEEP_ELF_ELEMENTALIST_3:
    case TILEP_MONS_FORMICID:
    case TILEP_MONS_PLAYER_SHADOW_FORMICID:
    case TILEP_MONS_XAKKRIXIS:
        *ofs_x = -2;
        *ofs_y = -1;
        break;
    case TILEP_MONS_ARCANIST:
    case TILEP_MONS_OCCULTIST:
        *ofs_x = -2;
        *ofs_y = -2;
        break;
    case TILEP_MONS_FANNAR:
    case TILEP_MONS_DONALD:
    case TILEP_MONS_LOUISE:
        *ofs_x = -2;
        *ofs_y = -4;
        break;
    case TILEP_MONS_NAGARAJA:
        *ofs_x = -2;
        *ofs_y = -5;
        break;
    case TILEP_MONS_NIKOLA:
        *ofs_x = -4;
        *ofs_y = -3;
        break;
    case TILEP_MONS_IRONBOUND_FROSTHEART:
        *ofs_x = -4;
        *ofs_y = -1;
        break;
    case TILEP_MONS_ARACHNE_STAVELESS:
    case TILEP_MONS_MERFOLK_WATER:
    case TILEP_MONS_MERFOLK_AQUAMANCER_WATER:
    case TILEP_MONS_MERFOLK_IMPALER_WATER:
    case TILEP_MONS_MERFOLK_JAVELINEER_WATER:
    case TILEP_MONS_MERFOLK_SIREN_WATER:
    case TILEP_MONS_MERFOLK_AVATAR_WATER:
    case TILEP_MONS_ILSUIW_WATER:
        *ofs_x = -1;
        *ofs_y = -5;
        break;
    case TILEP_MONS_ASTERION:
        *ofs_x = -3;
        *ofs_y = -1;
        break;
    // Shift upwards and to the right.
    case TILEP_MONS_CLOUD_MAGE:
    case TILEP_MONS_MASTER_ELEMENTALIST:
    case TILEP_MONS_JESSICA:
    case TILEP_MONS_ANGEL:
    case TILEP_MONS_DAEVA:
    case TILEP_MONS_ANCESTOR_HEXER:
    case TILEP_MONS_MAGGIE:
        *ofs_x = 1;
        *ofs_y = -1;
        break;
    case TILEP_MONS_AGNES_STAVELESS:
        *ofs_x = 0;
        *ofs_y = 1;
        break;
    case TILEP_MONS_TRAINING_DUMMY:
        *ofs_x = 3;
        *ofs_y = -1;
        break;
    case TILEP_MONS_MARGERY:
    case TILEP_MONS_ERICA_SWORDLESS:
    case TILEP_MONS_FAUN:
        *ofs_x = 1;
        *ofs_y = -3;
        break;
    case TILEP_MONS_SATYR:
        *ofs_x = 1;
        *ofs_y = -4;
        break;
    case TILEP_MONS_MAD_ACOLYTE_OF_LUGONU:
        *ofs_x = -3;
        *ofs_y = -2;
        break;
    case TILEP_MONS_ELF:
    case TILEP_MONS_PLAYER_SHADOW_ELF:
        *ofs_x = 2;
        *ofs_y = -3;
        break;
    case TILEP_MONS_HUMAN:
    case TILEP_MONS_HUMAN_1:
    case TILEP_MONS_HUMAN_2:
    case TILEP_MONS_PLAYER_SHADOW_HUMAN:
    case TILEP_MONS_ZENATA:
    case TILEP_MONS_METEORAN:
        *ofs_x = -1;
        *ofs_y = -2;
        break;
    case TILEP_MONS_GHOST:
        *ofs_x = 3;
        *ofs_y = -4;
        break;
    case TILEP_MONS_MERFOLK:
    case TILEP_MONS_PLAYER_SHADOW_MERFOLK:
    case TILEP_MONS_MERFOLK_AQUAMANCER:
    case TILEP_MONS_MERFOLK_IMPALER:
    case TILEP_MONS_MERFOLK_JAVELINEER:
    case TILEP_MONS_MERFOLK_SIREN:
    case TILEP_MONS_MERFOLK_AVATAR:
    case TILEP_MONS_ILSUIW:
        *ofs_x = 1;
        *ofs_y = -5;
        break;
    case TILEP_MONS_FRANCES: // left-handed
                             // TODO: use a mirrored tile instead
        *ofs_x = 20;
        *ofs_y = -1;
        break;
    // Shift downwards and to the left.
    case TILEP_MONS_GOBLIN:
    case TILEP_MONS_PLAYER_SHADOW_COGLIN:
    case TILEP_MONS_IJYB:
        *ofs_x = -2;
        *ofs_y = 3;
        break;
    case TILEP_MONS_URUG:
    case TILEP_MONS_ORC_PRIEST:
    case TILEP_MONS_ORC_HIGH_PRIEST:
    case TILEP_MONS_SPRIGGAN:
    case TILEP_MONS_PLAYER_SHADOW_SPRIGGAN:
    case TILEP_MONS_SPRIGGAN_DEFENDER:
        *ofs_x = -1;
        *ofs_y = 1;
        break;
    case TILEP_MONS_ORC_KNIGHT:
    case TILEP_MONS_FALLEN_ORC_KNIGHT:
    case TILEP_MONS_ORC_WIZARD:
    case TILEP_MONS_ORC_SORCERER:
    case TILEP_MONS_NERGALLE:
    case TILEP_MONS_ETTIN:
    case TILEP_MONS_STONE_GIANT:
    case TILEP_MONS_TITAN:
    case TILEP_MONS_FROST_GIANT:
    case TILEP_MONS_FIRE_GIANT:
    case TILEP_MONS_IRON_GIANT:
    case TILEP_MONS_CACTUS_GIANT:
    case TILEP_MONS_ENCHANTRESS:
    case TILEP_MONS_ORC_APOSTLE:
    case TILEP_MONS_ORC_APOSTLE_1:
    case TILEP_MONS_ORC_APOSTLE_2:
        *ofs_x = -2;
        *ofs_y = 1;
        break;
    case TILEP_MONS_SAINT_ROKA:
        *ofs_x = -3;
        *ofs_y = 1;
        break;
    case TILEP_MONS_SERAPH_SWORDLESS:
        *ofs_x = -1;
        *ofs_y = -10;
        break;
    // Shift downwards and to the right.
    case TILEP_MONS_OGRE:
    case TILEP_MONS_SWAMP_OGRE:
    case TILEP_MONS_OGRE_MAGE:
    case TILEP_MONS_IRONBOUND_THUNDERHULK:
    case TILEP_MONS_LODUL:
        *ofs_x = 1;
        *ofs_y = 1;
        break;
    case TILEP_MONS_TWO_HEADED_OGRE:
        *ofs_x = 1;
        *ofs_y = 2;
        break;
    case TILEP_MONS_ZOMBIE_LARGE:
        *ofs_x = 4;
        *ofs_y = 1;
        break;
    case TILEP_MONS_ZOMBIE_SMALL:
        *ofs_x = 4;
        *ofs_y = 3;
        break;
    case TILEP_MONS_TROLL:
    case TILEP_MONS_PLAYER_SHADOW_TROLL:
        *ofs_x = -3;
        *ofs_y = 11;
        break;
    case TILEP_MONS_GHOUL:
    case TILEP_MONS_PLAYER_SHADOW_GHOUL:
        *ofs_x = -3;
        *ofs_y = -3;
        break;
    case TILEP_MONS_ORB_OF_FIRE:
        *ofs_x = 0;
        *ofs_y = 2;
        break;
    case TILEP_MONS_TENTACLED_MONSTROSITY:
    case TILEP_MONS_CIGOTUVIS_MONSTER:
        *ofs_x = -4;
        *ofs_y = 2;
        break;
    case TILEP_MONS_SERPENT_OF_HELL_GEHENNA:
    case TILEP_MONS_SERPENT_OF_HELL_COCYTUS:
    case TILEP_MONS_SERPENT_OF_HELL_DIS:
    case TILEP_MONS_SERPENT_OF_HELL_TARTARUS:
        *ofs_x = -4;
        *ofs_y = 3;
        break;
    case TILEP_MONS_UNSEEN_HORROR:
        *ofs_x = -4;
        *ofs_y = -4;
        break;
    case TILEP_MONS_UGLY_THING:
    case TILEP_MONS_UGLY_THING_1:
    case TILEP_MONS_UGLY_THING_2:
    case TILEP_MONS_UGLY_THING_3:
    case TILEP_MONS_UGLY_THING_4:
    case TILEP_MONS_UGLY_THING_5:
        *ofs_x = -4;
        *ofs_y = -7;
        break;
    case TILEP_MONS_VERY_UGLY_THING:
    case TILEP_MONS_VERY_UGLY_THING_1:
    case TILEP_MONS_VERY_UGLY_THING_2:
    case TILEP_MONS_VERY_UGLY_THING_3:
    case TILEP_MONS_VERY_UGLY_THING_4:
    case TILEP_MONS_VERY_UGLY_THING_5:
        *ofs_x = -3;
        *ofs_y = 4;
        break;
    case TILEP_MONS_ORANGE_STATUE:
        *ofs_x = 3;
        *ofs_y = -4;
        break;
    case TILEP_MONS_HELLION:
        *ofs_x = -1;
        *ofs_y = 4;
        break;
    case TILEP_MONS_CACODEMON:
        *ofs_x = -3;
        *ofs_y = -13;
        break;
    case TILEP_MONS_STARCURSED_MASS:
    case TILEP_MONS_EXECUTIONER:
        *ofs_x = -4;
        *ofs_y = 4;
        break;
    case TILEP_MONS_WRETCHED_STAR:
        *ofs_x = 1;
        *ofs_y = -4;
        break;
    case TILEP_MONS_ELDRITCH_TENTACLE_PORTAL_9:
        *ofs_x = -2;
        *ofs_y = -3;
        break;
    case TILEP_MONS_ICE_FIEND:
        *ofs_x = 1;
        *ofs_y = 2;
        break;
    case TILEP_MONS_PANDEMONIUM_LORD:
        *ofs_x = -2;
        *ofs_y = 0;
        break;
    case TILEP_MONS_HELL_SENTINEL:
        *ofs_x = -1;
        *ofs_y = 4;
        break;
    case TILEP_MONS_DEMONIC_PLANT:
        *ofs_x = -1;
        *ofs_y = 2;
        break;
    case TILEP_MONS_TENTACLED_STARSPAWN:
        *ofs_x = -1;
        *ofs_y = 1;
        break;
    case TILEP_MONS_ABOMINATION_LARGE_1:
        *ofs_x = -2;
        *ofs_y = -8;
        break;
    case TILEP_MONS_ABOMINATION_LARGE_2:
    case TILEP_MONS_BLORKULA_THE_ORCULA:
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
    case TILEP_MONS_ORC:
    case TILEP_MONS_ORC_WARRIOR:
    case TILEP_MONS_FALLEN_ORC_WARRIOR:
        *ofs_x = -2;
        *ofs_y = -1;
        break;
    case TILEP_MONS_GREAT_ORB_OF_EYES:
        *ofs_x = -2;
        *ofs_y = -2;
        break;
    case TILEP_MONS_WRAITH:
        *ofs_x = -4;
        *ofs_y = -4;
        break;
    case TILEP_MONS_ORB_GUARDIAN:
        *ofs_x = -1;
        *ofs_y = 2;
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
    case TILEP_MONS_KOBOLD_BLASTMINER: // left-handed
        *ofs_x = 17;
        *ofs_y = 5;
        break;
    case TILEP_MONS_KILLER_KLOWN:
        *ofs_x = 0;
        *ofs_y = 4;
        break;
    case TILEP_MONS_KILLER_KLOWN_1:
        *ofs_x = -4;
        *ofs_y = -1;
        break;
    case TILEP_MONS_KILLER_KLOWN_2:
        *ofs_x = -2;
        *ofs_y = 4;
        break;
    case TILEP_MONS_KILLER_KLOWN_3:
        *ofs_x = -2;
        *ofs_y = -5;
        break;
    case TILEP_MONS_KILLER_KLOWN_4:
        *ofs_x = 20;
        *ofs_y = -10;
        break;

    default:
        // This monster cannot be displayed with a weapon.
        return false;
    }

    return true;
}

// Returns the amount of pixels necessary to shift a worn shield, like
// it's done with weapon. No monster should have a shield hard-drawn
// on the tile.
bool mcache_monster::get_shield_offset(tileidx_t mon_tile,
                                       int *ofs_x, int *ofs_y)
{
    switch (mon_tile)
    {
    case TILEP_MONS_ORC:
    case TILEP_MONS_URUG:
    case TILEP_MONS_BLORKULA_THE_ORCULA:
    case TILEP_MONS_ORC_PRIEST:
    case TILEP_MONS_ORC_WARRIOR:
    case TILEP_MONS_ORC_KNIGHT:
    case TILEP_MONS_ORC_WARLORD:
    case TILEP_MONS_ZOMBIE_ORC:
    case TILEP_MONS_DEEP_ELF_KNIGHT:
    case TILEP_MONS_KIRKE:
    case TILEP_MONS_DIMME:
    case TILEP_MONS_ORC_APOSTLE_2:
        *ofs_x = 1;
        *ofs_y = 0;
        break;

    case TILEP_MONS_SAINT_ROKA:
    case TILEP_MONS_MINOTAUR:
    case TILEP_MONS_DEMONSPAWN:
    case TILEP_MONS_DEMONSPAWN_BLOOD_SAINT:
    case TILEP_MONS_DEMONSPAWN_WARMONGER:
    case TILEP_MONS_DEMONSPAWN_CORRUPTER:
    case TILEP_MONS_DEMONSPAWN_SOUL_SCHOLAR:
    case TILEP_MONS_NORRIS:
        *ofs_x = 2;
        *ofs_y = 0;
        break;

    case TILEP_MONS_ARACHNE_STAVELESS:
        *ofs_x = -6;
        *ofs_y = 1;
        break;

    case TILEP_MONS_ARMATAUR:
        *ofs_x = -9;
        *ofs_y = 2;
        break;

    case TILEP_MONS_WIGLAF:
    case TILEP_MONS_FENSTRIDER_WITCH:
        *ofs_x = -1;
        *ofs_y = 1;
        break;

    case TILEP_MONS_ZOMBIE_SMALL:
    case TILEP_MONS_SKELETON_SMALL:
    case TILEP_MONS_VAULT_GUARD:
    case TILEP_MONS_IRONBOUND_CONVOKER:
    case TILEP_MONS_IRONBOUND_PRESERVER:
    case TILEP_MONS_IRONBOUND_FROSTHEART:
    case TILEP_MONS_VAULT_WARDEN:
        *ofs_x = -2;
        *ofs_y = 1;
        break;

    case TILEP_MONS_HUMAN:
    case TILEP_MONS_HUMAN_1:
    case TILEP_MONS_HUMAN_2:
    case TILEP_MONS_DEEP_ELF_BLADEMASTER: // second weapon
    case TILEP_MONS_HEADMASTER:
    case TILEP_MONS_LOUISE:
    case TILEP_MONS_FORMICID:
    case TILEP_MONS_XAKKRIXIS:
    case TILEP_MONS_VINE_STALKER:
    case TILEP_MONS_OCTOPODE:
    case TILEP_MONS_CHERUB:
    case TILEP_MONS_MENNAS:
        *ofs_x = 0;
        *ofs_y = 0;
        break;

    case TILEP_MONS_DONALD:
    case TILEP_MONS_ANCESTOR_KNIGHT:
        *ofs_x = -1;
        *ofs_y = -1;
        break;

    case TILEP_MONS_GNOLL:
        *ofs_x = -1;
        *ofs_y = 1;
        break;

    case TILEP_MONS_TWO_HEADED_OGRE: // second weapon
    case TILEP_MONS_ANTIQUE_CHAMPION:
    case TILEP_MONS_ANCIENT_CHAMPION:
        *ofs_x = 0;
        *ofs_y = 2;
        break;

    case TILEP_MONS_NAGA:
    case TILEP_MONS_NAGA_WARRIOR:
    case TILEP_MONS_NAGARAJA:
        *ofs_x = -3;
        *ofs_y = 0;
        break;

    case TILEP_MONS_SPRIGGAN:
    case TILEP_MONS_SPRIGGAN_DEFENDER:
        *ofs_x = 2;
        *ofs_y = 3;
        break;

    case TILEP_MONS_SPRIGGAN_DRUID:
    case TILEP_MONS_SPRIGGAN_BERSERKER:
        *ofs_x = 2;
        *ofs_y = -4;
        break;

    case TILEP_MONS_SPRIGGAN_AIR_MAGE:
        *ofs_x = 2;
        *ofs_y = -1;
        break;

    case TILEP_MONS_ENCHANTRESS:
        *ofs_x = 6;
        *ofs_y = 3;
        break;

    case TILEP_MONS_ASTERION:
        *ofs_x = 1;
        *ofs_y = 1;
        break;

    case TILEP_MONS_SALAMANDER:
    case TILEP_MONS_BARACHI:
        *ofs_x = 0;
        *ofs_y = -1;
        break;

    case TILEP_MONS_GNOLL_SERGEANT:
    case TILEP_MONS_FREDERICK:
        *ofs_x = 1;
        *ofs_y = -1;
        break;

    case TILEP_MONS_ENTROPY_WEAVER:
    case TILEP_MONS_HELL_KNIGHT:
    case TILEP_MONS_DEATH_KNIGHT:
    case TILEP_MONS_METEORAN:
        *ofs_x = -3;
        *ofs_y = 2;
        break;

    case TILEP_MONS_KOBOLD:
        *ofs_x = 0;
        *ofs_y = 3;
        break;

    case TILEP_MONS_SONJA:
        *ofs_x = 3;
        *ofs_y = -10;
        break;

    case TILEP_MONS_MUMMY:
        *ofs_x = -6;
        *ofs_y = 0;
        break;

    case TILEP_MONS_MERFOLK:
    case TILEP_MONS_MERFOLK_AQUAMANCER:
    case TILEP_MONS_MERFOLK_IMPALER:
    case TILEP_MONS_MERFOLK_JAVELINEER:
    case TILEP_MONS_MERFOLK_SIREN:
    case TILEP_MONS_MERFOLK_AVATAR:
    case TILEP_MONS_ILSUIW:
    case TILEP_MONS_WRAITH:
        *ofs_x = -3;
        *ofs_y = -2;
        break;

    case TILEP_MONS_MAURICE:
        *ofs_x = -5;
        *ofs_y = 4;
        break;

    case TILEP_MONS_JACK_O_LANTERN:
        *ofs_x = -6;
        *ofs_y = 6;
        break;

    case TILEP_MONS_MERFOLK_WATER:
    case TILEP_MONS_MERFOLK_AQUAMANCER_WATER:
    case TILEP_MONS_MERFOLK_IMPALER_WATER:
    case TILEP_MONS_MERFOLK_JAVELINEER_WATER:
    case TILEP_MONS_MERFOLK_SIREN_WATER:
    case TILEP_MONS_MERFOLK_AVATAR_WATER:
    case TILEP_MONS_ILSUIW_WATER:
        *ofs_x = -5;
        *ofs_y = -2;
        break;

    case TILEP_MONS_TROLL:
        *ofs_x = 2;
        *ofs_y = 7;
        break;

    case TILEP_MONS_GHOUL:
    case TILEP_MONS_OGRE:
    case TILEP_MONS_SWAMP_OGRE:
    case TILEP_MONS_TENGU:
    case TILEP_MONS_TENGU_CONJURER:
    case TILEP_MONS_TENGU_WARRIOR:
    case TILEP_MONS_TENGU_REAVER:
    case TILEP_MONS_SOJOBO:
        *ofs_x = 2;
        *ofs_y = 1;
        break;

    case TILEP_MONS_GARGOYLE:
        *ofs_x = -8;
        *ofs_y = 1;
        break;

    case TILEP_MONS_CENTAUR_MELEE:
        *ofs_x = -8;
        *ofs_y = -4;
        break;

    case TILEP_MONS_HALFLING:
        *ofs_x = -2;
        *ofs_y = 5;
        break;

    case TILEP_MONS_DEEP_ELF_DEMONOLOGIST:
    case TILEP_MONS_DEEP_ELF_AIR_MAGE:
    case TILEP_MONS_DEEP_ELF_FIRE_MAGE:
        *ofs_x = 2;
        *ofs_y = 0;
        break;

    case TILEP_MONS_ORB_OF_FIRE:
        *ofs_x = -4;
        *ofs_y = 2;
        break;

    case TILEP_MONS_TENTACLED_MONSTROSITY:
    case TILEP_MONS_CIGOTUVIS_MONSTER:
    case TILEP_MONS_SERPENT_OF_HELL_GEHENNA:
    case TILEP_MONS_SERPENT_OF_HELL_COCYTUS:
    case TILEP_MONS_SERPENT_OF_HELL_DIS:
    case TILEP_MONS_SERPENT_OF_HELL_TARTARUS:
        *ofs_x = 4;
        *ofs_y = -1;
        break;

    case TILEP_MONS_UNSEEN_HORROR:
        *ofs_x = 5;
        *ofs_y = -1;
        break;

    case TILEP_MONS_UGLY_THING:
    case TILEP_MONS_UGLY_THING_1:
    case TILEP_MONS_UGLY_THING_2:
    case TILEP_MONS_UGLY_THING_3:
    case TILEP_MONS_UGLY_THING_4:
    case TILEP_MONS_UGLY_THING_5:
        *ofs_x = 4;
        *ofs_y = -1;
        break;

    case TILEP_MONS_VERY_UGLY_THING:
    case TILEP_MONS_VERY_UGLY_THING_1:
    case TILEP_MONS_VERY_UGLY_THING_2:
    case TILEP_MONS_VERY_UGLY_THING_3:
    case TILEP_MONS_VERY_UGLY_THING_4:
    case TILEP_MONS_VERY_UGLY_THING_5:
        *ofs_x = 4;
        *ofs_y = 5;
        break;

    case TILEP_MONS_ORANGE_STATUE:
        *ofs_x = -2;
        *ofs_y = -3;
        break;

    case TILEP_MONS_HELLION:
        *ofs_x = 0;
        *ofs_y = 4;
        break;

    case TILEP_MONS_CACODEMON:
        *ofs_x = 4;
        *ofs_y = -13;
        break;

    case TILEP_MONS_STARCURSED_MASS:
        *ofs_x = -3;
        *ofs_y = 2;
        break;

    case TILEP_MONS_WRETCHED_STAR:
    case TILEP_MONS_JEREMIAH:
        *ofs_x = 1;
        *ofs_y = -4;
        break;

    case TILEP_MONS_ELDRITCH_TENTACLE_PORTAL_9:
        *ofs_x = 3;
        *ofs_y = 0;
        break;

    case TILEP_MONS_EXECUTIONER:
        *ofs_x = 2;
        *ofs_y = 4;
        break;

    case TILEP_MONS_ICE_FIEND:
        *ofs_x = -2;
        *ofs_y = 2;
        break;

    case TILEP_MONS_PANDEMONIUM_LORD:
    case TILEP_MONS_ANGEL:
    case TILEP_MONS_DAEVA:
        *ofs_x = 1;
        *ofs_y = 1;
        break;

    case TILEP_MONS_HELL_SENTINEL:
        *ofs_x = 4;
        *ofs_y = -5;
        break;

    case TILEP_MONS_DEMONIC_PLANT:
        *ofs_x = 5;
        *ofs_y = 2;
        break;

    case TILEP_MONS_TENTACLED_STARSPAWN:
        *ofs_x = 1;
        *ofs_y = 1;
        break;

    case TILEP_MONS_GOBLIN:
    case TILEP_MONS_IJYB:
        *ofs_x = 1;
        *ofs_y = 4;
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

    case TILEP_MONS_GREAT_ORB_OF_EYES:
    case TILEP_MONS_GHOST:
        *ofs_x = 2;
        *ofs_y = -2;
        break;

    case TILEP_MONS_FRANCES: // left-handed - shield goes in right hand
        *ofs_x = -20;
        *ofs_y = -1;
        break;

    case TILEP_MONS_ORB_GUARDIAN:
    case TILEP_MONS_ETTIN: // second weapon
        *ofs_x = 1;
        *ofs_y = 0;
        break;

    case TILEP_MONS_ORB_GUARDIAN_FETUS:
        *ofs_x = -8;
        *ofs_y = 5;
        break;

    case TILEP_MONS_JORMUNGANDR:
        *ofs_x = -2;
        *ofs_y = 2;
        break;
    case TILEP_MONS_SERAPH:
        *ofs_x = -2;
        *ofs_y = -7;
        break;
    case TILEP_MONS_LAMIA:
        *ofs_x = -2;
        *ofs_y = -5;
        break;
    case TILEP_MONS_GIAGGOSTUONO:
        *ofs_x = 2;
        *ofs_y = -7;
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
        else if (get_weapon_offset(m_mon_tile, &ofs_x, &ofs_y))
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
        else if (get_shield_offset(m_mon_tile, &ofs_x, &ofs_y))
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
        || get_weapon_offset(mon_tile, &ox, &oy);
    bool have_shield_offs = (mon.type == MONS_PLAYER
                             && Options.tile_shield_offsets.first != INT_MAX)
        || get_shield_offset(mon_tile, &ox, &oy);
    // Return true if the tile has a weapon offset and has a weapon,
    // a shield offset and a shield, or is a dual-wielder and has a
    // shield offset and an off-hand weapon (a valid edge case)
    return have_weapon_offs && mon.inv[MSLOT_WEAPON]
           || have_shield_offs
              && (mon.inv[MSLOT_SHIELD]
                  || mon.inv[MSLOT_ALT_WEAPON]
                     && mons_class_wields_two_weapons(mon.type));
}

/////////////////////////////////////////////////////////////////////////////
// mcache_draco

mcache_draco::mcache_draco(const monster_info& mon)
{
    ASSERT(mcache_draco::valid(mon));

    m_mon_tile = tileidx_draco_base(mon);
    const item_def* mon_wep = mon.inv[MSLOT_WEAPON].get();
    m_equ_tile = (mon_wep != nullptr) ? tilep_equ_weapon(*mon_wep) : 0;
    mon_wep = mon.inv[MSLOT_SHIELD].get();
    m_shd_tile = (mon_wep != nullptr) ? tilep_equ_shield(*mon_wep) : 0;
    m_job_tile = tileidx_draco_job(mon);
}

int mcache_draco::info(tile_draw_info *dinfo) const
{
    int i = 0;

    dinfo[i++].set(m_mon_tile);
    if (m_job_tile)
        dinfo[i++].set(m_job_tile);
    if (m_equ_tile)
        dinfo[i++].set(m_equ_tile, -3, -1);
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
                        ^ hash32(&mon.i_ghost, sizeof(mon.i_ghost));

    m_doll.parts[TILEP_PART_BASE] = TILEP_SHOW_EQUIP;
    tilep_race_default(mon.i_ghost.species, 0, &m_doll);
    tilep_job_default(mon.i_ghost.job, &m_doll);

    for (int p = TILEP_PART_CLOAK; p < TILEP_PART_MAX; p++)
    {
        if (m_doll.parts[p] == TILEP_SHOW_EQUIP)
        {
            int part_offset = hash_with_seed(tile_player_part_count[p], seed, p);
            m_doll.parts[p] = tile_player_part_start[p] + part_offset;
        }
    }

    int ac = mon.i_ghost.ac;
    ac *= (5 + hash_with_seed(11, seed, 1000));
    ac /= 10;

    // Become uncannily spooky!
    if (!Options.tile_grinch && today_is_halloween())
        m_doll.parts[TILEP_PART_HELM] = TILEP_HELM_PUMPKIN;
    else if (m_doll.parts[TILEP_PART_HELM] == TILEP_HELM_PUMPKIN)
        m_doll.parts[TILEP_PART_HELM] = TILEP_HELM_FIRST_NORM; // every day is *not* halloween


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
    dam *= (5 + hash_with_seed(11, seed, 1001));
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

    case SK_RANGED_WEAPONS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SHORTBOW;
        break;

#if TAG_MAJOR_VERSION == 34
    case SK_CROSSBOWS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_ARBALEST;
        break;

    case SK_SLINGS:
        m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SLING;
        break;
#endif

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
// mcache_armour
mcache_armour::mcache_armour(const monster_info& mon)
{
    ASSERT(mcache_armour::valid(mon));

    m_mon_tile = tileidx_monster(mon) & TILE_FLAG_MASK;

    const item_def* mon_armour = mon.inv[MSLOT_ARMOUR].get();
    if (mon_armour)
        m_arm_tile = tilep_equ_armour(*mon_armour);
    else
        m_arm_tile = 0;
}

int mcache_armour::info(tile_draw_info *dinfo) const
{
    int count = 0;
    dinfo[count++].set(m_mon_tile);
    if (m_arm_tile)
        dinfo[count++].set(m_arm_tile);
    return count;
}

bool mcache_armour::valid(const monster_info& mon)
{
    return mon.type == MONS_ANIMATED_ARMOUR;
}

/////////////////////////////////////////////////////////////////////////////
// mcache_demon

mcache_demon::mcache_demon(const monster_info& minf)
{
    ASSERT(mcache_demon::valid(minf));

    const uint32_t seed = hash32(&minf.mname[0], minf.mname.size());

    m_demon.head = tile_player_coloured(TILEP_DEMON_HEAD,
                                        element_colour(minf.colour()))
        + hash_with_seed(tile_player_count(TILEP_DEMON_HEAD), seed, 1);
    m_demon.body = tile_player_coloured(TILEP_DEMON_BODY,
                                        element_colour(minf.colour()))
        + hash_with_seed(tile_player_count(TILEP_DEMON_BODY), seed, 2);

    if (minf.is(MB_AIRBORNE))
    {
        m_demon.wings = tile_player_coloured(TILEP_DEMON_WINGS,
                                             element_colour(minf.colour()))
            + hash_with_seed(tile_player_count(TILEP_DEMON_WINGS), seed, 3);
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

/////////////////////////////////////////////////////////////////////////////
// mcache_mbeast

mcache_mbeast::mcache_mbeast(const monster_info& minf)
{
    ASSERT(mcache_mbeast::valid(minf));

    // the layer order should be:
    // base < fire < bwing < ox < weird < sting < twing < shock < horn
    // (lowest to highest)

    const int tier
        = mutant_beast_tier(minf.props[MUTANT_BEAST_TIER].get_short()) - 1;
    ASSERT(tier >= 0);
    ASSERT(tier < (short)tile_player_count(TILEP_MUTANT_BEAST_BASE));
    layers.push_back(TILEP_MUTANT_BEAST_BASE + tier);

    int facets = 0; // bitfield
    for (auto facet : minf.props[MUTANT_BEAST_FACETS].get_vector())
        facets |= (1 << facet.get_int());

    // an ordered list of tiers and corresponding tiles to add.
    // tiers may appear repeatedly, if they have multiple layers.
    static const vector<pair<beast_facet, tileidx_t>> facet_layers = {
        { BF_FIRE,  TILEP_MUTANT_BEAST_FIRE },
        { BF_BAT,   TILEP_MUTANT_BEAST_WING_BASE },
        { BF_OX,    TILEP_MUTANT_BEAST_OX },
        { BF_WEIRD, TILEP_MUTANT_BEAST_WEIRD },
        { BF_STING, TILEP_MUTANT_BEAST_STING },
        { BF_BAT,   TILEP_MUTANT_BEAST_WING_TOP },
        { BF_SHOCK, TILEP_MUTANT_BEAST_SHOCK },
        { BF_OX,    TILEP_MUTANT_BEAST_HORN },
    };

    for (auto &facet_layer : facet_layers)
        if (facets & (1 << facet_layer.first))
            layers.emplace_back(facet_layer.second + tier);
}

int mcache_mbeast::info(tile_draw_info *dinfo) const
{
    for (int i = 0; i < (int)layers.size(); ++i)
        dinfo[i].set(layers[i]);
    return layers.size();
}

bool mcache_mbeast::valid(const monster_info& mon)
{
    return mon.type == MONS_MUTANT_BEAST;
}

#endif
