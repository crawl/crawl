#!/usr/bin/env python

# --- WORK IN PROGRESS --- #
# Aim: to generate job-type.h and job-data.h in the same way as species,
# from yaml files

# NOTE Wanderers have empty skills and equipment sets

from __future__ import print_function

import argparse
import os
import sys
import traceback
import collections
import re

if sys.version_info.major == 2:
    from collections import MutableMapping
else:
    from collections.abc import MutableMapping

import yaml  # pip install pyyaml

def quote_or_nullptr(key, d):
    if key in d:
        return quote(d[key])
    else:
        return 'nullptr'

class Job(MutableMapping):
    """Parser for YAML definition files.

    If any YAML content is invalid, the relevant parser function below should
    raise ValueError.
    """

    # TODO: unify with processing in from_yaml
    YAML_MAIN_FIELDS = {'TAG_MAJOR_VERSION', # 'category', 'category_priority'
            'recommended_species', 'enum', 'name', 'short_name', 'str', 'int',
            'dex', 'weapon_choice', 'create_enum', 'skills', 'equipment',
            'spells'}

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

    def print_unknown_warnings(self, s):
        for key in s:
            if key not in self.YAML_MAIN_FIELDS:
                print("species_gen.py warning: Unknown field '%s' in species %s"
                                    % (key, self['enum']), file=sys.stderr)

    def from_yaml(self, s):
        # Pre-validation
        if s.get('TAG_MAJOR_VERSION', None) is not None:
            if not isinstance(s['TAG_MAJOR_VERSION'], int):
                raise ValueError('TAG_MAJOR_VERSION must be an integer')
    #    self.starting_species = s.get('difficulty') != False
    #    has_recommended_species = bool(s.get('recommended_species'))
    #    if self.starting_species != has_recommended_species:
    #        raise ValueError('recommended_jobs must not be empty (or'
    #                                            ' difficulty must be False)')

        # Set attributes
        self['enum'] = validate_string(s['enum'], 'enum', 'JOB_[A-Z_]+$')
        self['name'] = validate_string(s['name'], 'name', '..+')
        self['short_name'] = s.get('short_name', s['name'][:2])
        # TODO: currently done by skill enum values; change to be strings?
        self['skills'] = skills(s.get('skills', {}))
        self['str'] = validate_int_range(s['str'], 'str', -15, 15)
        self['int'] = validate_int_range(s['int'], 'int', -15, 15)
        self['dex'] = validate_int_range(s['dex'], 'dex', -15, 15)
        self['recommended_species'] = recommended_species(
                                            s.get('recommended_species', []))
        self['spells'] = spells(s.get('spells', []))
        self['equipment'] = equipment(s.get('equipment', []))
        self['weapon_choice'] = weapon_choice(
                                        s.get('weapon_choice', 'WCHOICE_NONE'))
    #    self['difficulty'] = difficulty(s.get('difficulty'))
    #    self['difficulty_priority'] = validate_int_range(difficulty_priority(
    #        s.get('difficulty_priority', 0)), 'difficulty_priority', 0, 1000)
        self['create_enum'] = validate_bool(
                                    s.get('create_enum', True), 'create_enum')

        if 'TAG_MAJOR_VERSION' in s:
            self['tag_major_version_opener'] = (
                        "#if TAG_MAJOR_VERSION == %s" % s['TAG_MAJOR_VERSION'])
            self['tag_major_version_closer'] = "#endif"
        else:
            self['tag_major_version_opener'] = ''
            self['tag_major_version_closer'] = ''
        self.print_unknown_warnings(s)

# SpeciesGroup = collections.namedtuple('SpeciesGroup',
#                                             ['position', 'width', 'species'])
# SpeciesGroupEntry = collections.namedtuple('SpeciesGroupEntry',
#                                             ['priority', 'enum'])
# SPECIES_GROUPS_TEMPLATE = {
#     'Simple': SpeciesGroup('coord_def(0, 0)', '20', []),
#     'Intermediate': SpeciesGroup('coord_def(25, 0)', '20', []),
#     'Advanced': SpeciesGroup('coord_def(50, 0)', '20', []),
# }
# SPECIES_GROUP_TEMPLATE = """
#     {{
#         "{name}",
#         {position},
#         {width},
#         {{ {species} }}
#     }},
# """

# SK_WEAPON is used when weapon is chosen by player
ALL_SKILLS = ('SK_FIGHTING', 'SK_SHORT_BLADES', 'SK_LONG_BLADES', 'SK_AXES',
    'SK_MACES_FLAILS', 'SK_POLEARMS', 'SK_STAVES', 'SK_RANGED_WEAPONS',
    'SK_THROWING', 'SK_ARMOUR', 'SK_DODGING', 'SK_STEALTH',
    'SK_SHIELDS', 'SK_UNARMED_COMBAT', 'SK_SPELLCASTING', 'SK_CONJURATIONS',
    'SK_HEXES','SK_SUMMONINGS', 'SK_NECROMANCY', 'SK_TRANSLOCATIONS',
    'SK_FIRE_MAGIC', 'SK_ICE_MAGIC', 'SK_AIR_MAGIC', 'SK_EARTH_MAGIC',
    'SK_ALCHEMY', 'SK_INVOCATIONS', 'SK_EVOCATIONS', 'SK_SHAPESHIFTING',
    'SK_WEAPON')
ALL_WCHOICES = ('WCHOICE_NONE', 'WCHOICE_PLAIN', 'WCHOICE_GOOD')


def recommended_species(species):
    return ', '.join(validate_string(race, 'Species', 'SP_[A-Z_]+')
                            for race in species)


def validate_string(val, name, pattern):
    '''
    Validate a string.

    Note that re.match anchors to the start of the string, so you don't need to
    prefix the pattern with '^'. But it doesn't require matching to the end, so
    you'll probably want to suffix '$'.

    We don't validate spells or equipment; there are too many of them!
    '''
    if not isinstance(val, str):
        raise ValueError('%s isn\'t a string' % name)
    if re.match(pattern, val):
        return val
    else:
        raise ValueError('%s doesn\'t match pattern %s' % (val, pattern))
    return val


def validate_bool(val, name):
    '''Validate a boolean.'''
    if not isinstance(val, bool):
        raise ValueError('%s isn\'t a boolean' % name)
    return val


def validate_int_range(val, name, min, max):
    if not isinstance(val, int):
        raise ValueError('%s isn\'t an integer' % name)
    if not min <= val <= max:
        raise ValueError('%s isn\'t between %s and %s' % (name, min, max))
    return val


def enumify(s):
    return s.replace(' ', '_').upper()


def quote(s):
    if not isinstance(s, str):
        raise ValueError('Expected a string but got %s' % repr(s))
    return '"%s"' % s


global LIST_TEMPLATE
LIST_TEMPLATE = """    {{ {list} }}"""
global SPELL_LIST_TEMPLATE
SPELL_LIST_TEMPLATE = """    {{\n{list}\n    }}"""

def empty_set(typ):
    return "    set<%s>()" % typ

def make_list(list_str, is_spell_list = False):
    global LIST_TEMPLATE
    #TODO: add linebreaks + indents to obey 80 chars?
    if len(list_str.strip()) == 0:
        return "    {}"
    elif is_spell_list:
        return SPELL_LIST_TEMPLATE.format(list=list_str)
    else:
        return LIST_TEMPLATE.format(list=list_str)


def skills(sk):
    out = []
    for skill, val in sorted(sk.items()):
        if skill not in ALL_SKILLS:
            raise ValueError("Unknown skill (typo?): %s" % skill)
        validate_int_range(val, skill, 0, 10)
        out.append("{{ {skill}, {val} }}".format(skill=skill, val=val))
    return make_list(', '.join(out))


def equipment(items):
    return make_list(', '.join('"{item}"'.format(item=item) for item in items))


def weapon_choice(wchoice):
    if wchoice not in ALL_WCHOICES:
        raise ValueError("Unknown weapon choice: %s" % wchoice)
    return wchoice

def spells(spell_list):
    return make_list(',\n'.join('        {spell}'.format(spell=spell)
                                for spell in spell_list), True)


# def difficulty(d):
#     if d not in SPECIES_GROUPS_TEMPLATE.keys() and d is not False:
#         raise ValueError("Unknown difficulty: %s" % d)
#     return d


# def difficulty_priority(prio):
#     try:
#         return int(prio)
#     except ValueError:
#         raise ValueError('difficulty_priority value "%s" is not an integer' %
#                                 prio)


# def update_species_group(sg, s):
#     difficulty = s['difficulty']
#     if difficulty is False:
#         # Don't add this species to the species select screen
#         return sg
#     entry = SpeciesGroupEntry(s['difficulty_priority'], s['enum'])
#     sg[difficulty].species.append(entry)
#     return sg


# def generate_species_groups(sg):
#     out = ''
#     for name, group in sg.items():
#         out += SPECIES_GROUP_TEMPLATE.format(
#             name = name,
#             position = group.position,
#             width = group.width,
#             species = ', '.join(
#                 e.enum for e in reversed(sorted(group.species))),
#         )
#     return out


def generate_job_type_data(s):
    if s['create_enum'] == False:
        return ''
    else:
        return '    %s,\n' % s['enum']


def load_template(templatedir, name):
    return open(os.path.join(templatedir, name)).read()


def main():
    parser = argparse.ArgumentParser(description='Generate job-data.h')
    parser.add_argument('datadir', help='dat/jobs source dir')
    parser.add_argument('templatedir',
                    help='util/job-gen template source dir')
    parser.add_argument('job_data', help='job-data.h output file path')
    parser.add_argument('job_type', help='job-type.h output file path')
    args = parser.parse_args()

    # Validate args
    if not os.path.isdir(args.datadir):
        print('datadir isn\'t a directory')
        sys.exit(1)
    if not os.path.isdir(args.templatedir):
        print('templatedir isn\'t a directory')
        sys.exit(1)

    # Load all species
    all_jobs = []
    for f_name in sorted(os.listdir(args.datadir)):
        if not f_name.endswith('.yaml'):
            continue
        f_path = os.path.join(args.datadir, f_name)
        try:
            job_spec = yaml.safe_load(open(f_path))
        except yaml.YAMLError as e:
            print("Failed to load %s: %s" % (f_name, e))
            sys.exit(1)

        try:
            job = Job(job_spec)
        except (ValueError, KeyError) as e:
            print("Failed to load %s" % f_name)
            traceback.print_exc()
            sys.exit(1)
        all_jobs.append(job)

    # Generate code
    job_data_out_text = load_template(args.templatedir,
                                                'job-data-header.txt')
    job_type_out_text = load_template(args.templatedir,
                                                'job-type-header.txt')

    job_data_template = load_template(args.templatedir,
                                                'job-data-jobs.txt')

#    species_groups = SPECIES_GROUPS_TEMPLATE
    for job in all_jobs:
        # species-data.h
        job_data_out_text += job_data_template.format(**job)
        # species-type.h
        job_type_out_text += generate_job_type_data(job)
#        # species-groups.h
#        species_groups = update_species_group(species_groups, species)

    job_data_out_text += load_template(args.templatedir,
                                        'job-data-deprecated-jobs.txt')
    job_data_out_text += load_template(args.templatedir,
                                        'job-data-footer.txt')
    with open(args.job_data, 'w') as f:
        f.write(job_data_out_text)

    job_type_out_text += load_template(args.templatedir,
                                        'job-type-footer.txt')
    with open(args.job_type, 'w') as f:
        f.write(job_type_out_text)

    # species_groups_out_text = ''
    # species_groups_out_text += load_template(args.templatedir,
    #                                     'species-groups-header.txt')
    # species_groups_out_text += generate_species_groups(species_groups)
    # species_groups_out_text += load_template(args.templatedir,
    #                                     'species-groups-footer.txt')
    # with open(args.species_groups, 'w') as f:
    #     f.write(species_groups_out_text)


if __name__ == '__main__':
    main()
