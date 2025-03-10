#!/usr/bin/env python3
"""Utility functions for python scripts converting yaml to C++ headers."""

# from __future__ import print_function

# import argparse
import os
import sys
import traceback
import collections
import re

# import yaml  # pip install pyyaml

def quote_or_nullptr(key, d):
    if key in d:
        return quote(d[key])
    else:
        return 'nullptr'


def validate_string(val, name, pattern):
    '''
    Validate a string.

    Note that re.match anchors to the start of the string, so you don't need to
    prefix the pattern with '^'. But it doesn't require matching to the end, so
    you'll probably want to suffix '$'.
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
    '''Validate an integer and check it is within the interval [min, max].'''
    if not isinstance(val, int):
        raise ValueError('%s isn\'t an integer' % name)
    if not min <= val <= max:
        raise ValueError('%s isn\'t between %s and %s' % (name, min, max))
    return val


def enumify(name, enum):
    return "{enum}_{name}".format(enum=enum, name=name).replace(' ', '_').upper()


def quote(s):
    if not isinstance(s, str):
        raise ValueError('Expected a string but got %s' % repr(s))
    return '"%s"' % s


def empty_set(typ):
    return "    set<%s>()" % typ


global LIST_TEMPLATE
LIST_TEMPLATE = """    {{ {list} }}"""
global SPELL_LIST_TEMPLATE
SPELL_LIST_TEMPLATE = """    {{\n{list}\n    }}"""

def make_list(list_str, is_spell_list = False):
    global LIST_TEMPLATE
    #TODO: add linebreaks + indents to obey 80 chars?
    if len(list_str.strip()) == 0:
        return "    {}"
    elif is_spell_list:
        return SPELL_LIST_TEMPLATE.format(list=list_str)
    else:
        return LIST_TEMPLATE.format(list=list_str)


def load_template(templatedir, name):
    return open(os.path.join(templatedir, name)).read()
