#!/bin/bash
# this script gets type counts for a number of categories in the game. It works
# more or less back to 0.3, with some caveats for a few version.
# this is in no way intended to suggest that the count of types is a reasonable
# way to evaluate quality...

# run from `source`

# convert values to CSV: util/what_is_left.sh | cut --delimiter=' ' -f 2 | tr '\n' ','

# we use incrementing the major as a proxy for what is currently deleted.
# this command strips out any includes, because in `enum.h` versions there were
# a bunch of stdlib includes that cause problems, not to mention tag-version.h.
# the "(placeholder)" thing is because of some enum.h stuff around 0.15; it
# should maybe be conditioned on something more specific?
cpp_args="-D TAG_MAJOR_VERSION=35"
process_cmd() {
    grep -v -e \#include "$1" -e \#pragma | grep -v "\(placeholder\)" | gcc $cpp_args -E -
}

# count draconians as one species
if [ -e dat/species ]; then
    sp_count=$(ls dat/species/*.yaml | wc -l)
    drac_count=$(ls dat/species/*draconian*.yaml | wc -l)
else
    species_file=$(test -e species-type.h && echo species-type.h || echo enum.h)
    sp_count=$(process_cmd $species_file | grep " SP_" | grep -v -e SP_UNKNOWN -e SP_RANDOM -e SP_VIABLE -e DRACONIAN -e "LAST_VALID" | wc -l)
    drac_count=0 # handled in the grep -v pass
fi
echo species: $(expr $sp_count - $drac_count + 1)

job_file=$(test -e job-type.h && echo job-type.h || echo enum.h)
echo jobs: `process_cmd $job_file | grep " JOB_" | grep -v -e JOB_UNKNOWN -e JOB_RANDOM -e JOB_VIABLE | wc -l`

# includes no god
religion_file=$(test -e god-type.h && echo god-type.h || echo enum.h)
echo religions: `process_cmd $religion_file | grep " GOD_" | grep -v -e GOD_RANDOM -e GOD_NAMELESS -e GOD_ECUMENICAL | wc -l`

spell_file=$(test -e spell-type.h && echo spell-type.h || echo enum.h)
echo spells_total: `process_cmd $spell_file | grep " SPELL_" |  wc -l`
# call this `approx` because I'm pretty sure this flag is not used very
# consistently.
echo approx_spells_monster: `process_cmd spl-data.h | grep -e "spflag::monster" -e SPFLAG_MONSTER | wc -l `

# in 0.2 and 0.1, items were in enum.h, and this doesn't handle that
item_file=$(test -e item-prop-enum.h && echo item-prop-enum.h || test -e itemprop-enum.h && echo itemprop-enum.h || echo itemprop.h)
echo armours: `process_cmd $item_file | grep " ARM_" | grep -v -e ARM_FIRST -e ARM_LAST | wc -l`
echo weapons: `process_cmd $item_file | grep " WPN_" | grep -v -e WPN_UNKNOWN -e WPN_RANDOM -e WPN_VIABLE -e WPN_THROWN -e WPN_UNARMED | wc -l`
echo brands: `process_cmd $item_file | grep " SPWPN_" | grep -v -e MAX_GHOST_BRAND -e FORBID_BRAND -e DEBUG_RANDART | wc -l`
jewel_ring=`process_cmd $item_file | grep " RING_"  | grep -v -e RING_FIRST | wc -l`
jewel_amu=`process_cmd $item_file | grep " AMU_" | grep -v -e AMU_FIRST | wc -l`
echo jewellery: $(expr $jewel_ring + $jewel_amu)

# need to be strict about this one because there is some repitition here. n.b. this enum looks a bit inaccurate currently:
evoc_misc=`process_cmd $item_file | grep "^    MISC_[A-Z_]*,\$" | grep -v -e DECK_UNKNOWN -e QUAD_DAMAGE | sort | uniq | wc -l`
# TODO: in 0.10 and before, rods were staves, and this doesn't handle that correctly.
evoc_rod=`process_cmd $item_file | grep "  ROD_" | wc -l` # the "  " here filters out a bunch of junk in enum.h
evoc_wand=`process_cmd $item_file | grep " WAND_" | wc -l`
echo evocables: $(expr $evoc_misc + $evoc_rod + $evoc_wand)

# are food types worth bothering with?

if [ -e art-data.txt ]; then
    unrand_count=$(grep "^NAME: " art-data.txt | grep -v DUMMY | wc -l)
    # this is never validated, so assumes no typos...
    # using `nogen` unfortunately won't work as this includes unique-specific items,
    # such as FDS.
    removed_unrands=$(grep "TAG_MAJOR_VERSION" art-data.txt | grep -v "end TAG_MAJOR" | wc -l)
    sprint_only=2 # axe of woe, amulet of invis
    echo unrands: $(expr $unrand_count - $removed_unrands - $sprint_only)
    # requires a build to have happened, and doesn't handle removed randarts anyways
    #echo randarts: `gcc $cpp_args -E art-enum.h | grep UNRAND_ | grep -v -e UNRAND_DUMMY -e UNRAND_START | wc -l`
else
    # unrands and fixedarts are collapsed with brands in this version, e.g. SPWPN_SINGING_SWORD
    # in principle one could extrac this from unrand.h
    echo unrands: NA
fi

# there are some weird things in the monster enum, e.g. chuck & nellie, should maybe prune a bit more carefully
mons_file=$(test -e monster-type.h && echo monster-type.h || echo enum.h)
echo monsters: `process_cmd $mons_file | grep " MONS_" | grep -v -e MONS_NO_MONSTER -e MONS_SENSED -e MONS_PLAYER -e MONS_PROGRAM_BUG -e MONS_FIRST -e MONS_LAST | wc -l`

# would be nice to break out portal branches. Would also be nice to have branch depths?
branch_file=$(test -e branch-type.h && echo branch-type.h || echo enum.h)
# manually check for forest, dwarven hall, because they were not marked as removed until around 0.15, despite never generating in a release
modern_branches=$(process_cmd $branch_file | grep " BRANCH_" | grep -v -e GLOBAL_BRANCH_INFO -e BRANCH_FIRST -e BRANCH_LAST -e BRANCH_UNUSED -e BRANCH_FOREST -e BRANCH_DWARVEN_HALL | wc -l)
# before 0.11. This undercounts portals, but I haven't found a good way
# to get a list at these versions.
ancient_branches=$(process_cmd $branch_file | grep " LEVEL_" | grep -v LEVEL_DUNGEON | wc -l)
echo branches: $(expr $modern_branches + $ancient_branches)

des_path=$(test -e dat/des && echo "dat/des" || echo "dat/")

# these may be a bit approximate; doesn't take into account weights etc.
echo layouts: $(for i in $(find $des_path -name "*.des"); do grep '^NAME: ' "$i"; done | grep "layout_" | wc -l)
echo vaults: $(for i in $(find $des_path -name "*.des"); do grep '^NAME: ' "$i"; done | grep -v "layout_" | wc -l)
