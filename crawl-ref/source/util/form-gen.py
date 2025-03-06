#!/usr/bin/env python3

"""
Generate form-data.h

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

from genutil import *

import yaml  # pip install pyyaml

class Field:
    def __init__(self, f, required = False):
        self.f = f
        self.required = required

class Form(MutableMapping):
    """Parser for YAML definition files.

    If any YAML content is invalid, the relevant parser function below should
    raise ValueError.
    """

    def __init__(self, yaml_dict, enum_list):
        self.backing_dict = dict()
        self.from_yaml(yaml_dict, enum_list)

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

    def from_yaml(self, s, enum_list):
        for key in s:
            if key not in keyfns:
                raise ValueError("unknown key '%s'" % key)
            if key in self:
                raise ValueError("duplicate key '%s'" % key)
            try:
                self[key] = keyfns[key].f(s[key])
            except ValueError as e:
                raise ValueError("failed to parse '%s': %s" % (key, e))

        for key in keyfns:
            if key not in self and keyfns[key].required:
                raise ValueError("missing required key '%s'" % key)

        if self['enum'] not in enum_list:
            raise ValueError("%s does not match any enum" % self['enum'])

        # Derive some names from other names, if none are overrided
        if 'short_name' not in self:
            self['short_name'] = self['enum'].capitalize()
        if 'long_name' not in self:
            self['long_name'] = self['enum'] + "-form"
        if 'wiz_name' not in self:
            self['wiz_name'] = self['enum']

        for key in defaults:
            if key not in self:
                self[key] = defaults[key]

        self["full_enum"] = "transformation::" + self['enum']
        self["joined_resists"] = ' | '.join(r.format() for r in self["resists"])
        self["ac_scaling"] = self["ac"].format()
        self["unarmed_scaling"] = self["unarmed"].format()

        if 'TAG_MAJOR_VERSION' in s:
            self['tag_major_version_opener'] = (
                        "\n#if TAG_MAJOR_VERSION == %s" % s['TAG_MAJOR_VERSION'])
            self['tag_major_version_closer'] = "#endif\n"
        else:
            self['tag_major_version_opener'] = ''
            self['tag_major_version_closer'] = ''

def get_fields(d, names):
    fields = []
    for f in names:
        if f not in d:
            raise ValueError("missing field '%s'", f)
        fields.append(d[f])
    if len(d) != len(names):
        raise ValueError("unexpected fields") # XXX add more context
    return fields

def parse_bool(s):
    if not isinstance(s, bool):
        raise ValueError("isn't a bool")
    return 'true' if s else 'false'

def parse_str(s):
    if not isinstance(s, str):
        raise ValueError("isn't a string")
    return s

def parse_str_list(s):
    for a in s:
        if not isinstance(a, str):
            raise ValueError("isn't a string")
    return ', '.join(quote(a) for a in s)

def parse_attack_verbs(s):
    if type(s) is list:
        if len(s) != 1 and len(s) != 4:
            raise ValueError("isn't 1 or 4 strings long")
        return "{ " + parse_str_list(s) + " }"
    if type(s) is str:
        return s.upper() + "_VERBS"
    else:
        raise ValueError("isn't a list or an enum name")

def parse_muts(s):
    if type(s) is not list:
        raise ValueError("isn't a list")
    ret = []
    for a in s:
        ret.append("{" + parse_str_list(a) + "}")
    return ',\n     '.join(ret)

def parse_num(s, min, max):
    if not isinstance(s, int):
        raise ValueError("isn't an int")
    if not min <= s <= max:
        raise ValueError("isn't between %s and %s" % (min, max))
    return s

def parse_flags(s):
    flags = []
    for f in s:
        flags.append("M_" + f.upper())
    return flags

def parse_resists(s):
    resists = []
    for r in s:
        resists.append(parse_resist(r, s[r]))
    return resists

class Resist:
    def __init__(self, min = 0, max = 1):
        self.min = min;
        self.max = max;

ALL_RESISTS = {
    'fire': Resist(-1, 4),
    'cold': Resist(-1, 4),
    'elec': Resist(-1, 4),
    'poison': Resist( -1, 4),
    'neg': Resist(0, 4),
    'corr': Resist(0, 4),
    'miasma': Resist(0, 4),
    'torment': Resist(),
    'petrify': Resist(),
    'damnation': Resist(),
    'steam': Resist(),
}

class ResVal:
    def __init__(self, enum, val):
        self.enum = enum
        self.val = val

    def format(self):
        if self.val == 0 or self.val == 1:
            return self.enum
        return "mrd(%s, %d)" % (self.enum, self.val)

def parse_resist(name, value):
    if not name in ALL_RESISTS:
        raise ValueError("unknown resist '%s', name")

    res = ALL_RESISTS[name]
    try:
        val = parse_num(value, res.min, res.max)
    except ValueError as e:
        raise ValueError("invalid value for resist '%s': %s" % (name, e))
    return ResVal("MR_RES_%s" % name.upper(), val)

class Skill:
    def __init__(self, min, max):
        self.min = min;
        self.max = max;

def parse_skill(s):
    char, colour = get_fields(s, ["min", "max"])
    return Skill(parse_num(s["min"], 0, 27), parse_num(s["max"], 0, 27))

# XXX: Based off equipment_slot. See also transform.cc
ALL_SLOTS = {
    'weapon': 1 << 1,
    'offhand': 1 << 2,
    'body': 1 << 3,
    'helmet': 1 << 4,
    'gloves': 1 << 5,
    'boots': 1 << 6,
    'barding': 1 << 7,
    'cloak': 1 << 8,
    'ring': 1 << 9,
    'amulet': 1 << 10,
    'weapon_or_offhand': 1 << 12,
    'held': 1 << 1 | 1 << 2 | 1 << 12,
    'aux': 1 << 4 | 1 << 5 | 1 << 6 | 1 << 7 | 1 << 8,
    'physical': 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6 | 1 << 7 | 1 << 8 | 1 << 12,
    'jewellery': 1 << 9 | 1 << 10,
    'all': 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6 | 1 << 7 | 1 << 8 | 1 << 9 | 1 << 10 | 1 << 12,
}

def parse_slots(s):
    slot = 0
    for f in s:
        if not f in ALL_SLOTS:
            raise ValueError("unknown slot type '%s', f")
        slot |= ALL_SLOTS[f]
    return slot

def parse_enum(s, prefix, vals):
    if s not in vals:
        raise ValueError("%s not in %s" % (s, vals))
    return prefix + s.upper()

SIZES = {'tiny', 'little', 'small', 'medium', 'large', 'giant', 'character'}
def parse_size(s):
    return parse_enum(s, "SIZE_", SIZES)

class FormScaling:
    def __init__(self, base=0, scaling=0, xl_based=False):
        self.base = base;
        self.scaling = scaling;
        self.xl_based = xl_based;

    def format(self):
        if self.base == 0 and self.scaling == 0 and self.xl_based == False:
            return '{}'

        ret = "FormScaling()"
        if self.base > 0:
            ret += ".Base(" + str(self.base) + ")"
        if self.scaling > 0:
            ret += ".Scaling(" + str(self.scaling) + ")"
        if self.xl_based == "true":
            ret += ".XLBased()"
        return ret

def parse_scaling(s):
    return FormScaling(
                base = parse_num(s['base'], 0, 100) if 'base' in s else 0,
                scaling = parse_num(s['scaling'], 0, 100    ) if 'scaling' in s else 0,
                xl_based = parse_bool(s['xl_based'] if 'xl_based' in s else False))

CAPABILITIES = {'default', 'enable', 'forbid'}
def parse_capability(s):
    if not isinstance(s, bool):
        raise ValueError("not a boolean value")
    if s:
        return "FC_ENABLE"
    else:
        return "FC_FORBID"

keyfns = {
    'enum': Field(parse_str, required = True),
    'equivalent_mons': Field(lambda s: "MONS_" + s.upper()),
    'short_name': Field(parse_str),
    'long_name': Field(parse_str),
    'wiz_name': Field(parse_str),

    'description': Field(parse_str),

    'skill': Field(parse_skill),
    'talisman': Field(lambda s: "TALISMAN_" + s.upper()),

    'melds': Field(parse_slots),
    'resists': Field(parse_resists),

    'str': Field(lambda s: parse_num(s, -99, 127)),
    'dex': Field(lambda s: parse_num(s, -99, 127)),
    'size': Field(parse_size),
    'hp_mod': Field(lambda s: parse_num(s, 1, 100)),
    'move_speed': Field(lambda s: parse_num(s, 1, 100)),

    'ac': Field(parse_scaling),
    'can_cast': Field(parse_bool),
    'unarmed': Field(parse_scaling),

    'unarmed_brand': Field(lambda s: "SPWPN_" + s.upper()),
    'unarmed_colour': Field(lambda s: s.upper()),
    'unarmed_name': Field(parse_str),
    'unarmed_verbs': Field(parse_attack_verbs),

    'can_fly': Field(parse_capability),
    'can_swim': Field(parse_capability),
    'changes_anatomy': Field(parse_bool),
    'changes_substance': Field(parse_bool),
    'holiness': Field(lambda s: "MH_" + s.upper()),

    'has_blood': Field(parse_capability),
    'has_hair': Field(parse_capability),
    'has_bones': Field(parse_capability),
    'has_feet': Field(parse_capability),
    'has_ears': Field(parse_capability),

    'shout_verb': Field(parse_str),
    'shout_volume': Field(lambda s: parse_num(s, -100, 100)),
    'hand_name': Field(parse_str),
    'foot_name': Field(parse_str),
    'prayer_action': Field(parse_str),
    'flesh_name': Field(parse_str),

    'fakemuts': Field(parse_muts),
    'badmuts': Field(parse_muts),

    'TAG_MAJOR_VERSION': Field(lambda s: parse_num(s, 0, 100)),
}

defaults = {
    'equivalent_mons': 'MONS_PROGRAM_BUG',
    'short_name': "",
    'long_name': "",
    'wiz_name': "",

    'description': "",

    'skill': Skill(0, 0),
    'talisman': 'NUM_TALISMANS',

    'melds': 0,
    'resists': [ResVal('MR_NO_FLAGS', 0)],

    'str': 0,
    'dex': 0,
    'size': "SIZE_CHARACTER",
    'hp_mod': 10,
    'move_speed': 10,

    'ac': FormScaling(),
    'can_cast': 'true',
    'unarmed': FormScaling(),

    'unarmed_brand': "SPWPN_NORMAL",
    'unarmed_colour': "LIGHTGREY",
    'unarmed_name': "",
    'unarmed_verbs': "DEFAULT_VERBS",

    'can_fly': "FC_DEFAULT",
    'can_swim': "FC_DEFAULT",
    'changes_anatomy': "false",
    'changes_substance': "false",
    'holiness': "MH_NONE",

    'has_blood': "FC_DEFAULT",
    'has_hair': "FC_DEFAULT",
    'has_bones': "FC_DEFAULT",
    'has_feet': "FC_DEFAULT",
    'has_ears': "FC_DEFAULT",

    'shout_verb': "",
    'shout_volume': 0,
    'hand_name': "",
    'foot_name': "",
    'prayer_action': "",
    'flesh_name': "",

    'fakemuts': "",
    'badmuts': "",
}

def load_template(templatedir, name):
    return open(os.path.join(templatedir, name)).read()

def main():
    parser = argparse.ArgumentParser(description='Generate form-data.h')
    parser.add_argument('datadir', help='dat/forms source dir')
    parser.add_argument('templatedir', help='util/form-gen template source dir')
    parser.add_argument('form_enum', help='transformation.h file path')
    parser.add_argument('form_data', help='form-data.h output file path')
    args = parser.parse_args()

    if not os.path.isdir(args.datadir):
        print('datadir isn\'t a directory')
        sys.exit(1)
    if not os.path.isdir(args.templatedir):
        print('templatedir isn\'t a directory')
        sys.exit(1)

    text = load_template(args.templatedir, 'header.txt')

    tmpl = load_template(args.templatedir, 'body.txt')

    # Determine enum order, so that form data can be output in matching order.
    # This makes a bunch of assumptions about the formating of transformation.h,
    # but those assumptions seem unlikely to change in the near future.
    enum_order = {}
    enum_started = False
    index = 0
    transformation_h = open(args.form_enum).read()
    enum_lines = transformation_h.splitlines()
    for ln in enum_lines:
        trimmed = ln.lstrip()
        if trimmed.startswith("{"):
            enum_started = True
        elif trimmed.startswith("{"):
            enum_started = False
        elif enum_started == False or trimmed.startswith("#") or trimmed.startswith("/"):
            continue
        else:
            enum_name = re.split(',|/s+', trimmed)[0]
            enum_order.update({enum_name : index})
            index += 1

    form_data = []
    for f_name in sorted(os.listdir(args.datadir)):
        if not f_name.endswith('.yaml'):
            continue
        f_path = os.path.join(args.datadir, f_name)
        try:
            form_spec = yaml.safe_load(open(f_path))
        except yaml.YAMLError as e:
            print("Failed to load %s: %s" % (f_name, e))
            sys.exit(1)
        try:
            form_data.append(Form(form_spec, enum_order))
        except (ValueError, KeyError) as e:
            print("Failed to load %s" % f_name)
            traceback.print_exc()
            sys.exit(1)

    # Now sort the data in the proper place
    sorted_form_data = [0] * len(form_data)
    for f in form_data:
        index = enum_order[f['enum']]
        sorted_form_data[index] = f

    for f in sorted_form_data:
        text += tmpl.format(**f)

    text += load_template(args.templatedir, 'footer.txt')

    with open(args.form_data, 'w') as f:
        f.write(text)

if __name__ == '__main__':
    main()
