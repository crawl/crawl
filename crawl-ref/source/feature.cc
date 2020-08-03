#include "AppHdr.h"

#include "feature.h"

#include "colour.h"
#include "libutil.h"
#include "options.h"
#include "player.h"
#include "viewchar.h"

#include "feature-data.h"

static FixedVector<feature_def, NUM_SHOW_ITEMS> item_defs;
static int feat_index[NUM_FEATURES];
static feature_def invis_fd, cloud_fd;

/** What symbol should be used for this feature?
 *
 *  @returns The symbol from the 'feature' option if given, otherwise the
 *           character corresponding to ::dchar.
 */
char32_t feature_def::symbol() const
{
    auto over = map_find(Options.feature_symbol_overrides, feat);
    if (over && (*over)[0])
        return get_glyph_override((*over)[0]);

    return dchar_glyph(dchar);
}

/** What symbol should be used for this feature when magic mapped?
 *
 *  @returns The symbol from the 'feature' option if given, otherwise the
 *           character corresponding to ::magic_dchar if set, otherwise
 *           the normal symbol.
 */
char32_t feature_def::magic_symbol() const
{
    auto over = map_find(Options.feature_symbol_overrides, feat);
    if (over && (*over)[1])
        return get_glyph_override((*over)[1]);

    if (magic_dchar != NUM_DCHAR_TYPES)
        return dchar_glyph(magic_dchar);
    else
        return symbol();
}

/** What colour should be used for this feature?
 *
 *  @returns The colour from the feature option if given, otherwise
 *           the normal colour.
 */
colour_t feature_def::colour() const
{
    auto ofeat = map_find(Options.feature_colour_overrides, feat);
    if (ofeat && ofeat->dcolour)
        return ofeat->dcolour;

    return dcolour;
}

/** What colour should be used for this feature when out of LOS?
 *
 *  @returns The unseen_colour from the feature option if given, otherwise
 *           the normal unseen_colour.
 */
colour_t feature_def::unseen_colour() const
{
    auto ofeat = map_find(Options.feature_colour_overrides, feat);
    if (ofeat && ofeat->unseen_dcolour)
        return ofeat->unseen_dcolour;

    return unseen_dcolour;
}

/** What colour should be used for this feature when we have knowledge of it?
 *
 *  @returns The seen_colour from the feature option if given, otherwise
 *           the normal seen_colour.
 */
colour_t feature_def::seen_colour() const
{
    auto ofeat = map_find(Options.feature_colour_overrides, feat);
    if (ofeat && ofeat->seen_dcolour)
        return ofeat->seen_dcolour;

    return seen_dcolour;
}

/** Emphasised colour when out of LoS?
 *
 *  @returns The seen_em_colour from the feature option if given, otherwise
 *           the normal seen_em_colour.
 */
colour_t feature_def::seen_em_colour() const
{
    auto ofeat = map_find(Options.feature_colour_overrides, feat);
    if (ofeat && ofeat->seen_em_dcolour)
        return ofeat->seen_em_dcolour;

    return seen_em_dcolour;
}

/** Emphasised colour in LOS?
 *
 *  @returns The em_colour from the feature option if given, otherwise
*            the normal em_colour.
 */
colour_t feature_def::em_colour() const
{
    auto ofeat = map_find(Options.feature_colour_overrides, feat);
    if (ofeat && ofeat->em_dcolour)
        return ofeat->em_dcolour;

    return em_dcolour;
}

/** Do the default colour relations on a feature_def.
 *
 *  @param[out] f The feature_def to be filled out.
 */
static void _create_colours(feature_def &f)
{
    if (f.seen_dcolour == BLACK)
        f.seen_dcolour = f.unseen_dcolour;

    if (f.seen_em_dcolour == BLACK)
        f.seen_em_dcolour = f.seen_dcolour;

    if (f.em_dcolour == BLACK)
        f.em_dcolour = f.dcolour;
}

static void _init_feature_index()
{
    for (int i = 0; i < NUM_FEATURES; ++i)
        feat_index[i] = -1;

    for (int i = 0; i < (int) ARRAYSZ(feat_defs); ++i)
    {
        const dungeon_feature_type feat = feat_defs[i].feat;
        ASSERT_RANGE(feat, 0, NUM_FEATURES);
        ASSERT(feat_index[feat] == -1); // enum repeated in feature-data!
        feat_index[feat] = i;
    }
}

/** Called at startup to perform fixups on feat_defs and fill in item_defs
 *  and {invis,cloud}_fd.
 */
void init_show_table()
{
    _init_feature_index();

    for (int i = 0; i < NUM_SHOW_ITEMS; i++)
    {
        show_item_type si = static_cast<show_item_type>(i);
        // SHOW_ITEM_NONE is bogus, but "invis exposed" is an ok placeholder
        COMPILE_CHECK(DCHAR_ITEM_AMULET - DCHAR_ITEM_DETECTED + 2 == NUM_SHOW_ITEMS);
        item_defs[si].minimap = MF_ITEM;
        item_defs[si].dchar = static_cast<dungeon_char_type>(i
            + DCHAR_ITEM_DETECTED - SHOW_ITEM_DETECTED);
        _create_colours(item_defs[si]);
    }

    invis_fd.dchar = DCHAR_INVIS_EXPOSED;
    invis_fd.minimap = MF_MONS_HOSTILE;
    _create_colours(invis_fd);

    cloud_fd.dchar = DCHAR_CLOUD;
    cloud_fd.minimap = MF_SKIP;
    _create_colours(cloud_fd);
}

/** Get the feature_def (not necessarily in feat_defs) for this show_type.
 *
 *  @param object The show_type for which a corresponding feature_def needs
 *                to be found.
 *  @returns a feature_def corresponding to it.
 */
const feature_def &get_feature_def(show_type object)
{
    switch (object.cls)
    {
    case SH_INVIS_EXPOSED:
        return invis_fd;
    case SH_CLOUD:
        return cloud_fd;
    case SH_ITEM:
        return item_defs[object.item];
    case SH_MONSTER:
        // If this is a monster that is hidden explicitly, show items if
        // any instead, or the base feature if there are no items.
        if (object.item != SHOW_ITEM_NONE)
            return item_defs[object.item];
    case SH_FEATURE:
        return get_feature_def(object.feat);
    default:
        die("invalid show object: class %d", object.cls);
    }
}

/** Does an entry matching this type exist in feat_defs?
 *
 *  Since there is a gap in the dungeon_feature_type enum, a loop that iterates
 *  over all its entries will run into values that don't correspond to any real
 *  feature type. get_feature_def can't return a good value for this, so anything
 *  that might reference an invalid type should check this function first.
 *
 *  @param feat The dungeon_feature_type that might be invalid.
 *  @returns true if feat exists in feature-data, false otherwise.
 */
bool is_valid_feature_type(dungeon_feature_type feat)
{
    return feat >= 0 && (size_t) feat < ARRAYSZ(feat_index)
           && feat_index[feat] != -1;
}

/** Get the feature_def in feat_defs for this dungeon_feature_type.
 *
 *  @param feat The dungeon_feature_type for which the feature_def needs to be found.
 *  @returns a feature_def with the data in feature-data.h.
 */
const feature_def &get_feature_def(dungeon_feature_type feat)
{
    ASSERT_RANGE(feat, 0, NUM_FEATURES);
    ASSERTM(is_valid_feature_type(feat), "Invalid feature type %d", int(feat));
    return feat_defs[feat_index[feat]];
}

/** What should be showed for this feat on magic mapping?
 *
 *  @param feat The feat to be hidden by magic mapping.
 *  @returns the converted type.
 */
dungeon_feature_type magic_map_base_feat(dungeon_feature_type feat)
{
    switch (feat)
    {
        case DNGN_ENDLESS_SALT:
            return DNGN_FLOOR;
        case DNGN_OPEN_SEA:
        case DNGN_LAVA_SEA:
            return DNGN_SHALLOW_WATER;
        default:
            break;
    }

    const feature_def& fdef = get_feature_def(feat);
    switch (fdef.dchar)
    {
    case DCHAR_STATUE:
        return DNGN_GRANITE_STATUE;
    case DCHAR_FLOOR:
        return DNGN_FLOOR;
    case DCHAR_WAVY:
        return DNGN_SHALLOW_WATER;
    case DCHAR_ARCH:
        return DNGN_UNKNOWN_PORTAL;
    case DCHAR_FOUNTAIN:
        return DNGN_FOUNTAIN_BLUE;
    case DCHAR_WALL:
        // special-case vaults walls because the vast majority of vaults walls
        // are stone, and the rock walls look totally different
        if (you.where_are_you == BRANCH_VAULTS)
            return DNGN_STONE_WALL;
        else
            return DNGN_ROCK_WALL;
    case DCHAR_ALTAR:
        return DNGN_UNKNOWN_ALTAR;
    default:
        // We could do more, e.g. map the different upstairs together.
        return feat;
    }
}
