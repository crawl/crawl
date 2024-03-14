#!/usr/bin/env python
"""
Generate the files job-data.h, job-type.h and job-groups.h from the YAML files
in ../dat/jobs/.
"""

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

from genutil import *

class Job(MutableMapping):
    """Parser for YAML definition files.

    If any YAML content is invalid, the relevant parser function below should
    raise ValueError.
    """

    # TODO: unify with processing in from_yaml
    YAML_MAIN_FIELDS = {'TAG_MAJOR_VERSION', 'category', 'category_priority',
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
        self.starting_job = s.get('difficulty') != False
        has_recommended_species = bool(s.get('recommended_species'))
        if self.starting_job != has_recommended_species:
            raise ValueError('recommended_jobs must not be empty (or'
                                                ' category must be False)')

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
                                        s.get('weapon_choice', 'none'))
        self['category'] = category(s.get('category'))
        self['category_priority'] = validate_int_range(category_priority(
            s.get('category_priority', 0)), 'category_priority', 0, 1000)
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

JobGroup = collections.namedtuple('JobGroup', ['position', 'width', 'jobs'])
JobGroupEntry = collections.namedtuple('JobGroupEntry', ['priority', 'enum'])
JOB_GROUPS_TEMPLATE = {
    'Warrior': JobGroup('coord_def(0, 0)', '15', []),
    'Adventurer': JobGroup('coord_def(0, 7)', '15', []),
    'Zealot': JobGroup('coord_def(15, 0)', '20', []),
    'Warrior-mage': JobGroup('coord_def(35, 0)', '21', []),
    'Mage': JobGroup('coord_def(56, 0)', '22', []),
}
JOB_GROUP_TEMPLATE = """
    {{
        "{name}",
        {position},
        {width},
        {{ {jobs} }}
    }},
"""

# SK_WEAPON is used when weapon is chosen by player
ALL_SKILLS = ('fighting', 'short blades', 'long blades', 'axes',
    'maces and flails', 'polearms', 'staves', 'ranged weapons',
    'throwing', 'armour', 'dodging', 'stealth',
    'shields', 'unarmed combat', 'spellcasting', 'conjurations',
    'hexes', 'summonings', 'necromancy', 'translocations',
    'fire magic', 'ice magic', 'air magic', 'earth magic',
    'alchemy', 'invocations', 'evocations', 'shapeshifting',
    'weapon')
ALL_WCHOICES = ('none', 'plain', 'good')

#SPELL_ENUM = 'SPELL'
SKILL_ENUM = 'SK'
SPECIES_ENUM = 'SP'

def recommended_species(species):
     # Allow writing 'draconian' for SP_BASE_DRACONIAN
    out = []
    for race in species:
        if race.lower() == "draconian":
            race = "base draconian"
        out.append(enumify(validate_string(race, 'Species',
                                           '[A-Za-z_ ]+'), SPECIES_ENUM))
    return make_list(', '.join(out))


def skills(sk):
    out = []
    for skill, val in sorted(sk.items()):
        if skill not in ALL_SKILLS:
            raise ValueError("Unknown skill (typo?): %s" % skill)
        # Hack: allow "maces and flails" for SK_MACES_FLAILS
        if skill.lower() == "maces and flails":
            skill = "maces flails"
        validate_int_range(val, skill, 0, 10)
        out.append("{{ {skill}, {val} }}".format(
                            skill=enumify(skill, SKILL_ENUM), val=val))
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


def category(c):
    if c not in JOB_GROUPS_TEMPLATE.keys() and c is not False:
        raise ValueError("Unknown category: %s" % c)
    return c


def category_priority(prio):
    try:
        return int(prio)
    except ValueError:
        raise ValueError('category_priority value "%s" is not an integer' %
                                prio)


def update_job_group(sg, s):
    category = s['category']
    if category is False:
        # Don't add this species to the species select screen
        return sg
    entry = JobGroupEntry(s['category_priority'], s['enum'])
    sg[category].jobs.append(entry)
    return sg


def generate_job_groups(sg):
    out = ''
    for name, group in sg.items():
        out += JOB_GROUP_TEMPLATE.format(
            name = name,
            position = group.position,
            width = group.width,
            jobs = ', '.join(
                e.enum for e in reversed(sorted(group.jobs))),
        )
    return out


def generate_job_type_data(s):
    if s['create_enum'] == False:
        return ''
    else:
        return '    %s,\n' % s['enum']


def main():
    parser = argparse.ArgumentParser(description='Generate job-data.h')
    parser.add_argument('datadir', help='dat/jobs source dir')
    parser.add_argument('templatedir',
                    help='util/job-gen template source dir')
    parser.add_argument('job_data', help='job-data.h output file path')
    parser.add_argument('job_groups', help='job-groups.h output file path')
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

    job_groups = JOB_GROUPS_TEMPLATE
    for job in all_jobs:
        # job-data.h
        job_data_out_text += job_data_template.format(**job)
        # job-type.h
        job_type_out_text += generate_job_type_data(job)
        # job-groups.h
        job_groups = update_job_group(job_groups, job)

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

    job_groups_out_text = ''
    job_groups_out_text += load_template(args.templatedir,
                                        'job-groups-header.txt')
    job_groups_out_text += generate_job_groups(job_groups)
    job_groups_out_text += load_template(args.templatedir,
                                        'job-groups-footer.txt')
    with open(args.job_groups, 'w') as f:
        f.write(job_groups_out_text)


if __name__ == '__main__':
    main()
