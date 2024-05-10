#!/usr/bin/env python3

"""
Generate species-data.h, aptitudes.h, species-groups.h, and species-type.h

Works with both Python 2 & 3. If that changes, update how the Makefile calls
this.
"""

from __future__ import print_function

import argparse
import os
import sys
import traceback
import re
import collections
if sys.version_info.major == 2:
    from collections import MutableMapping
else:
    from collections.abc import MutableMapping

import yaml  # pip install pyyaml

from genutil import *

class Species(MutableMapping):
    """Parser for YAML definition files.

    If any YAML content is invalid, the relevant parser function below should
    raise ValueError.
    """

    # TODO: unify with processing in from_yaml
    YAML_MAIN_FIELDS = {'TAG_MAJOR_VERSION', 'fake_mutations', 'difficulty',
            'recommended_jobs', 'enum', 'monster', 'name', 'short_name',
            'adjective', 'genus', 'species_flags', 'aptitudes', 'can_swim',
            'undead_type', 'size', 'str', 'int', 'dex', 'levelup_stats',
            'levelup_stat_frequency', 'recommended_jobs', 'recommended_weapons',
            'difficulty', 'difficulty_priority', 'create_enum', 'walking_verb',
            'altar_action', 'mutations', 'child_name', 'orc_name'}

    def __init__(self, yaml_dict):
        self.backing_dict = dict()
        self.from_yaml(yaml_dict)

    def __getitem__(self, key):
        return self.backing_dict[key]

    def __setitem__(self, key, val):
        self.backing_dict[key] = val

    def __delitem__(self, key):
        del self.backing_dict[key]

    def __iter__(self):
        return iter(self.backing_dict)

    def __len__(self):
        return len(self.store)

    def set_recommended_weapons(self, weapons):
        if not self.starting_species:
            self.backing_dict['recommended_weapons'] = ""
            return
        if not weapons:
            weapons = list(ALL_WEAPON_SKILLS)
            weapons.remove('short blades')
            weapons.remove('unarmed combat')
        out = []
        for weap in weapons:
            if weap.lower() == "maces and flails":
                weap = "maces flails"
            out.append(enumify(validate_string(weap, 'Weapon Skill',
                                          '[A-Za-z_ ]+'), SKILL_ENUM))
        self.backing_dict['recommended_weapons'] = ", ".join(out)

    def print_unknown_warnings(self, s):
        for key in s:
            if key not in self.YAML_MAIN_FIELDS:
                print("species_gen.py warning: Unknown field '%s' in species %s"
                                    % (key, self['enum']), file=sys.stderr)

    def levelup_stats_from_yaml(self, s):
        self['levelup_stats'] = levelup_stats(s.get('levelup_stats', "default"))
        self['levelup_stat_frequency'] = validate_int_range(
                s['levelup_stat_frequency'], 'levelup_stat_frequency', 0, 28)

        if (self['levelup_stats'] == empty_set("stat_type")
                                    and self['levelup_stat_frequency'] < 28):
            print("species_gen.py warning: species %s has empty levelup_stats"
                  " but a <28 levelup_stat_frequency."
                  % (self['enum']), file=sys.stderr)

        if (self['levelup_stats'] != empty_set("stat_type")
                                    and self['levelup_stat_frequency'] > 27):
            print("species_gen.py warning: species %s has non-empty"
                  " levelup_stats but a levelup_stat_frequency that is > 27."
                  % (self['enum']), file=sys.stderr)

    def from_yaml(self, s):
        # Pre-validation
        if s.get('TAG_MAJOR_VERSION', None) is not None:
            if not isinstance(s['TAG_MAJOR_VERSION'], int):
                raise ValueError('TAG_MAJOR_VERSION must be an integer')
        if not isinstance(s.get('fake_mutations', []), list):
            raise ValueError('fake_mutations must be a list')
        self.starting_species = s.get('difficulty') != False
        has_recommended_jobs = bool(s.get('recommended_jobs'))
        if self.starting_species != has_recommended_jobs:
            raise ValueError('recommended_jobs must not be empty (or'
                                                ' difficulty must be False)')

        # Set attributes
        self['enum'] = validate_string(s['enum'], 'enum', 'SP_[A-Z_]+$')
        self['monster_name'] = validate_string(s['monster'], 'monster',
                                                'MONS_[A-Z_]+$')
        self['name'] = validate_string(s['name'], 'name', '..+')
        self['short_name'] = s.get('short_name', s['name'][:2])
        self['adjective'] = quote_or_nullptr('adjective', s)
        self['genus'] = quote_or_nullptr('genus', s)
        self['species_flags'] = species_flags(s.get('species_flags', []))
        self['xp'] = validate_int_range(s['aptitudes']['xp'], 'xp', -10, 10)
        self['hp'] = validate_int_range(s['aptitudes']['hp'], 'hp', -10, 10)
        self['mp'] = validate_int_range(s['aptitudes']['mp_mod'], 'mp_mod',
                                                                        -5, 20)
        self['wl'] = validate_int_range(s['aptitudes']['wl'], 'wl', 0, 20)
        self['aptitudes'] = aptitudes(s['aptitudes'])
        self['habitat'] = 'HT_LAND' if not s.get('can_swim') else 'HT_WATER'
        self['undead'] = undead_type(s.get('undead_type', 'alive'))
        self['size'] = size(s.get('size', 'medium'))
        self['str'] = validate_int_range(s['str'], 'str', 1, 100)
        self['int'] = validate_int_range(s['int'], 'int', 1, 100)
        self['dex'] = validate_int_range(s['dex'], 'dex', 1, 100)
        self.levelup_stats_from_yaml(s)
        self['mutations'] = mutations(s.get('mutations', {}))
        self['fake_mutations_long'] = fake_mutations_long(
                                            s.get('fake_mutations', []))
        self['fake_mutations_short'] = fake_mutations_short(
                                            s.get('fake_mutations', []))
        self['recommended_jobs'] = recommended_jobs(
                                            s.get('recommended_jobs', []))
        self.set_recommended_weapons(s.get('recommended_weapons', []))
        self['difficulty'] = difficulty(s.get('difficulty'))
        self['difficulty_priority'] = validate_int_range(difficulty_priority(
            s.get('difficulty_priority', 0)), 'difficulty_priority', 0, 1000)
        self['create_enum'] = validate_bool(
                                    s.get('create_enum', False), 'create_enum')
        self['walking_verb'] = quote_or_nullptr('walking_verb', s)
        self['altar_action'] = quote_or_nullptr('altar_action', s)
        self['child_name']   = quote_or_nullptr('child_name', s)
        self['orc_name']     = quote_or_nullptr('orc_name', s)

        if 'TAG_MAJOR_VERSION' in s:
            self['tag_major_version_opener'] = (
                        "#if TAG_MAJOR_VERSION == %s" % s['TAG_MAJOR_VERSION'])
            self['tag_major_version_closer'] = "#endif"
        else:
            self['tag_major_version_opener'] = ''
            self['tag_major_version_closer'] = ''
        self.print_unknown_warnings(s)

SpeciesGroup = collections.namedtuple('SpeciesGroup',
                                            ['position', 'width', 'species'])
SpeciesGroupEntry = collections.namedtuple('SpeciesGroupEntry',
                                            ['priority', 'enum'])
SPECIES_GROUPS_TEMPLATE = collections.OrderedDict()
SPECIES_GROUPS_TEMPLATE['Simple'] = SpeciesGroup('coord_def(0, 0)', '50', [])
SPECIES_GROUPS_TEMPLATE['Intermediate'] = SpeciesGroup('coord_def(1, 0)', '20', [])
SPECIES_GROUPS_TEMPLATE['Advanced'] = SpeciesGroup('coord_def(2, 0)', '20', [])
SPECIES_GROUP_TEMPLATE = """
    {{
        "{name}",
        {position},
        {width},
        {{ {species} }}
    }},
"""
ALL_APTITUDES = ('fighting', 'short_blades', 'long_blades', 'axes',
    'maces_and_flails', 'polearms', 'staves', 'ranged weapons',
    'throwing', 'armour', 'dodging', 'stealth', 'shields', 'unarmed_combat',
    'spellcasting', 'conjurations', 'hexes', 'summoning',
    'necromancy', 'forgecraft', 'translocations', 'fire_magic',
    'ice_magic', 'air_magic', 'earth_magic', 'alchemy', 'invocations',
    'evocations', 'shapeshifting')
UNDEAD_TYPES = ('alive', 'undead', 'semi_undead')
SIZES = ('SIZE_TINY', 'SIZE_LITTLE', 'SIZE_SMALL', 'SIZE_MEDIUM', 'SIZE_LARGE',
    'SIZE_GIANT')
ALL_STATS = ('str', 'int', 'dex')
ALL_WEAPON_SKILLS = ('short blades', 'long blades', 'axes',
    'maces and flails', 'polearms', 'staves', 'unarmed combat')

ALL_SPECIES_FLAGS = {'draconian', 'no_hair', 'no_bones',
    'small_torso', 'barding', 'no_feet',
    'no_blood', 'no_ears'}

JOB_ENUM = 'JOB'
UNDEAD_TYPE_ENUM = 'US'
SPECIES_FLAG_ENUM = 'SPF'
SKILL_ENUM = 'SK'

def recommended_jobs(jobs):
    return ', '.join(enumify(validate_string(job, 'Job', '[A-Za-z_ ]+'),
                             JOB_ENUM) for job in jobs)


def size(size):
    val = "SIZE_%s" % size.upper()
    if val not in SIZES:
        raise ValueError('Size %s is invalid, pick one of tiny, little, '
                                    'small, medium, large, big, or giant')
    return val


def species_flags(flags):
    global ALL_SPECIES_FLAGS
    out = set()
    for f in flags:
        if f not in ALL_SPECIES_FLAGS:
            raise ValueError("Unknown species flag %s" % f)
        out.add(enumify(f, SPECIES_FLAG_ENUM))
    if not out:
        out.add('SPF_NONE')
    return ' | '.join(out)


def undead_type(type):
    if type not in UNDEAD_TYPES:
        raise ValueError('Unknown undead type %s' % type)
    return enumify(type, UNDEAD_TYPE_ENUM)


def levelup_stats(stats):
    if stats == "default":
        stats = ALL_STATS
    else:
        # this is pretty loose type checking because we don't want to make
        # any assumptions about how yaml parser handles sequences.
        if isinstance(stats, str):
            raise ValueError(
                "Expected `default` or list for levelup_stats, not `%s`" % stats)
        for s in stats:
            if s not in ALL_STATS:
                raise ValueError('Unknown stat %s' % s)
    if len(stats) == 0:
        return empty_set("stat_type")
    else:
        return make_list(', '.join("STAT_%s" % s.upper() for s in stats))


def mutations(mut_def):
    out = []
    for xl, muts in sorted(mut_def.items()):
        validate_int_range(xl, 'Mutation Level', 1, 27)
        if not isinstance(muts, dict):
            raise ValueError('Mutation key %s doesn\'t seem to have a valid '
                                        'map of {name: amount} entries' % xl)
        for mut_name, amt in sorted(muts.items()):
            validate_string(mut_name, 'Mutation Name', 'MUT_[A-Z_]+')
            validate_int_range(amt, 'Mutation Amount', -3, 3)
            out.append("{{ {mut_name}, {amt}, {xl} }}".format(
                mut_name=mut_name,
                xl=xl,
                amt=amt,
            ))
    return make_list(', '.join(out))

def fake_mutations_long(fmut_def):
    return make_list(', '.join(quote(m.get('long'))
                                    for m in fmut_def if m.get('long')))

def fake_mutations_short(fmut_def):
    return make_list(', '.join(quote(m.get('short'))
                                    for m in fmut_def if m.get('short')))

def aptitudes(apts):
    for apt, val in apts.items():
        if apt not in ALL_APTITUDES and apt not in ('xp', 'hp', 'mp_mod', 'wl'):
            raise ValueError("Unknown aptitude (typo?): %s" % apt)
        validate_int_range(val, apt, -10, 20)
    return apts


def difficulty(d):
    if d not in SPECIES_GROUPS_TEMPLATE.keys() and d is not False:
        raise ValueError("Unknown difficulty: %s" % d)
    return d


def difficulty_priority(prio):
    try:
        return int(prio)
    except ValueError:
        raise ValueError('difficulty_priority value "%s" is not an integer' %
                                prio)


def generate_aptitudes_data(s, template):
    """Convert a species in YAML representation to aptitudes.h format.

    If any of the required data can't be loaded, ValueError is raised and
    passed to the caller.
    """
    # Now generate the aptitudes block. The default is 0.
    # Note: We have to differentiate between 0 and 'False' aptitudes specified
    # in YAML. The latter is UNUSABLE_SKILL.
    aptitudes = {apt: 0 for apt in ALL_APTITUDES}
    for apt, val in s['aptitudes'].items():
        if apt in ('xp', 'hp', 'mp_mod', 'wl'):
            continue
        if val is False:
            aptitudes[apt] = 'UNUSABLE_SKILL'
        else:
            aptitudes[apt] = val
    aptitudes['tag_major_version_opener'] = s['tag_major_version_opener']
    aptitudes['tag_major_version_closer'] = s['tag_major_version_closer']
    return template.format(enum = s['enum'], **aptitudes)


def update_species_group(sg, s):
    difficulty = s['difficulty']
    if difficulty is False:
        # Don't add this species to the species select screen
        return sg
    entry = SpeciesGroupEntry(s['difficulty_priority'], s['enum'])
    sg[difficulty].species.append(entry)
    return sg


def generate_species_groups(sg):
    out = ''
    for name, group in sg.items():
        out += SPECIES_GROUP_TEMPLATE.format(
            name = name,
            position = group.position,
            width = group.width,
            species = ', '.join(
                e.enum for e in reversed(sorted(group.species))),
        )
    return out


def generate_species_type_data(s):
    if s['create_enum'] == False:
        return ''
    else:
        return '    %s,\n' % s['enum']


def main():
    parser = argparse.ArgumentParser(description='Generate species-data.h')
    parser.add_argument('datadir', help='dat/species source dir')
    parser.add_argument('templatedir',
                    help='util/species-gen template source dir')
    parser.add_argument('species_data', help='species-data.h output file path')
    parser.add_argument('aptitudes', help='aptitudes.h output file path')
    parser.add_argument('species_groups',
                    help='species-groups.h output file path')
    parser.add_argument('species_type', help='species-type.h output file path')
    args = parser.parse_args()

    # Validate args
    if not os.path.isdir(args.datadir):
        print('datadir isn\'t a directory')
        sys.exit(1)
    if not os.path.isdir(args.templatedir):
        print('templatedir isn\'t a directory')
        sys.exit(1)

    # Load all species
    all_species = []
    for f_name in sorted(os.listdir(args.datadir)):
        if not f_name.endswith('.yaml'):
            continue
        f_path = os.path.join(args.datadir, f_name)
        try:
            species_spec = yaml.safe_load(open(f_path))
        except yaml.YAMLError as e:
            print("Failed to load %s: %s" % (f_name, e))
            sys.exit(1)

        try:
            species = Species(species_spec)
        except (ValueError, KeyError) as e:
            print("Failed to load %s" % f_name)
            traceback.print_exc()
            sys.exit(1)
        all_species.append(species)

    # Generate code
    species_data_out_text = load_template(args.templatedir,
                                                'species-data-header.txt')
    aptitudes_out_text = load_template(args.templatedir, 'aptitudes-header.txt')
    species_type_out_text = load_template(args.templatedir,
                                                'species-type-header.txt')

    species_data_template = load_template(args.templatedir,
                                                'species-data-species.txt')
    aptitude_template = load_template(args.templatedir, 'aptitude-species.txt')
    species_groups = SPECIES_GROUPS_TEMPLATE
    for species in all_species:
        # species-data.h
        species_data_out_text += species_data_template.format(**species)
        # aptitudes.h
        aptitudes_out_text += generate_aptitudes_data(species,
                                                            aptitude_template)
        # species-type.h
        species_type_out_text += generate_species_type_data(species)
        # species-groups.h
        species_groups = update_species_group(species_groups, species)

    species_data_out_text += load_template(args.templatedir,
                                        'species-data-deprecated-species.txt')
    species_data_out_text += load_template(args.templatedir,
                                        'species-data-footer.txt')
    with open(args.species_data, 'w') as f:
        f.write(species_data_out_text)

    aptitudes_out_text += load_template(args.templatedir,
                                        'aptitudes-deprecated-species.txt')
    aptitudes_out_text += load_template(args.templatedir,
                                        'aptitudes-footer.txt')
    with open(args.aptitudes, 'w') as f:
        f.write(aptitudes_out_text)

    species_type_out_text += load_template(args.templatedir,
                                        'species-type-footer.txt')
    with open(args.species_type, 'w') as f:
        f.write(species_type_out_text)

    species_groups_out_text = ''
    species_groups_out_text += load_template(args.templatedir,
                                        'species-groups-header.txt')
    species_groups_out_text += generate_species_groups(species_groups)
    species_groups_out_text += load_template(args.templatedir,
                                        'species-groups-footer.txt')
    with open(args.species_groups, 'w') as f:
        f.write(species_groups_out_text)


if __name__ == '__main__':
    main()
