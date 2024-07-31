#include "AppHdr.h"
#ifdef USE_TILE_LOCAL
#include "tiledgnbuf.h"

#include "mpr.h"
#include "tile-flags.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-main.h"
#include "rltiles/tiledef-player.h"
#include "tag-version.h"
#include "tiledoll.h"
#include "tilefont.h"
#include "tilemcache.h"
#include "tilepick.h"
#include "options.h"

DungeonCellBuffer::DungeonCellBuffer(const ImageManager *im) :
    m_buf_floor(&im->get_texture(TEX_FLOOR)),
    m_buf_wall(&im->get_texture(TEX_WALL)),
    m_buf_feat(&im->get_texture(TEX_FEAT)),
    m_buf_feat_trans(&im->get_texture(TEX_FEAT), 17),
    m_buf_doll(&im->get_texture(TEX_PLAYER), 17),
    m_buf_main_trans(&im->get_texture(TEX_DEFAULT), 17),
    m_buf_main(&im->get_texture(TEX_DEFAULT)),
    m_buf_spells(&im->get_texture(TEX_GUI)),
    m_buf_skills(&im->get_texture(TEX_GUI)),
    m_buf_commands(&im->get_texture(TEX_GUI)),
    m_buf_icons(&im->get_texture(TEX_ICONS)),
    m_buf_glyphs(im->get_glyph_font())
{
}

static bool _in_water(const packed_cell &cell)
{
    return (cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING);
}

void DungeonCellBuffer::add_glyph(const char32_t &g, const VColour &col, int x, int y)
{
    float sx = x;
    float sy = y;
    m_buf_glyphs.get_font_wrapper().store(m_buf_glyphs, sx, sy, g, col);
}

void DungeonCellBuffer::add_glyph(const char32_t &g, const VColour &col, const VColour &bg, int x, int y)
{
    float sx = x;
    float sy = y;
    m_buf_glyphs.get_font_wrapper().store(m_buf_glyphs, sx, sy, g, col, bg);
}

void DungeonCellBuffer::add(const packed_cell &cell, int x, int y)
{
    pack_background(x, y, cell);

    const tileidx_t fg_idx = cell.fg & TILE_FLAG_MASK;
    const bool in_water = _in_water(cell);

    tileidx_t cloud_idx = cell.cloud & TILE_FLAG_MASK;

    // in the shoals, ink is handled in pack_cell_overlays(): don't overdraw
    if (cloud_idx == TILE_CLOUD_INK && player_in_branch(BRANCH_SHOALS))
        cloud_idx = 0;

    if (fg_idx >= TILEP_MCACHE_START)
    {
        mcache_entry *entry = mcache.get(fg_idx);
        if (entry)
            pack_mcache(entry, x, y, in_water);
        else
            m_buf_doll.add(TILEP_MONS_UNKNOWN, x, y, 0, in_water, false);
    }
    else if (fg_idx == TILEP_PLAYER)
        pack_player(x, y, in_water);
    else if (get_tile_texture(fg_idx) == TEX_PLAYER)
        m_buf_doll.add(fg_idx, x, y, TILEP_PART_MAX, in_water, false);

    pack_foreground(x, y, cell);

    // Draw cloud layer(s)
    if (cloud_idx)
    {
        ASSERT(get_tile_texture(cloud_idx) == TEX_DEFAULT);

        // If there's a foreground, sandwich it between two semi-transparent
        // clouds at different z-indices. This uses the same alpha fading as
        // a swimming character but applied to the cloud (instead of as normal
        // applied to the character).
        if (fg_idx)
        {
            m_buf_main_trans.add_masked(cloud_idx, x, y, 0, 0, 0, -1, 255, 255, 20);
            m_buf_main_trans.add_masked(cloud_idx, x, y, 50, 0, 0, -1, 15, 255,20);
        }
        else
            // Otherwise render it normally with full transparency
             m_buf_main.add(cloud_idx, x, y);
    }

    // Render any 'main' overlays (zaps) on top of clouds and items.
    for (int i = 0; i < cell.num_dngn_overlay; ++i)
    {
        const auto tile = cell.dngn_overlay[i];
        if (TILE_DNGN_MAX <= tile && tile < TILE_MAIN_MAX)
            add_main_tile(tile, x, y);
    }
}

void DungeonCellBuffer::add_monster(const monster_info &mon, int x, int y)
{
    tileidx_t t    = tileidx_monster(mon);
    tileidx_t t0   = t & TILE_FLAG_MASK;
    tileidx_t flag = t & (~TILE_FLAG_MASK);

    // Copied from _tile_place_monster()
    if (!mons_class_is_stationary(mon.type) || mon.type == MONS_TRAINING_DUMMY)
    {
        tileidx_t mcache_idx = mcache.register_monster(mon);
        t = flag | (mcache_idx ? mcache_idx : t0);
        t0 = t & TILE_FLAG_MASK;
    }

    // Copied from ::add()
    if (t0 >= TILEP_MCACHE_START)
    {
        mcache_entry *entry = mcache.get(t0);
        if (entry)
            pack_mcache(entry, x, y, false);
        else
            m_buf_doll.add(TILEP_MONS_UNKNOWN, x, y, 0, false, false);
    }
    else if (get_tile_texture(t0) == TEX_PLAYER)
        m_buf_doll.add(t0, x, y, TILEP_PART_MAX, false, false);
    else if (get_tile_texture(t0) == TEX_DEFAULT)
    {
        const tileidx_t base_idx = tileidx_known_base_item(t0);
        if (base_idx)
            m_buf_main.add(base_idx, x, y);
        m_buf_main.add(t0, x, y);
    }

    // hijack pack_foreground() to draw status icons
    packed_cell fake_cell;
    fake_cell.fg = flag;
    fake_cell.bg = 0;
    fake_cell.icons = status_icons_for(mon);
    pack_foreground(x, y, fake_cell);
}

void DungeonCellBuffer::add_dngn_tile(int tileidx, int x, int y,
                                      bool in_water)
{
    ASSERT(tileidx < TILE_FEAT_MAX);

    if (tileidx < TILE_FLOOR_MAX)
        m_buf_floor.add(tileidx, x, y);
    else if (tileidx < TILE_WALL_MAX)
        m_buf_wall.add(tileidx, x, y);
    else if (in_water)
    {
        m_buf_floor.add(TILE_DNGN_SHALLOW_WATER, x, y);
        m_buf_feat_trans.add(tileidx, x, y, 0, true, false);
    }
    else
        m_buf_feat.add(tileidx, x, y);
}

void DungeonCellBuffer::add_main_tile(int tileidx, int x, int y)
{
    const tileidx_t base = tileidx_known_base_item(tileidx);
    if (base)
        m_buf_main.add(base, x, y);

    m_buf_main.add(tileidx, x, y);
}

void DungeonCellBuffer::add_special_tile(int tileidx, int x, int y, int ox, int oy)
{
    m_buf_main.add(tileidx, x, y, ox, oy, false);
}

void DungeonCellBuffer::add_spell_tile(int tileidx, int x, int y)
{
    m_buf_spells.add(tileidx, x, y);
}

void DungeonCellBuffer::add_skill_tile(int tileidx, int x, int y)
{
    m_buf_skills.add(tileidx, x, y);
}

void DungeonCellBuffer::add_command_tile(int tileidx, int x, int y)
{
    m_buf_commands.add(tileidx, x, y);
}

void DungeonCellBuffer::add_icons_tile(int tileidx, int x, int y)
{
    m_buf_icons.add(tileidx, x, y);
}

void DungeonCellBuffer::add_icons_tile(int tileidx, int x, int y,
                                       int ox, int oy)
{
    m_buf_icons.add(tileidx, x, y, ox, oy, false);
}

void DungeonCellBuffer::clear()
{
    m_buf_floor.clear();
    m_buf_wall.clear();
    m_buf_feat.clear();
    m_buf_feat_trans.clear();
    m_buf_doll.clear();
    m_buf_main_trans.clear();
    m_buf_main.clear();
    m_buf_glyphs.clear();
    m_buf_spells.clear();
    m_buf_skills.clear();
    m_buf_commands.clear();
    m_buf_icons.clear();
}

void DungeonCellBuffer::draw()
{
    // normal tiles draw cycle: no glyphs
    draw_tiles();
    draw_icons();
}

void DungeonCellBuffer::draw_glyphs()
{
    // this can't happen under the same transform as regular tiles, so it
    // needs to be called separately. (Refactor somehow?)

    // TODO: used only in DungeonRegion. Implement:
    // * grid controls (items, monsters)
    // * popups
    // * menu icons
    m_buf_glyphs.draw();
}

void DungeonCellBuffer::draw_tiles()
{
    // things that alternate with glyphs
    m_buf_floor.draw();
    m_buf_wall.draw();
    m_buf_feat.draw();
    m_buf_feat_trans.draw();
    m_buf_doll.draw();
    m_buf_main_trans.draw();
    m_buf_main.draw();
}

void DungeonCellBuffer::draw_icons()
{
    // things that happen after / instead of glyphs
    m_buf_skills.draw();
    m_buf_spells.draw();
    m_buf_commands.draw();
    m_buf_icons.draw();
}

void DungeonCellBuffer::add_blood_overlay(int x, int y, const packed_cell &cell,
                                          bool is_wall)
{
    if (cell.is_liquefied && !is_wall)
    {
        int offset = cell.flv.special % tile_dngn_count(TILE_LIQUEFACTION);
        m_buf_floor.add(TILE_LIQUEFACTION + offset, x, y);
    }
    else if (cell.is_bloody && Options.show_blood)
    {
        tileidx_t basetile;
        if (is_wall)
        {
            basetile = cell.old_blood ? TILE_WALL_OLD_BLOOD : TILE_WALL_BLOOD_S;
            basetile += tile_dngn_count(basetile) * cell.blood_rotation;
        }
        else
            basetile = TILE_BLOOD;
        const int offset = cell.flv.special % tile_dngn_count(basetile);
        m_buf_feat.add(basetile + offset, x, y);
    }
}

void DungeonCellBuffer::pack_background(int x, int y, const packed_cell &cell)
{
    const tileidx_t bg = cell.bg;
    const tileidx_t bg_idx = cell.bg & TILE_FLAG_MASK;

    if (bg_idx >= TILE_DNGN_FIRST_TRANSPARENT)
        add_dngn_tile(cell.flv.floor, x, y);

    // Draw blood beneath feature tiles.
    if (bg_idx > TILE_WALL_MAX)
        add_blood_overlay(x, y, cell);

    add_dngn_tile(bg_idx, x, y, cell.mangrove_water);

    if (bg_idx > TILE_DNGN_UNSEEN)
    {
        // Draw blood on top of wall tiles.
        if (bg_idx <= TILE_WALL_MAX)
            add_blood_overlay(x, y, cell, bg_idx >= TILE_FLOOR_MAX);

        for (int i = 0; i < cell.num_dngn_overlay; ++i)
        {
            const auto tile = cell.dngn_overlay[i];
            if (tile < TILE_DNGN_MAX)
                add_dngn_tile(tile, x, y);
        }

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            // Add tentacle corner overlays.
            if (bg & TILE_FLAG_TENTACLE_NW)
            {
                if (bg & TILE_FLAG_TENTACLE_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_OVERLAY_NW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_ELDRITCH)
                    m_buf_feat.add(TILE_ELDRITCH_OVERLAY_NW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_STARSPAWN)
                    m_buf_feat.add(TILE_STARSPAWN_OVERLAY_NW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_VINE)
                    m_buf_feat.add(TILE_VINE_OVERLAY_NW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_ZOMBIE_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_ZOMBIE_OVERLAY_NW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_SIMULACRUM_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_SIMULACRUM_OVERLAY_NW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_SPECTRAL_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_SPECTRAL_OVERLAY_NW, x, y);
            }
            else if (bg & TILE_FLAG_TENTACLE_NE)
            {
                if (bg & TILE_FLAG_TENTACLE_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_OVERLAY_NE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_ELDRITCH)
                    m_buf_feat.add(TILE_ELDRITCH_OVERLAY_NE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_STARSPAWN)
                    m_buf_feat.add(TILE_STARSPAWN_OVERLAY_NE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_VINE)
                    m_buf_feat.add(TILE_VINE_OVERLAY_NE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_ZOMBIE_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_ZOMBIE_OVERLAY_NE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_SIMULACRUM_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_SIMULACRUM_OVERLAY_NE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_SPECTRAL_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_SPECTRAL_OVERLAY_NE, x, y);
            }
            else if (bg & TILE_FLAG_TENTACLE_SW)
            {
                if (bg & TILE_FLAG_TENTACLE_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_OVERLAY_SW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_ELDRITCH)
                    m_buf_feat.add(TILE_ELDRITCH_OVERLAY_SW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_STARSPAWN)
                    m_buf_feat.add(TILE_STARSPAWN_OVERLAY_SW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_VINE)
                    m_buf_feat.add(TILE_VINE_OVERLAY_SW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_ZOMBIE_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_ZOMBIE_OVERLAY_SW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_SIMULACRUM_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_SIMULACRUM_OVERLAY_SW, x, y);
                else if (bg & TILE_FLAG_TENTACLE_SPECTRAL_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_SPECTRAL_OVERLAY_SW, x, y);
            }
            else if (bg & TILE_FLAG_TENTACLE_SE)
            {
                if (bg & TILE_FLAG_TENTACLE_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_OVERLAY_SE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_ELDRITCH)
                    m_buf_feat.add(TILE_ELDRITCH_OVERLAY_SE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_STARSPAWN)
                    m_buf_feat.add(TILE_STARSPAWN_OVERLAY_SE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_VINE)
                    m_buf_feat.add(TILE_VINE_OVERLAY_SE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_ZOMBIE_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_ZOMBIE_OVERLAY_SE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_SIMULACRUM_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_SIMULACRUM_OVERLAY_SE, x, y);
                else if (bg & TILE_FLAG_TENTACLE_SPECTRAL_KRAKEN)
                    m_buf_feat.add(TILE_KRAKEN_SPECTRAL_OVERLAY_SE, x, y);
            }
        }

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            if (cell.is_sanctuary)
                m_buf_feat.add(TILE_SANCTUARY, x, y);
            if (cell.is_blasphemy)
                m_buf_feat.add(TILE_BLASPHEMY, x, y);
            if (cell.is_silenced)
                m_buf_feat.add(TILE_SILENCED, x, y);
            if (cell.halo == HALO_RANGE)
                m_buf_feat.add(TILE_HALO_RANGE, x, y);
            if (cell.halo == HALO_UMBRA)
                m_buf_feat.add(TILE_UMBRA + random2(4), x, y);

            if (cell.orb_glow)
                m_buf_feat.add(TILE_ORB_GLOW + cell.orb_glow - 1, x, y);
            if (cell.quad_glow)
                m_buf_feat.add(TILE_QUAD_GLOW, x, y);
            if (cell.disjunct)
                m_buf_feat.add(TILE_DISJUNCT + cell.disjunct - 1, x, y);
            if (cell.awakened_forest)
                m_buf_icons.add(TILEI_BERSERK, x, y);
            if (cell.has_bfb_corpse)
                m_buf_feat.add(TILE_BLOOD_FOR_BLOOD, x, y);

            if (cell.fg)
            {
                const tileidx_t att_flag = cell.fg & TILE_FLAG_ATT_MASK;
                if (att_flag == TILE_FLAG_PET)
                    m_buf_feat.add(TILE_HALO_FRIENDLY, x, y);
                else if (att_flag == TILE_FLAG_GD_NEUTRAL)
                    m_buf_feat.add(TILE_HALO_GD_NEUTRAL, x, y);
                else if (att_flag == TILE_FLAG_NEUTRAL)
                    m_buf_feat.add(TILE_HALO_NEUTRAL, x, y);

                const tileidx_t threat_flag = cell.fg & TILE_FLAG_THREAT_MASK;
                if (threat_flag == TILE_FLAG_TRIVIAL)
                    m_buf_feat.add(TILE_THREAT_TRIVIAL, x, y);
                else if (threat_flag == TILE_FLAG_EASY)
                    m_buf_feat.add(TILE_THREAT_EASY, x, y);
                else if (threat_flag == TILE_FLAG_TOUGH)
                    m_buf_feat.add(TILE_THREAT_TOUGH, x, y);
                else if (threat_flag == TILE_FLAG_NASTY)
                    m_buf_feat.add(TILE_THREAT_NASTY, x, y);
                else if (threat_flag == TILE_FLAG_UNUSUAL)
                    m_buf_feat.add(TILE_THREAT_UNUSUAL, x, y);

                if (cell.is_highlighted_summoner)
                    m_buf_feat.add(TILE_HALO_SUMMONER, x, y);
            }

            // Apply the travel exclusion under the foreground if the cell is
            // visible. It will be applied later if the cell is unseen.
            if (bg & TILE_FLAG_EXCL_CTR)
                m_buf_feat.add(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
            else if (bg & TILE_FLAG_TRAV_EXCL)
                m_buf_feat.add(TILE_TRAVEL_EXCLUSION_BG, x, y);
        }

    }

    // allow rays even on completely unseen squares (e.g. passwall)
    if (bg & TILE_FLAG_RAY)
        m_buf_feat.add(TILE_RAY, x, y);
    else if (bg & TILE_FLAG_RAY_OOR)
        m_buf_feat.add(TILE_RAY_OUT_OF_RANGE, x, y);
    else if (bg & TILE_FLAG_LANDING)
        m_buf_feat.add(TILE_LANDING, x, y);
    else if (bg & TILE_FLAG_RAY_MULTI)
        m_buf_feat.add(TILE_RAY_MULTI, x, y);
}

static const int FIXED_LOC_ICON = -1;

static map<tileidx_t, int> status_icon_sizes = {
    { TILEI_STICKY_FLAME,   7 },
    { TILEI_INNER_FLAME,    7 },
    { TILEI_CONSTRICTED,    11 },
    { TILEI_HASTED,         6 },
    { TILEI_SLOWED,         6 },
    { TILEI_MIGHT,          6 },
    { TILEI_DRAIN,          6 },
    { TILEI_PAIN_MIRROR,    7 },
    { TILEI_PETRIFYING,     6 },
    { TILEI_PETRIFIED,      6 },
    { TILEI_BLIND,          10 },
    { TILEI_BOUND_SOUL,     6 },
    { TILEI_POSSESSABLE,    6 },
    { TILEI_INFESTED,       6 },
    { TILEI_CORRODED,       6 },
    { TILEI_SWIFT,          6 },
    { TILEI_RECALL,         6 },
    { TILEI_VILE_CLUTCH,    11 },
    { TILEI_SLOWLY_DYING,   10 },
    { TILEI_FIRE_CHAMP,     7 },
    { TILEI_ANGUISH,        8 },
    { TILEI_WEAKENED,       6 },
    { TILEI_WATERLOGGED,    10 },
    { TILEI_STILL_WINDS,    10 },
    { TILEI_GHOSTLY,        8 },
    { TILEI_ANTIMAGIC,      10 },
    { TILEI_DAZED,          6 },
    { TILEI_PARTIALLY_CHARGED, 6 },
    { TILEI_FULLY_CHARGED,  6 },
    { TILEI_FIRE_VULN,      8 },
    { TILEI_CONC_VENOM,     7 },
    { TILEI_REPEL_MISSILES, 10 },
    { TILEI_INJURY_BOND,    10 },
    { TILEI_TELEPORTING,    9 },
    { TILEI_RESISTANCE,     8 },
    { TILEI_BRILLIANCE,     10 },
    { TILEI_MALMUTATED,     8 },
    { TILEI_GLOW_LIGHT,     10 },
    { TILEI_GLOW_HEAVY,     10 },
    { TILEI_PAIN_BOND,      11 },
    { TILEI_BULLSEYE,       10 },
    { TILEI_VITRIFIED,      6 },
    { TILEI_CURSE_OF_AGONY, 10 },
    { TILEI_REGENERATION,   8 },

    // These are in the bottom right, so don't need to shift.
    { TILEI_BERSERK,        FIXED_LOC_ICON },
    { TILEI_IDEALISED,      FIXED_LOC_ICON },
    { TILEI_TOUCH_OF_BEOGH, FIXED_LOC_ICON },

    // These are always in the top left. They may overlap.
    // (E.g. for summoned dancing weapons or animated armour.)
    { TILEI_ANIMATED_WEAPON, FIXED_LOC_ICON },
    { TILEI_SUMMONED,        FIXED_LOC_ICON },
    { TILEI_PERM_SUMMON,     FIXED_LOC_ICON },
    { TILEI_VENGEANCE_TARGET,FIXED_LOC_ICON },

    // Along the bottom of the monster.
    { TILEI_SHADOWLESS,      FIXED_LOC_ICON },
};

void DungeonCellBuffer::pack_foreground(int x, int y, const packed_cell &cell)
{
    const tileidx_t fg = cell.fg;
    const tileidx_t bg = cell.bg;
    const tileidx_t fg_idx = cell.fg & TILE_FLAG_MASK;
    const bool in_water = _in_water(cell);

    if (get_tile_texture(fg_idx) == TEX_DEFAULT)
    {
        const tileidx_t base_idx = tileidx_known_base_item(fg_idx);

        if (in_water)
        {
            if (base_idx)
                m_buf_main_trans.add(base_idx, x, y, 0, true, false);
            m_buf_main_trans.add(fg_idx, x, y, 0, true, false);
        }
        else
        {
            if (base_idx)
                m_buf_main.add(base_idx, x, y);

            m_buf_main.add(fg_idx, x, y);
        }
    }

    if (fg & TILE_FLAG_NET)
        m_buf_icons.add(TILEI_TRAP_NET, x, y);

    if (fg & TILE_FLAG_WEB)
        m_buf_icons.add(TILEI_TRAP_WEB, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        m_buf_icons.add(TILEI_SOMETHING_UNDER, x, y);

    // Pet mark
    if (fg & TILE_FLAG_ATT_MASK)
    {
        const tileidx_t att_flag = fg & TILE_FLAG_ATT_MASK;
        if (att_flag == TILE_FLAG_PET)
            m_buf_icons.add(TILEI_FRIENDLY, x, y);
        else if (att_flag == TILE_FLAG_GD_NEUTRAL)
            m_buf_icons.add(TILEI_GOOD_NEUTRAL, x, y);
        else if (att_flag == TILE_FLAG_NEUTRAL)
            m_buf_icons.add(TILEI_NEUTRAL, x, y);
    }

    int status_shift = 0;
    if (fg & TILE_FLAG_BEH_MASK)
    {
        const tileidx_t beh_flag = fg & TILE_FLAG_BEH_MASK;
        if (beh_flag == TILE_FLAG_PARALYSED)
        {
            m_buf_icons.add(TILEI_PARALYSED, x, y);
            status_shift += 12;
        }
        else if (beh_flag == TILE_FLAG_STAB)
        {
            m_buf_icons.add(TILEI_STAB_BRAND, x, y);
            status_shift += 12;
        }
        else if (beh_flag == TILE_FLAG_MAY_STAB)
        {
            m_buf_icons.add(TILEI_MAY_STAB_BRAND, x, y);
            status_shift += 7;
        }
        else if (beh_flag == TILE_FLAG_FLEEING)
        {
            m_buf_icons.add(TILEI_FLEEING, x, y);
            status_shift += 3;
        }
    }

    if (fg & TILE_FLAG_POISON_MASK)
    {
        const tileidx_t poison_flag = fg & TILE_FLAG_POISON_MASK;
        if (poison_flag == TILE_FLAG_POISON)
        {
            m_buf_icons.add(TILEI_POISON, x, y, -status_shift, 0);
            status_shift += 5;
        }
        else if (poison_flag == TILE_FLAG_MORE_POISON)
        {
            m_buf_icons.add(TILEI_MORE_POISON, x, y, -status_shift, 0);
            status_shift += 5;
        }
        else if (poison_flag == TILE_FLAG_MAX_POISON)
        {
            m_buf_icons.add(TILEI_MAX_POISON, x, y, -status_shift, 0);
            status_shift += 5;
        }
    }

    // We might want to enforce some explicit ordering on these.
    // Currently, they default to iteration order.
    for (auto icon : cell.icons)
    {
        int size = status_icon_sizes[icon];
        if (size == FIXED_LOC_ICON)
        {
            m_buf_icons.add(icon, x, y);
            continue;
        }

        m_buf_icons.add(icon, x, y, -status_shift, 0);
        if (!size)
        {
            dprf("unknown icon %" PRIu64, icon);
            size = 7; // could maybe crash here?
        }
        status_shift += size;
        // Very large numbers of these can go off the edge of
        // the tile. Oh well! (Could skip those..?)
    }

    if (bg & TILE_FLAG_UNSEEN && (bg != TILE_FLAG_UNSEEN || fg))
        m_buf_icons.add(TILEI_MESH, x, y);

    if (bg & TILE_FLAG_OOR && (bg != TILE_FLAG_OOR || fg))
        m_buf_icons.add(TILEI_OOR_MESH, x, y);

    if (bg & TILE_FLAG_MM_UNSEEN && (bg != TILE_FLAG_MM_UNSEEN || fg))
        m_buf_icons.add(TILEI_MAGIC_MAP_MESH, x, y);

    if (bg & TILE_FLAG_RAMPAGE)
        m_buf_icons.add(TILEI_RAMPAGE, x, y);

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
        m_buf_icons.add(TILEI_NEW_STAIR, x, y);

    if (bg & TILE_FLAG_NEW_TRANSPORTER && status_shift == 0)
        m_buf_icons.add(TILEI_NEW_TRANSPORTER, x, y);

    if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
        m_buf_icons.add(TILEI_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
        m_buf_icons.add(TILEI_TRAVEL_EXCLUSION_FG, x, y);

    // Tutorial cursor takes precedence over other cursors.
    if (bg & TILE_FLAG_TUT_CURSOR)
        m_buf_icons.add(TILEI_TUTORIAL_CURSOR, x, y);
    else if (bg & TILE_FLAG_CURSOR)
    {
        int type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILEI_CURSOR : TILEI_CURSOR2;

        if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
           type = TILEI_CURSOR3;

        m_buf_icons.add(type, x, y);
    }

    if (cell.travel_trail & 0xF)
    {
        m_buf_icons.add(TILEI_TRAVEL_PATH_FROM +
                        (cell.travel_trail & 0xF) - 1, x, y);
    }
    if (cell.travel_trail & 0xF0)
    {
        m_buf_icons.add(TILEI_TRAVEL_PATH_TO +
                        ((cell.travel_trail & 0xF0) >> 4) - 1, x, y);
    }

    if (fg & TILE_FLAG_MDAM_MASK)
    {
        tileidx_t mdam_flag = fg & TILE_FLAG_MDAM_MASK;
        if (mdam_flag == TILE_FLAG_MDAM_LIGHT)
            m_buf_icons.add(TILEI_MDAM_LIGHTLY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_MOD)
            m_buf_icons.add(TILEI_MDAM_MODERATELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_HEAVY)
            m_buf_icons.add(TILEI_MDAM_HEAVILY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_SEV)
            m_buf_icons.add(TILEI_MDAM_SEVERELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_ADEAD)
            m_buf_icons.add(TILEI_MDAM_ALMOST_DEAD, x, y);
    }

    if (fg & TILE_FLAG_DEMON)
    {
        tileidx_t demon_flag = fg & TILE_FLAG_DEMON;
        if (demon_flag == TILE_FLAG_DEMON_1)
            m_buf_icons.add(TILEI_DEMON_NUM1, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_2)
            m_buf_icons.add(TILEI_DEMON_NUM2, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_3)
            m_buf_icons.add(TILEI_DEMON_NUM3, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_4)
            m_buf_icons.add(TILEI_DEMON_NUM4, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_5)
            m_buf_icons.add(TILEI_DEMON_NUM5, x, y);
    }
}

void DungeonCellBuffer::pack_player(int x, int y, bool submerged)
{
    dolls_data result = player_doll;
    fill_doll_equipment(result);
    pack_doll(result, x, y, submerged, false);
}

void DungeonCellBuffer::pack_doll(const dolls_data &doll, int x, int y,
                                  bool submerged, bool ghost)
{
    pack_doll_buf(m_buf_doll, doll, x, y, submerged, ghost);
}

void DungeonCellBuffer::pack_mcache(mcache_entry *entry, int x, int y,
                                    bool submerged)
{
    ASSERT(entry);

    bool trans = entry->transparent();

    const dolls_data *doll = entry->doll();
    if (doll)
        pack_doll(*doll, x, y, submerged, trans);

    tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
    int draw_info_count = entry->info(&dinfo[0]);
    for (int i = 0; i < draw_info_count; i++)
    {
        m_buf_doll.add(dinfo[i].idx, x, y, TILEP_PART_MAX, submerged, trans,
                       dinfo[i].ofs_x, dinfo[i].ofs_y);
    }
}

FontWrapper *DungeonCellBuffer::get_glyph_font()
{
    return &m_buf_glyphs.get_font_wrapper();
}

#endif //TILEDGNBUF.CC
