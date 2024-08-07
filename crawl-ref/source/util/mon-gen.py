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

class Field:
    def __init__(self, f, required = False):
        self.f = f
        self.required = required

ENERGIES = ['walk', 'swim', 'attack', 'missile', 'spell']

class Monster(MutableMapping):
    """Parser for YAML definition files.

    If any YAML content is invalid, the relevant parser function below should
    raise ValueError.
    """

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

    def desc_energy(self):
        if 'energy' not in self:
            return 'DEFAULT_ENERGY'
        custom_vals = self['energy']
        return "{%s}" % ', '.join(str(custom_vals.get(e, 10)) for e in ENERGIES)

    def unique_corpse(self): # sorry
        return self["has_corpse"] == 'true' and self["species"] == self["enum"] and 'M_CANT_SPAWN' not in self["flags"]

    def base_tile(self):
        if "tile" not in self:
            return "tilep_" + self["enum"]
        tile = self["tile"]
        if tile.startswith("tile"):
            return tile
        return "tilep_mons_" + tile

    def corpse_tile(self):
        if "corpse_tile" in self:
            return "TILE_CORPSE_" + self["corpse_tile"].upper()
        if not self.unique_corpse():
            return "TILE_ERROR"
        if "tile" in self and self["tile"] != "program_bug":
            return "TILE_CORPSE_" + self["tile"].upper()
        return "TILE_CORPSE_" + self["enum"][len("MONS_"):]

    def fill_tile_desc(self):
        desc = self.base_tile().upper()
        if "tile_variance" in self:
            desc += ", " + self["tile_variance"]
        self["tile_desc"] = desc
        self["tile_corpse"] = self.corpse_tile()

    def from_yaml(self, s):
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

        for key in defaults:
            if key not in self:
                self[key] = defaults[key]

        if 'enum' not in self:
            self['enum'] = "MONS_" + self['name'].upper().replace(" ", "_")
        if 'species' not in self:
            self['species'] = self['enum']
        if 'genus' not in self:
            self['genus'] = self['species']

        if 'will' not in self:
            if 'will_per_hd' not in self:
                raise ValueError("set exactly one of 'will' and 'will_per_hd'")
            # Yuck, we still use negative will for this?
            self['will'] = -self['will_per_hd']
        elif 'will_per_hd' in self:
            raise ValueError("set exactly one of 'will' and 'will_per_hd'")

        self["joined_flags"] = ' | '.join(self["flags"])
        # Could try to concatenate together resists at the same level, but why?
        self["joined_resists"] = ' | '.join(r.format() for r in self["resists"])
        self["joined_holiness"] = ' | '.join(h for h in self["holiness"])
        self["joined_attacks"] = ', '.join(a.format() for a in self["attacks"])
        self["corpse"] = "true" if "corpse" in self and self["corpse"] else "false" # ew
        self["energy_desc"] = self.desc_energy()
        self.fill_tile_desc()

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

def parse_num(s, min, max):
    if not isinstance(s, int):
        raise ValueError("isn't an int")
    if not min <= s <= max:
        raise ValueError("isn't between %s and %s" % (min, max))
    return s

class Glyph:
    def __init__(self, char, colour):
        self.char = char;
        self.colour = colour;

def parse_glyph(s):
    char, colour = get_fields(s, ["char", "colour"])
    return Glyph(parse_glyph_char(s["char"]), parse_str(s["colour"]).upper())

def parse_glyph_char(s):
    if not isinstance(s, str):
        raise ValueError("glyph isn't a string")
    if len(s) != 1:
        raise ValueError("glyph must be exactly one character")
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
    'acid': Resist(0, 4),
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

def parse_will(s):
    if s == 'invuln':
        return "WILL_INVULN"
    return parse_num(s, 0, 200)

class Attack:
    def __init__(self, type, damage, flavour):
        self.type = type
        self.damage = damage
        self.flavour = flavour

    def format(self):
        return "{%s, %s, %d}" % (self.type, self.flavour, self.damage)

def parse_attacks(s):
    atks = []
    if not s:
        return atks

    for a in s:
        if 'type' not in a:
            raise ValueError("missing type for attack '%s'", a)
        if 'damage' not in a:
            raise ValueError("missing damage for attack '%s'", a)
        for field in a:
            if field not in {'type', 'damage', 'flavour'}:
                raise ValueError("unknown attack field '%s'", field)
        atks.append(Attack(
            type = "AT_" + a['type'].upper(),
            damage = parse_num(a['damage'], 0, 100),
            flavour = "AF_" + a['flavour'].upper() if 'flavour' in a else 'AF_PLAIN',
        ))
    return atks

def parse_enum(s, prefix, vals):
    if s not in vals:
        raise ValueError("%s not in %s" % (s, vals))
    return prefix + s.upper()

INTELLIGENCES = {'brainless', 'animal', 'human'}
def parse_intelligence(s):
    return parse_enum(s, "I_", INTELLIGENCES)

USE_TYPES = {'nothing', 'open_doors', 'starting_equipment', 'weapons_armour'}
def parse_use(s):
    return parse_enum(s, "MONUSE_", USE_TYPES)

SIZES = {'tiny', 'little', 'small', 'medium', 'large', 'giant'}
def parse_size(s):
    return parse_enum(s, "SIZE_", SIZES)

SHAPES = {
    'humanoid',
    'humanoid_winged',
    'humanoid_tailed',
    'humanoid_winged_tailed',
    'centaur',
    'naga',
    'quadruped',
    'quadruped_tailless',
    'quadruped_winged',
    'bat',
    'bird',
    'snake',
    'fish',
    'insect',
    'insect_winged',
    'arachnid',
    'centipede',
    'snail',
    'plant',
    'fungus',
    'orb',
    'blob',
    'misc',
}
def parse_shape(s):
    return parse_enum(s, "MON_SHAPE_", SHAPES)

def parse_energy(s):
    if 'move' in s:
        if 'walk' in s or 'swim' in s:
            raise ValueError("'move' is redundant with 'walk'/'swim'")
        s['walk'] = s['move']
        s['swim'] = s['move']
        del s['move']
    for k in s:
        if k not in ENERGIES:
            raise ValueError("unknown energy type '%s'", k)
        parse_num(s[k], 1, 100)
    return s

HOLINESSES = {'holy', 'natural', 'undead', 'demonic', 'nonliving', 'plant'}
def parse_holiness(s):
    return [parse_enum(h, "MH_", HOLINESSES) for h in s]

HABITATS = {'land', 'amphibious', 'water', 'lava', 'amphibious_lava'}
def parse_habitat(s):
    return parse_enum(s, "HT_", HABITATS)

TVARIES = {'mod', 'cycle', 'random', 'water'}
def parse_tile_variants(s):
    return parse_enum(s, "TVARY_", TVARIES)

keyfns = {
    'glyph': Field(parse_glyph, required = True),
    'name': Field(parse_str, required = True),
    'enum': Field(lambda s: "MONS_" + s.upper()),

    'flags': Field(parse_flags),

    'resists': Field(parse_resists),

    'xp_mult': Field(lambda s: parse_num(s, 0, 999)),
    'species': Field(lambda s: "MONS_" + s.upper()),
    'genus': Field(lambda s: "MONS_" + s.upper()),
    'holiness': Field(parse_holiness),
    'will': Field(parse_will),
    'will_per_hd': Field(lambda s: parse_num(s, 1, 27)),

    'attacks': Field(parse_attacks, required = True),

    'hd': Field(lambda s: parse_num(s, 0, 9999), required = True),
    'hp_10x': Field(lambda s: parse_num(s, 0, 100000000), required = True),

    'ac': Field(lambda s: parse_num(s, -99, 127), required = True),
    'ev': Field(lambda s: parse_num(s, -99, 127), required = True),
    'spells': Field(lambda s: "MST_" + s.upper()),
    'has_corpse': Field(parse_bool),
    'shout': Field(lambda s: "S_" + s.upper()),

    'intelligence': Field(parse_intelligence, required = True),
    'habitat': Field(parse_habitat),
    'speed': Field(lambda s: parse_num(s, 0, 81)),
    'energy': Field(parse_energy),

    'uses': Field(parse_use),
    'size': Field(parse_size, required = True),
    'shape': Field(parse_shape, required = True),
    'god': Field(lambda s: "GOD_" + s.upper().replace(" ", "_")),

    'tile': Field(parse_str),
    'tile_variance': Field(parse_tile_variants),
    'corpse_tile': Field(parse_str),
}

defaults = {
    'flags': ['M_NO_FLAGS'],
    'resists': [ResVal('MR_NO_FLAGS', 0)],
    'xp_mult': 10,
    'holiness': ['MH_NATURAL'],
    'spells': 'MST_NO_SPELLS',
    'shout': 'S_SILENT',
    'habitat': 'HT_LAND',
    'speed': 10,
    'uses': 'MONUSE_NOTHING',
    'has_corpse': 'false',
    'god': 'GOD_NO_GOD',
}

def load_template(templatedir, name):
    return open(os.path.join(templatedir, name)).read()

def main():
    parser = argparse.ArgumentParser(description='Generate mon-data.h')
    parser.add_argument('datadir', help='dat/mons source dir')
    parser.add_argument('templatedir', help='util/mon-gen template source dir')
    parser.add_argument('mon_data', help='mon-data.h output file path')
    args = parser.parse_args()

    if not os.path.isdir(args.datadir):
        print('datadir isn\'t a directory')
        sys.exit(1)
    if not os.path.isdir(args.templatedir):
        print('templatedir isn\'t a directory')
        sys.exit(1)

    text = load_template(args.templatedir, 'header.txt')

    tmpl = load_template(args.templatedir, 'body.txt')
    for f_name in sorted(os.listdir(args.datadir)):
        if not f_name.endswith('.yaml'):
            continue
        f_path = os.path.join(args.datadir, f_name)
        try:
            mon_spec = yaml.safe_load(open(f_path))
        except yaml.YAMLError as e:
            print("Failed to load %s: %s" % (f_name, e))
            sys.exit(1)
        try:
            mons = Monster(mon_spec)
        except (ValueError, KeyError) as e:
            print("Failed to load %s" % f_name)
            traceback.print_exc()
            sys.exit(1)

        text += tmpl.format(**mons)

    text += load_template(args.templatedir, 'footer.txt')

    with open(args.mon_data, 'w') as f:
        f.write(text)

if __name__ == '__main__':
    main()
