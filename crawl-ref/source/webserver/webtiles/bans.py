import itertools
import os, os.path
import yaml

# code for disallowing usernames.


# not an exhaustive leet compendium, but tuned in particular to some common
# slurs. It won't get them all. If someone starts iterating on this, I
# recommend custom code in config.py. Note that punctuation is not generally
# allowed, which removes some headaches here.
leet = {'1': 'i',
        '2': 'r', # also z...
        '3': 'e',
        '4': 'a',
        '5': 's',
        '6': 'g',
        '7': 't',
        '8': 'b',
        '9': 'g',
        '0': 'o',
        }

# return a canonical version of a nickname, doing some deleeting. Useful for
# nickname checking against extreme slurs.
def deleet(s):
    global leet
    # is there a more elegant re version of this?
    c = s.lower()
    for k in leet:
        c = c.replace(k, leet[k])
    return c


# not currently used, but this could be added to as an override list to avoid
# scunthorpe problems
nick_check_override = set()

def nick_check(s, bad_name, leet=False, repeats=False, part=False):
    global nick_check_override
    if bad_name in nick_check_override:
        return False

    if leet:
        s = deleet(s)
    if repeats:
        # remove all repeats from both strings
        s = ''.join(l[0] for l in itertools.groupby(s))
        bad_name = ''.join(l[0] for l in itertools.groupby(bad_name))
    if part:
        # recommended with care: lots of names that you'd want to ban match
        # against parts of many words. (aka
        # https://en.wikipedia.org/wiki/Scunthorpe_problem).
        # But, certain ones such as the n-word are basically safe to do this
        # with, imo. To some extent this issue applies to the other options
        # to this function as well.
        return bad_name.lower() in s.lower()
    else:
        return bad_name.lower() == s.lower()


def do_ban_check(nick, ban_list):
    if not ban_list:
        return False
    for ban_list_element in ban_list:
        # expects ban_list_element to be either:
        # * A str, in which case, we simply check against that nick, or
        # * A dict, containing a mapping 'nicks' to a list of nicks, and
        # optionally an `options` dict that uses `util.nick_check` named
        # arguments to determine how to do the check.
        if not ban_list_element:
            continue
        if isinstance(ban_list_element, str):
            if nick_check(nick, ban_list_element):
                return True
            continue
        ban_list = ban_list_element.get('names', [])
        if not ban_list:
            continue
        for bad in ban_list:
            if nick_check(nick, bad, *ban_list_element.get('options', {})):
                return True
    return False


def validate(ban_list):
    try:
        iter(ban_list)
    except TypeError:
        raise ValueError("Malformed ban list: expected iterable (got '%s')"
                                                        % repr(ban_list))

    try:
        # simplest is just to run the ban check against a name that isn't
        # valid and so shouldn't ever be banned, which exercises the whole
        # thing. In fact, we also throw an error if '' is banned.
        do_ban_check('', ban_list)
    except:
        # try to get a more specific error
        for b in ban_list:
            match = False
            try:
                match = do_ban_check('', [b])
            except:
                raise ValueError("Malformed ban list (element '%s')" % repr(b))
            if match:
                raise ValueError("Ban list matches the empty string!")


# Load a ban list in txt format. This is a list of nicknames, each on their
# own line, that will be matched exactly (non-case-sensitive).
def load_from_txt(path):
    # caller should check path if they are expecting it to definitely exist
    collect = []
    if not os.path.isfile(path):
        return collect
    with open(path) as f:
        for l in f.readlines():
            l = l.strip()
            # allow comments on their own lines only (per sequell), skip blank
            # lines.
            if not l or l[0] == '#':
                continue
            collect.append(l)
    validate(collect)
    return collect


# load a ban list in yaml format. This must be a dict with a single mapping,
# 'banned', to a list. That list consists of either strings (names to ban
# with exact non-case-sensitive matching), or dicts. The dict format is:
#   names: a list of names to check using options:
#   options: can set any of `leet`, `repeats`, or `part to do inexact matching.
#            see `nick_check` for details. As noted there, be wary of
#            overmatching.
# All name matching is non-case-sensitive.
def load_from_yaml(path):
    # caller should check path if they are expecting it to definitely exist
    collect = []
    if not os.path.isfile(path):
        return collect
    with open(path) as f:
        override_data = yaml.safe_load(f)
        # we are expecting a map with a single key, `banned`
        if not isinstance(override_data, dict):
            raise ValueError("Banfile error: '%s' must be a map" % path)
        for key, value in override_data.items():
            # XX could add an 'allowed' list here?
            if key == 'banned':
                if not isinstance(value, list):
                    raise ValueError("Banfile error: '%s' must be a list" % key)
                collect.extend(value)
            else:
                raise ValueError("Banfile error: extraneous key ('%s')" % key)
    validate(collect)
    return collect


def load_bans(path):
    if path.lower().endswith('yml') or path.lower().endswith('yaml'):
        return load_from_yaml(path)
    else:
        return load_from_txt(path)
