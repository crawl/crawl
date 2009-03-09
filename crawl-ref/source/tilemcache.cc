#include "AppHdr.h"
REVISION("$Rev$");

#include "tilemcache.h"
#include "tags.h"
#include "ghost.h"
#include "mon-util.h"

mcache_manager mcache;

// Used internally for streaming.
enum mcache_type
{
    MCACHE_MONSTER,
    MCACHE_DRACO,
    MCACHE_GHOST,
    MCACHE_DEMON,
    MCACHE_MAX,

    MCACHE_NULL
};

// Custom marshall/unmarshall functions.
static void unmarshallDoll(reader &th, dolls_data &doll)
{
    for (unsigned int i = 0; i < TILEP_PART_MAX; i++)
        doll.parts[i] = unmarshallLong(th);
}

static void marshallDoll(writer &th, const dolls_data &doll)
{
    for (unsigned int i = 0; i < TILEP_PART_MAX; i++)
        marshallLong(th, doll.parts[i]);
}

static void unmarshallDemon(reader &th, demon_data &demon)
{
    demon.head = unmarshallLong(th);
    demon.body = unmarshallLong(th);
    demon.wings = unmarshallLong(th);
}

static void marshallDemon(writer &th, const demon_data &demon)
{
    marshallLong(th, demon.head);
    marshallLong(th, demon.body);
    marshallLong(th, demon.wings);
}

// Internal mcache classes.  The mcache_manager creates these internally.
// The only access external clients need is through the virtual
// info function.

class mcache_monster : public mcache_entry
{
public:
    mcache_monster(const monsters *mon);
    mcache_monster(reader &th);

    virtual unsigned int info(tile_draw_info *dinfo) const;

    static bool valid(const monsters *mon);

    static bool get_weapon_offset(int mon_tile, int &ofs_x, int &ofs_y);

    virtual void construct(writer &th);

protected:
    int m_mon_tile;
    int m_equ_tile;
};

class mcache_draco : public mcache_entry
{
public:
    mcache_draco(const monsters *mon);
    mcache_draco(reader &th);

    virtual unsigned int info(tile_draw_info *dinfo) const;

    static bool valid(const monsters *mon);

    virtual void construct(writer &th);

protected:
    int m_mon_tile;
    int m_job_tile;
    int m_equ_tile;
};

class mcache_ghost : public mcache_entry
{
public:
    mcache_ghost(const monsters *mon);
    mcache_ghost(reader &th);

    virtual const dolls_data *doll() const;

    static bool valid(const monsters *mon);

    virtual void construct(writer &th);

protected:
    dolls_data m_doll;
};

class mcache_demon : public mcache_entry
{
public:
    mcache_demon(const monsters *mon);
    mcache_demon(reader &th);

    virtual unsigned int info(tile_draw_info *dinfo) const;

    static bool valid(const monsters *mon);

    virtual void construct(writer &th);

protected:
    demon_data m_demon;
};

/////////////////////////////////////////////////////////////////////////////
// tile_fg_store

unsigned int tile_fg_store::operator=(unsigned int tile)
{
    if (tile & TILE_FLAG_MASK == m_tile & TILE_FLAG_MASK)
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

unsigned int mcache_manager::register_monster(const monsters *mon)
{
    assert(mon);
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

    unsigned int idx = ~0;

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

mcache_entry *mcache_manager::get(unsigned int tile)
{
    unsigned int idx = tile & TILE_FLAG_MASK;
    if (idx < TILEP_MCACHE_START)
        return NULL;

    if (idx >= TILEP_MCACHE_START + m_entries.size())
        return NULL;

    mcache_entry *entry = m_entries[idx - TILEP_MCACHE_START];
    return (entry);
}

void mcache_manager::read(reader &th)
{
    unsigned int size = unmarshallLong(th);
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
    marshallLong(th, m_entries.size());
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
    m_ref_count = unmarshallLong(th);
}

void mcache_entry::construct(writer &th)
{
    marshallLong(th, m_ref_count);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_monster

mcache_monster::mcache_monster(const monsters *mon)
{
    assert(mcache_monster::valid(mon));

    m_mon_tile = tileidx_monster(mon, false) & TILE_FLAG_MASK;

    int mon_wep = mon->inv[MSLOT_WEAPON];
    m_equ_tile = tilep_equ_weapon(mitm[mon_wep]);
}

// Returns the amount of pixels necessary to shift a wielded weapon
// from its default placement. Tiles showing monsters already wielding
// a weapon should not be listed here.
bool mcache_monster::get_weapon_offset(int mon_tile, int &ofs_x, int &ofs_y)
{
    switch (mon_tile)
    {
    // No shift necessary.
    case TILEP_MONS_VAULT_GUARD:
    case TILEP_MONS_HOBGOBLIN:
    case TILEP_MONS_DEEP_ELF_MASTER_ARCHER:
    case TILEP_MONS_DEEP_ELF_BLADEMASTER:
    case TILEP_MONS_IMP:
    case TILEP_MONS_ANGEL:
    case TILEP_MONS_NORRIS:
    case TILEP_MONS_MAUD:
    case TILEP_MONS_DUANE:
    case TILEP_MONS_EDMUND:
    case TILEP_MONS_FRANCES:
    case TILEP_MONS_HAROLD:
    case TILEP_MONS_JOSEPH:
    case TILEP_MONS_JOZEF:
    case TILEP_MONS_RUPERT:
    case TILEP_MONS_TERENCE:
    case TILEP_MONS_WAYNE:
    case TILEP_MONS_FREDERICK:
    case TILEP_MONS_RAKSHASA:
    case TILEP_MONS_RAKSHASA_FAKE:
    case TILEP_MONS_VAMPIRE_KNIGHT:
    case TILEP_MONS_SKELETAL_WARRIOR:
    case TILEP_MONS_MERMAID:
    case TILEP_MONS_MERMAID_WATER:
    case TILEP_MONS_MERFOLK_FIGHTER:
    case TILEP_MONS_MERFOLK_FIGHTER_WATER:
    case TILEP_MONS_SIREN:
    case TILEP_MONS_SIREN_WATER:
    case TILEP_MONS_ILSUIW:
    case TILEP_MONS_ILSUIW_WATER:
        ofs_x = 0;
        ofs_y = 0;
        break;
    // Shift upwards.
    case TILEP_MONS_GNOLL:
    case TILEP_MONS_DEEP_ELF_DEATH_MAGE:
        ofs_x = -1;
        ofs_y = 0;
        break;
    case TILEP_MONS_TIAMAT:
        ofs_x = -2;
        ofs_y = 0;
        break;
    // Shift downwards.
    case TILEP_MONS_YAKTAUR_MELEE:
        ofs_x = 2;
        ofs_y = 0;
        break;
    case TILEP_MONS_YAKTAUR_CAPTAIN_MELEE:
        ofs_x = 4;
        ofs_y = 0;
        break;
    // Shift to the left.
    case TILEP_MONS_CENTAUR_WARRIOR_MELEE:
    case TILEP_MONS_DEEP_ELF_SORCERER:
    case TILEP_MONS_DEEP_ELF_HIGH_PRIEST:
        ofs_x = 0;
        ofs_y = -1;
        break;
    case TILEP_MONS_MIDGE:
        ofs_x = 0;
        ofs_y = -2;
        break;
    case TILEP_MONS_KOBOLD_DEMONOLOGIST:
        ofs_x = 0;
        ofs_y = -10;
        break;
    // Shift to the right.
    case TILEP_MONS_DEEP_ELF_KNIGHT:
    case TILEP_MONS_NAGA:
    case TILEP_MONS_GREATER_NAGA:
    case TILEP_MONS_NAGA_WARRIOR:
    case TILEP_MONS_GUARDIAN_NAGA:
    case TILEP_MONS_NAGA_MAGE:
        ofs_x = 0;
        ofs_y = 1;
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
        ofs_x = 0;
        ofs_y = 2;
        break;
    case TILEP_MONS_GOBLIN:
    case TILEP_MONS_IJYB:
        ofs_x = 0;
        ofs_y = 4;
        break;
    // Shift upwards and to the left.
    case TILEP_MONS_DEEP_ELF_MAGE:
    case TILEP_MONS_DEEP_ELF_SUMMONER:
    case TILEP_MONS_DEEP_ELF_CONJURER:
    case TILEP_MONS_DEEP_ELF_PRIEST:
    case TILEP_MONS_DEEP_ELF_DEMONOLOGIST:
    case TILEP_MONS_DEEP_ELF_ANNIHILATOR:
        ofs_x = -1;
        ofs_y = -2;
        break;
    case TILEP_MONS_CENTAUR_MELEE:
        ofs_x = -1;
        ofs_y = -3;
        break;
    case TILEP_MONS_SONJA:
        ofs_x = -2;
        ofs_y = -7;
        break;
    case TILEP_MONS_OGRE_MAGE:
        ofs_x = -4;
        ofs_y = -2;
        break;
    // Shift upwards and to the right.
    case TILEP_MONS_HELL_KNIGHT:
        ofs_x = -1;
        ofs_y = 3;
        break;
    // Shift downwards and to the left.
    case TILEP_MONS_WIZARD:
        ofs_x = 2;
        ofs_y = -2;
        break;
    case TILEP_MONS_RED_DEVIL:
        ofs_x = 2;
        ofs_y = -3;
        break;
    // Shift downwards and to the right.
    case TILEP_MONS_BIG_KOBOLD:
        ofs_x = 2;
        ofs_y = 3;
        break;
    case TILEP_MONS_KOBOLD:
        ofs_x = 3;
        ofs_y = 4;
        break;
    case TILEP_MONS_ELF:
    case TILEP_MONS_ZOMBIE_LARGE:
        ofs_x = 4;
        ofs_y = 1;
        break;
    case TILEP_MONS_ZOMBIE_SMALL:
        ofs_x = 4;
        ofs_y = 3;
        break;
    case TILEP_MONS_HUMAN:
        ofs_x = 5;
        ofs_y = 2;
        break;
    default:
        // This monster cannot be displayed with a weapon.
        return (false);
    }

    return (true);
}

unsigned int mcache_monster::info(tile_draw_info *dinfo) const
{
    int ofs_x, ofs_y;
    get_weapon_offset(m_mon_tile, ofs_x, ofs_y);

    dinfo[0].set(m_mon_tile);
    dinfo[1].set(m_equ_tile, ofs_x, ofs_y);

    // In some cases, overlay a second weapon tile...
    if (m_mon_tile == TILEP_MONS_DEEP_ELF_BLADEMASTER)
    {
        int eq2;
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

        dinfo[2].set(eq2, -ofs_x, ofs_y);
        return 3;
    }
    else
        return 2;
}

bool mcache_monster::valid(const monsters *mon)
{
    if (!mon)
        return (false);
    int mon_wep = mon->inv[MSLOT_WEAPON];
    if (mon_wep == NON_ITEM)
        return (false);

    int mon_tile = tileidx_monster(mon, false) & TILE_FLAG_MASK;

    int ox, oy;
    return get_weapon_offset(mon_tile, ox, oy);
}

mcache_monster::mcache_monster(reader &th) : mcache_entry(th)
{
    m_mon_tile = unmarshallLong(th);
    m_equ_tile = unmarshallLong(th);
}

void mcache_monster::construct(writer &th)
{
    mcache_entry::construct(th);

    marshallLong(th, m_mon_tile);
    marshallLong(th, m_equ_tile);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_draco

mcache_draco::mcache_draco(const monsters *mon)
{
    assert(mcache_draco::valid(mon));

    int draco = draco_subspecies(mon);
    int colour;
    switch (draco)
    {
    default:
    case MONS_DRACONIAN:        colour = 0; break;
    case MONS_BLACK_DRACONIAN:  colour = 1; break;
    case MONS_YELLOW_DRACONIAN: colour = 2; break;
    case MONS_GREEN_DRACONIAN:  colour = 3; break;
    case MONS_MOTTLED_DRACONIAN:colour = 4; break;
    case MONS_PALE_DRACONIAN:   colour = 5; break;
    case MONS_PURPLE_DRACONIAN: colour = 6; break;
    case MONS_RED_DRACONIAN:    colour = 7; break;
    case MONS_WHITE_DRACONIAN:  colour = 8; break;
    }

    m_mon_tile = TILEP_DRACO_BASE + colour;
    int mon_wep = mon->inv[MSLOT_WEAPON];
    m_equ_tile = (mon_wep != NON_ITEM) ? tilep_equ_weapon(mitm[mon_wep]) : 0;

    switch (mon->type)
    {
        case MONS_DRACONIAN_CALLER:
            m_job_tile = TILEP_DRACO_CALLER;
            break;
        case MONS_DRACONIAN_MONK:
            m_job_tile = TILEP_DRACO_MONK;
            break;
        case MONS_DRACONIAN_ZEALOT:
            m_job_tile = TILEP_DRACO_ZEALOT;
            break;
        case MONS_DRACONIAN_SHIFTER:
            m_job_tile = TILEP_DRACO_SHIFTER;
            break;
        case MONS_DRACONIAN_ANNIHILATOR:
            m_job_tile = TILEP_DRACO_ANNIHILATOR;
            break;
        case MONS_DRACONIAN_KNIGHT:
            m_job_tile = TILEP_DRACO_KNIGHT;
            break;
        case MONS_DRACONIAN_SCORCHER:
            m_job_tile = TILEP_DRACO_SCORCHER;
            break;
        default:
            m_job_tile = 0;
            break;
    }
}

unsigned int mcache_draco::info(tile_draw_info *dinfo) const
{
    unsigned int i = 0;

    dinfo[i++].set(m_mon_tile);
    if (m_job_tile)
        dinfo[i++].set(m_job_tile);
    if (m_equ_tile)
        dinfo[i++].set(m_equ_tile, -2, 0);

    return i;
}

bool mcache_draco::valid(const monsters *mon)
{
    return (mon && mon->type >= MONS_FIRST_DRACONIAN
            && mon->type <= MONS_LAST_DRACONIAN);
}

mcache_draco::mcache_draco(reader &th) : mcache_entry(th)
{
    m_mon_tile = unmarshallLong(th);
    m_job_tile = unmarshallLong(th);
    m_equ_tile = unmarshallLong(th);
}

void mcache_draco::construct(writer &th)
{
    mcache_entry::construct(th);

    marshallLong(th, m_mon_tile);
    marshallLong(th, m_job_tile);
    marshallLong(th, m_equ_tile);
}

/////////////////////////////////////////////////////////////////////////////
// mcache_ghost

mcache_ghost::mcache_ghost(const monsters *mon)
{
    assert(mcache_ghost::valid(mon));

    const struct ghost_demon &ghost = *mon->ghost;

    unsigned int pseudo_rand = ghost.max_hp * 54321 * 54321;
    int gender = (pseudo_rand >> 8) & 1;

    tilep_race_default(ghost.species, gender,
                       ghost.xl, m_doll.parts);
    tilep_job_default(ghost.job, gender, m_doll.parts);

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
    else if (ac > 5 )
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
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_GREAT_FRAIL;
        else if (dam > 25)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_GREAT_MACE;
        else if (dam > 20)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SPIKED_FRAIL;
        else if (dam > 15)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_MORNINGSTAR;
        else if (dam > 10)
            m_doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_FRAIL;
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

bool mcache_ghost::valid(const monsters *mon)
{
    return (mon && mon->type == MONS_PLAYER_GHOST);
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

/////////////////////////////////////////////////////////////////////////////
// mcache_demon

mcache_demon::mcache_demon(const monsters *mon)
{
    assert(mcache_demon::valid(mon));

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

unsigned int mcache_demon::info(tile_draw_info *dinfo) const
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

bool mcache_demon::valid(const monsters *mon)
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
