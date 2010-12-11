#include "AppHdr.h"

#ifdef USE_TILE
#include "tilemcache.h"

#include "env.h"
#include "ghost.h"
#include "mon-util.h"
#include "tags.h"
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

// Custom marshall/unmarshall functions.
static void unmarshallDoll(reader &th, dolls_data &doll)
{
    for (unsigned int i = 0; i < TILEP_PART_MAX; i++)
        doll.parts[i] = unmarshallInt(th);
}

static void marshallDoll(writer &th, const dolls_data &doll)
{
    for (unsigned int i = 0; i < TILEP_PART_MAX; i++)
        marshallInt(th, doll.parts[i]);
}

static void unmarshallDemon(reader &th, demon_data &demon)
{
    demon.head = unmarshallInt(th);
    demon.body = unmarshallInt(th);
    demon.wings = unmarshallInt(th);
}

static void marshallDemon(writer &th, const demon_data &demon)
{
    marshallInt(th, demon.head);
    marshallInt(th, demon.body);
    marshallInt(th, demon.wings);
}

// Internal mcache classes.  The mcache_manager creates these internally.
// The only access external clients need is through the virtual
// info function.

class mcache_monster : public mcache_entry
{
public:
    mcache_monster(const monster* mon);
    mcache_monster(reader &th);

    virtual int info(tile_draw_info *dinfo) const;

    static bool valid(const monster* mon);

    static bool get_weapon_offset(tileidx_t mon_tile, int *ofs_x, int *ofs_y);

    virtual void construct(writer &th);

protected:
    tileidx_t m_mon_tile;
    tileidx_t m_equ_tile;
};

class mcache_draco : public mcache_entry
{
public:
    mcache_draco(const monster* mon);
    mcache_draco(reader &th);

    virtual int info(tile_draw_info *dinfo) const;

    static bool valid(const monster* mon);

    virtual void construct(writer &th);

protected:
    tileidx_t m_mon_tile;
    tileidx_t m_job_tile;
    tileidx_t m_equ_tile;
};

class mcache_ghost : public mcache_entry
{
public:
    mcache_ghost(const monster* mon);
    mcache_ghost(reader &th);

    virtual const dolls_data *doll() const;

    static bool valid(const monster* mon);

    virtual void construct(writer &th);

    virtual bool transparent() const;

protected:
    dolls_data m_doll;
};

class mcache_demon : public mcache_entry
{
public:
    mcache_demon(const monster* mon);
    mcache_demon(reader &th);

    virtual int info(tile_draw_info *dinfo) const;

    static bool valid(const monster* mon);

    virtual void construct(writer &th);

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

unsigned int mcache_manager::register_monster(const monster* mon)
{
    ASSERT(mon);
    if (!mon)
        return 0;

    // TODO enne - is it worth it to search against all mcache entries?
    // TODO enne - pool mcache types to avoid too much alloc/dealloc?

    mcache_entry *entry;

    if (mcache_demon::valid(mon))
        entry = new mcache_demon(mon);
    else if (mcache_ghost::valid(mon))
        entry = new mcache_ghost(mon);
    else if (mcache_draco::valid(mon))
        entry = new mcache_draco(mon);
    else if (mcache_monster::valid(mon))
        entry = new mcache_monster(mon);
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

    return (TILEP_MCACHE_START + idx);
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
        return (NULL);

    if (idx >= TILEP_MCACHE_START + m_entries.size())
        return (NULL);

    mcache_entry *entry = m_entries[idx - TILEP_MCACHE_START];
    return (entry);
}

void mcache_manager::read(reader &th)
{
    unsigned int size = unmarshallInt(th);
    m_entries.reserve(size);
    m_entries.clear();

    for (unsigned int i = 0; i < size; i++)
    {
        char type = unmarshallByte(th);

        mcache_entry *entry;
        switch (type)
        {
        case MCACHE_MONSTER:
            entry = new mcache_monster(th);
            break;
        case MCACHE_DRACO:
            entry = new mcache_draco(th);
            break;
        case MCACHE_GHOST:
            entry = new mcache_ghost(th);
            break;
        case MCACHE_DEMON:
            entry = new mcache_demon(th);
            break;
        default:
            ASSERT(!"Invalid streamed mcache type.");
        case MCACHE_NULL:
            entry = NULL;
            break;
        }

        m_entries.push_back(entry);
    }
}

void mcache_manager::construct(writer &th)
{
    marshallInt(th, m_entries.size());
    for (unsigned int i = 0; i < m_entries.size(); i++)
    {
        if (m_entries[i] == NULL)
        {
            marshallByte(th, MCACHE_NULL);
            continue;
        }

        if (dynamic_cast<mcache_monster*>(m_entries[i]))
            marshallByte(th, MCACHE_MONSTER);
        else if (dynamic_cast<mcache_draco*>(m_entries[i]))
            marshallByte(th, MCACHE_DRACO);
        else if (dynamic_cast<mcache_ghost*>(m_entries[i]))
            marshallByte(th, MCACHE_GHOST);
        else if (dynamic_cast<mcache_demon*>(m_entries[i]))
            marshallByte(th, MCACHE_DEMON);
        else
        {
            marshallByte(th, MCACHE_NULL);
            continue;
        }

        m_entries[i]->construct(th);
    }
}

/////////////////////////////////////////////////////////////////////////////
// mcache_entry

mcache_entry::mcache_entry(reader &th)
{
    m_ref_count = unmarshallInt(th);
}

void mcache_entry::construct(writer &th)
{
    marshallInt(th, m_ref_count);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_monster

mcache_monster::mcache_monster(const monster* mon)
{
    ASSERT(mcache_monster::valid(mon));

    m_mon_tile = tileidx_monster(mon) & TILE_FLAG_MASK;

    const int mon_wep = mon->inv[MSLOT_WEAPON];
    m_equ_tile = tilep_equ_weapon(mitm[mon_wep]);
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
    case TILEP_MONS_VAULT_GUARD:
    case TILEP_MONS_DEEP_ELF_MASTER_ARCHER:
    case TILEP_MONS_DEEP_ELF_BLADEMASTER:
    case TILEP_MONS_IMP:
    case TILEP_MONS_IRON_IMP:
    case TILEP_MONS_SHADOW_IMP:
    case TILEP_MONS_NORRIS:
    case TILEP_MONS_MAUD:
    case TILEP_MONS_EDMUND:
    case TILEP_MONS_FRANCES:
    case TILEP_MONS_HAROLD:
    case TILEP_MONS_JOSEPH:
    case TILEP_MONS_JOZEF:
    case TILEP_MONS_RUPERT:
    case TILEP_MONS_TERENCE:
    case TILEP_MONS_WIGLAF:
    case TILEP_MONS_FREDERICK:
    case TILEP_MONS_RAKSHASA:
    case TILEP_MONS_RAKSHASA_FAKE:
    case TILEP_MONS_VAMPIRE_KNIGHT:
    case TILEP_MONS_SKELETAL_WARRIOR:
    case TILEP_MONS_ANGEL:
    case TILEP_MONS_CHERUB:
    case TILEP_MONS_MERFOLK:
    case TILEP_MONS_MERFOLK_WATER:
    case TILEP_MONS_MERFOLK_JAVELINEER:
    case TILEP_MONS_MERFOLK_JAVELINEER_WATER:
    case TILEP_MONS_MERFOLK_IMPALER:
    case TILEP_MONS_MERFOLK_IMPALER_WATER:
    case TILEP_MONS_MERFOLK_AQUAMANCER:
    case TILEP_MONS_MERFOLK_AQUAMANCER_WATER:
    case TILEP_MONS_MERMAID:
    case TILEP_MONS_MERMAID_WATER:
    case TILEP_MONS_SIREN:
    case TILEP_MONS_SIREN_WATER:
    case TILEP_MONS_ILSUIW:
    case TILEP_MONS_ILSUIW_WATER:
    case TILEP_MONS_SPRIGGAN:
    case TILEP_MONS_KENKU:
        *ofs_x = 0;
        *ofs_y = 0;
        break;
    // Shift to the left.
    case TILEP_MONS_GNOLL:
    case TILEP_MONS_GRUM:
    case TILEP_MONS_CRAZY_YIUF:
    case TILEP_MONS_DEEP_ELF_DEATH_MAGE:
    case TILEP_MONS_SPRIGGAN_DEFENDER:
    case TILEP_MONS_SPRIGGAN_BERSERKER:
        *ofs_x = -1;
        *ofs_y = 0;
        break;
    case TILEP_MONS_HOBGOBLIN:
    case TILEP_MONS_TIAMAT:
        *ofs_x = -2;
        *ofs_y = 0;
        break;
    case TILEP_MONS_HILL_GIANT:
        *ofs_x = -3;
        *ofs_y = 0;
        break;
    // Shift to the right.
    case TILEP_MONS_DEMONSPAWN:
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
    // Shift upwards.
    case TILEP_MONS_CENTAUR_WARRIOR_MELEE:
    case TILEP_MONS_DEEP_ELF_SORCERER:
    case TILEP_MONS_DEEP_ELF_HIGH_PRIEST:
        *ofs_x = 0;
        *ofs_y = -1;
        break;
    case TILEP_MONS_MIDGE:
        *ofs_x = 0;
        *ofs_y = -2;
        break;
    // Shift downwards.
    case TILEP_MONS_DEEP_ELF_KNIGHT:
    case TILEP_MONS_NAGA:
    case TILEP_MONS_GREATER_NAGA:
    case TILEP_MONS_NAGA_WARRIOR:
    case TILEP_MONS_GUARDIAN_SERPENT:
    case TILEP_MONS_NAGA_MAGE:
    case TILEP_MONS_THE_ENCHANTRESS:
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
    case TILEP_MONS_DEEP_ELF_SOLDIER:
        *ofs_x = 0;
        *ofs_y = 2;
        break;
    case TILEP_MONS_GOBLIN:
    case TILEP_MONS_IJYB:
        *ofs_x = -2;
        *ofs_y = 4;
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
        *ofs_x = -1;
        *ofs_y = -3;
        break;
    case TILEP_MONS_MAURICE:
        *ofs_x = -2;
        *ofs_y = -2;
        break;
    case TILEP_MONS_SONJA:
        *ofs_x = -2;
        *ofs_y = -2;
        break;
    // Shift upwards and to the right.
    case TILEP_MONS_AGNES:
        *ofs_x = 1;
        *ofs_y = -3;
        break;
    case TILEP_MONS_WIZARD:
        *ofs_x = 2;
        *ofs_y = -2;
        break;
    case TILEP_MONS_RED_DEVIL:
        *ofs_x = 2;
        *ofs_y = -3;
        break;
    // Shift downwards and to the left.
    case TILEP_MONS_HELL_KNIGHT:
        *ofs_x = -1;
        *ofs_y = 3;
        break;
    // Shift downwards and to the right.
    case TILEP_MONS_BIG_KOBOLD:
        *ofs_x = -1;
        *ofs_y = 0;
        break;
    case TILEP_MONS_KOBOLD:
        *ofs_x = 0;
        *ofs_y = 0;
        break;
    case TILEP_MONS_ELF:
    case TILEP_MONS_ZOMBIE_LARGE:
        *ofs_x = 4;
        *ofs_y = 1;
        break;
    case TILEP_MONS_ZOMBIE_SMALL:
        *ofs_x = 4;
        *ofs_y = 3;
        break;
    case TILEP_MONS_HALFLING:
        *ofs_x = 4;
        *ofs_y = 2;
        break;
    case TILEP_MONS_HUMAN:
        *ofs_x = 5;
        *ofs_y = 2;
        break;
    default:
        // This monster cannot be displayed with a weapon.
        return (false);
    }

    return (true);
}

int mcache_monster::info(tile_draw_info *dinfo) const
{
    int ofs_x, ofs_y;
    get_weapon_offset(m_mon_tile, &ofs_x, &ofs_y);

    int count = 0;
    dinfo[count++].set(m_mon_tile);
    if (m_equ_tile)
        dinfo[count++].set(m_equ_tile, ofs_x, ofs_y);

    // In some cases, overlay a second weapon tile...
    if (m_mon_tile == TILEP_MONS_DEEP_ELF_BLADEMASTER)
    {
        tileidx_t eq2;
        switch (m_equ_tile)
        {
            case TILEP_HAND1_DAGGER:
                eq2 = TILEP_HAND2_DAGGER;
                break;
            case TILEP_HAND1_SABRE:
                eq2 = TILEP_HAND2_SABRE;
                break;
            default:
            case TILEP_HAND1_SHORT_SWORD_SLANT:
                eq2 = TILEP_HAND2_SHORT_SWORD_SLANT;
                break;
        };

        if (eq2)
            dinfo[count++].set(eq2, -ofs_x, ofs_y);
    }

    return (count);
}

bool mcache_monster::valid(const monster* mon)
{
    if (!mon)
        return (false);
    int mon_wep = mon->inv[MSLOT_WEAPON];
    if (mon_wep == NON_ITEM)
        return (false);

    tileidx_t mon_tile = tileidx_monster(mon) & TILE_FLAG_MASK;

    int ox, oy;
    return get_weapon_offset(mon_tile, &ox, &oy);
}

mcache_monster::mcache_monster(reader &th) : mcache_entry(th)
{
    m_mon_tile = unmarshallInt(th);
    m_equ_tile = unmarshallInt(th);
}

void mcache_monster::construct(writer &th)
{
    mcache_entry::construct(th);

    marshallInt(th, m_mon_tile);
    marshallInt(th, m_equ_tile);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_draco

mcache_draco::mcache_draco(const monster* mon)
{
    ASSERT(mcache_draco::valid(mon));

    m_mon_tile = tileidx_draco_base(mon);
    int mon_wep = mon->inv[MSLOT_WEAPON];
    m_equ_tile = (mon_wep != NON_ITEM) ? tilep_equ_weapon(mitm[mon_wep]) : 0;
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

    return i;
}

bool mcache_draco::valid(const monster* mon)
{
    return (mon && mons_is_draconian(mon->type));
}

mcache_draco::mcache_draco(reader &th) : mcache_entry(th)
{
    m_mon_tile = unmarshallInt(th);
    m_job_tile = unmarshallInt(th);
    m_equ_tile = unmarshallInt(th);
}

void mcache_draco::construct(writer &th)
{
    mcache_entry::construct(th);

    marshallInt(th, m_mon_tile);
    marshallInt(th, m_job_tile);
    marshallInt(th, m_equ_tile);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_ghost

mcache_ghost::mcache_ghost(const monster* mon)
{
    ASSERT(mcache_ghost::valid(mon));

    const struct ghost_demon &ghost = *mon->ghost;

    unsigned int pseudo_rand = ghost.max_hp * 54321 * 54321;

    tilep_race_default(ghost.species, ghost.xl, &m_doll);
    tilep_job_default(ghost.job, &m_doll);

    for (int p = TILEP_PART_CLOAK; p < TILEP_PART_MAX; p++)
    {
        if (m_doll.parts[p] == TILEP_SHOW_EQUIP)
        {
            int part_offset = pseudo_rand % tile_player_part_count[p];
            m_doll.parts[p] = tile_player_part_start[p] + part_offset;
        }
    }

    int ac = ghost.ac;
    ac *= (5 + (pseudo_rand / 11) % 11);
    ac /= 10;

    if (ac > 25)
        m_doll.parts[TILEP_PART_BODY] = TILEP_BODY_PLATE_BLACK;
    else if (ac > 20)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_BANDED;
    else if (ac > 15)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_SCALEMAIL;
    else if (ac > 10)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_CHAINMAIL;
    else if (ac > 5)
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_LEATHER_HEAVY;
    else
        m_doll.parts[TILEP_PART_BODY]= TILEP_BODY_ROBE_BLUE;

    int sk = ghost.best_skill;
    int dam = ghost.damage;
    dam *= (5 + pseudo_rand % 11);
    dam /= 10;

    switch (sk)
    {
    case SK_MACES_FLAILS:
        if (dam > 30)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_GREAT_FLAIL;
        else if (dam > 25)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_GREAT_MACE;
        else if (dam > 20)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SPIKED_FLAIL;
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

bool mcache_ghost::valid(const monster* mon)
{
    return (mon && mons_is_pghost(mon->type));
}

mcache_ghost::mcache_ghost(reader &th) : mcache_entry(th)
{
    unmarshallDoll(th, m_doll);
}

void mcache_ghost::construct(writer &th)
{
    mcache_entry::construct(th);

    marshallDoll(th, m_doll);
}

bool mcache_ghost::transparent() const
{
    return (true);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_demon

mcache_demon::mcache_demon(const monster* mon)
{
    ASSERT(mcache_demon::valid(mon));

    const struct ghost_demon &ghost = *mon->ghost;

    unsigned int pseudo_rand1 = ghost.max_hp * 54321 * 54321;
    unsigned int pseudo_rand2 = ghost.ac * 54321 * 54321;
    unsigned int pseudo_rand3 = ghost.ev * 54321 * 54321;

    int head_offset = pseudo_rand1 % tile_player_count(TILEP_DEMON_HEAD);
    m_demon.head = TILEP_DEMON_HEAD + head_offset;

    int body_offset = pseudo_rand2 % tile_player_count(TILEP_DEMON_BODY);
    m_demon.body = TILEP_DEMON_BODY + body_offset;

    if (ghost.ev % 2)
    {
        int wings_offset = pseudo_rand3 % tile_player_count(TILEP_DEMON_WINGS);
        m_demon.wings = TILEP_DEMON_WINGS + wings_offset;
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

bool mcache_demon::valid(const monster* mon)
{
    return (mon && mon->type == MONS_PANDEMONIUM_DEMON);
}

mcache_demon::mcache_demon(reader &th) : mcache_entry(th)
{
    unmarshallDemon(th, m_demon);
}

void mcache_demon::construct(writer &th)
{
    mcache_entry::construct(th);

    marshallDemon(th, m_demon);
}

#endif
