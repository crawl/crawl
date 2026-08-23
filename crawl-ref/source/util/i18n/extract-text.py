################################################################################
#
# This script extracts strings from C++ (and Lua and .des) source code
#
################################################################################

import glob
import json
import re
import sys

msg_transforms = {
    # item-prop.cc
    "You learned that %s %s actually %s.": [
        "You learned that @item1@ is actually @item2@.",
        "You learned that @items1@ are actually @items2@"
    ],
}

# pattern for recognising strings
# handles escaped double-quotes
STRING_PATTERN = r'"(?:[^"\\]|\\.)*"'

IGNORE_STRINGS = [
    # partial strings
    'the', 'a', 'an', 'is', 'are',
    'your', 'his', 'her', 'its', 'their',
    'of', 'in', 'by', 'and', 'or', 'but',
    '%s', '%d', '%f',
    # bug
    'eggplant',
]

# These files need special handling because they define data structures
# containing strings (normally names of things)
SPECIAL_FILES = [
    'mon-data.h',
    'spl-data.h', 'zap-data.h', 'feature-data.h',
    'item-prop.cc', 'item-name.cc',
    'art-data.txt',
    'job-data.h', 'form-data.h'
]

IGNORE_FILES = [
    # art-data.h generated from art-data.txt
    'art-data.h',
    # generated from yaml files
    'species-data.h',
    # these just contain a bunch of compile flags, etc.
    'AppHdr.h', 'AppHdr.cc',
    'build.h', 'compflag.h',
    'version.h', 'version.cc',
    # json tags should not be translated
    'branch-data-json.h', 'branch-data-json.cc',
    'json.h', 'json.cc', 'json-wrapper.h',
    'tileweb-text.h', 'tileweb-text.cc',
    # nor other tags
    'colour.h', 'colour.cc', 'format.h', 'format.cc',
    # keys/properties
    'defines.h', 'dgn-layouts.h', 'dgn-layouts.cc', 'god-abil.h', 'initfile.cc',
    'libunix.h', 'libunix.cc', 'libutil.h', 'libutil.cc', 'mgen-data.h',
    'mi-enum.h', 'mon-abil.h', 'mon-clone.h', 'mon-speak.cc', 'monster.h',
    'religion-enum.h',
    # debug/test stuff
    'debug.h', 'ctest.h', 'ctest.cc', 'fake-main.cc', 'coord-def.h',
    'crash.h', 'crash.cc', 'errors.h', 'errors.cc',
    # English grammar
    'english.h', 'english.cc',
    # files related to the translation process itself
    'localise.h', 'localise.cc', 'localise-util.h', 'localise-util.cc',
    'database.h', 'database.cc', 'sqldbm.cc',
    # stuff related to morgue file is not translated
    # (because if we run this on a server, we want all the morgues in English)
    'chardump.h', 'chardump.cc', 'kills.h', 'kills.cc', 'notes.h', 'notes.cc',
    # lua scripting stuff
    'clua.h', 'clua.cc', 'cluautil.h', 'cluautil.cc', 'dlua.h', 'dlua.cc',
    # internal logic
    'domino.h', 'domino.cc', 'dungeon.h', 'dungeon.cc', 'mapdef.h', 'mapdef.cc',
    'mapmark.h', 'mapmark.cc', 'maps.h', 'maps.cc', 'mon-gear.h', 'mon-gear.cc',
    'mon-pathfind.cc', 'ng-init.cc', 'ng-input.cc', 'precision-menu.h',
    'precision-menu.cc', 'randbook.h', 'randbook.cc', 'sound.h', 'sound.cc',
    'tilepick.cc', 'viewchar.h', 'viewchar.cc',
    # error messages that probably aren't worth translating
    'fontwrapper-ft.cc', 'game-options.h', 'game-options.cc', 'glwrapper-ogl.cc',
    'libw32c.h', 'libw32c.cc', 'package.cc', 'windowmanager-sdl.cc',
    # utils
    'stringutil.cc', 'syscalls.h', 'syscalls.cc', 'ui.h', 'ui.cc',
    'unicode.h', 'unicode.cc',
    # nonsense
    'lang-fake.h', 'lang-fake.cc',
    # dump file stuff
    'dat/clua/kills.lua',
    # simple messaging - I don't think this is actually used, even though it's built into webtiles
    'dgl-message.h', 'dgl-message.cc',
]

IGNORE_SECTIONS = {
    'abyss.cc':         ['_abyss_grid'],        # dungeon generation
    'acquire.cc':       [
        '_hyphenated_letters',                  # hyphenate aquirement option letters
        '_why_reject'                           # debug messages
    ],
    'arena.cc':         ['arena_tee'],          # arena dumpfile stuff
    'art-func.h':       ['_SINGING_SWORD_melee_effects'], # internal keys
    'artefact.cc':      ['replace_name_parts'],
    'attack.cc':        ['chaos_effects'],      # chaos effect names are only used for debugging
    'beam.cc':          ['enchant_monster_invisible'], # we expand strings at the point of calling
    'command.cc':       [
        # diagnostic stuff
        'features', '_get_version_information', '_get_version_features', '_get_version_changes',
        '_add_movement_diagram',                # same for all languages
        '_get_help_section',                    # section header text that is never used
    ],
    # internal identifiers
    'delay.cc':         ['activity_interrupt_names'],
    'describe.cc':      [
        'get_command_description',              # lookup key
        'trap_name',                            # for interpreting vault spec
    ],
    'dgn-shoals.cc':    ['dgn_build_shoals_level'], # dungeon builder stuff
    'god-abil.cc':      [
        '_god_blessing_description',            # milestones
        '_gozag_shop_spec',                     # internal identifiers
    ],
    'god-conduct.cc':   ['conducts'],           # debug
    'hints.cc':         [
        '_replace_static_tags',                 # error messages for translator, not end user
        '_hints_target_mode',                   # literal commands (and % placeholder)
    ],
    'hiscores.cc':      [
        'kill_method_names',                    # scorefile stuff (never displayed)
        '_job_name', '_job_abbrev',             # obsolete jobs (backgrounds)
        '_species_name', '_species_abbrev',     # obsolete species
        '_deconstruct_shooter_phrase',          # search strings
        'scorefile_entry::terse_missile_name',  # search strings
        'scorefile_entry::set_base_xlog_fields', # internal ids
    ],
    'item-name.cc':     [
        '_random_vowel',                        # random name generation
        '_random_cons',
        '_random_consonant_set',
    ],
    'item-prop.cc':     ['item_sets'],          # internal ids
    'jobs.cc':          ['debug_jobdata'],      # debug
    'message.cc':       [
        # this function adds a prefix to the message parameter
        # this script will add it when extracting the message at the point of call
        'wu_jian_sifu_message',
    ],
    'misc.cc':          ['maybe_to_string'],    # debug
    'mon-death.cc':     [
        '_milestone_kill_verb',                 # only used for milesone
        '_killer_type_name',                    # internal keys
    ],
    'mon-ench.cc':      [
        # debug
        'enchant_names', 'mon_enchant::operator_string',
        'mon_enchant::kill_category_desc',
    ],
    'mon-info.cc':      ['_monster_list_colour_names'],     # internal identifiers
    'mon-place.cc':     ['debug_bands'],        # debug
    'mon-util.cc':      [
        'holiness_name',                        # only used in LUA
        'mons_type_name',                       # names for "random" monsters
        '_get_species_insult', 'get_mon_shape_str',  # db keys
        'debug_mondata', 'debug_monspells',   #debug
    ],
    'monster.cc':       [
        '_invalid_monster_str',                 # debugging stuff
    ],
    'output.cc':        [
        'mpr_monster_list',                     # only used in morgue and for debugging
        '_itosym',                              # symbolic representaion of resist numbers
    ],
    'xom.cc':           [
        '_note_potion_effect',                  # milestones
        '_get_death_type_keyword', 'xom_events', # internal keys
        # debug stuff
        'validate_xom_events', '_list_exploration_estimate', 'debug_xom_effects',
    ],
}

################################
# Grammatical utility functions
################################

def has_article_the(string):
    return re.match("^the ", string, flags=re.IGNORECASE)

def has_article_a(string):
    return re.match("^(a|an) ", string, flags=re.IGNORECASE)

def remove_article(string):
    return re.sub("^(a|an|the|some) ", "", string, flags=re.IGNORECASE)

def article_a(string):
    string = remove_article(string)
    if re.search('^[aeiouAEIOU]', string) and not string.startswith('one-'):
        return "an " + string
    else:
        return "a " + string

def article_the(string):
    return "the " + remove_article(string).lstrip()

def possessive(string):
    return string + "'s"

# pluralise a string - replicates the logic of the equivalent function in english.cc
def pluralise(string):
    # if it's something like "potion of healing" then we want "potions of healing", not "potion of healings"
    # so separate the suffix, pluralise the main noun, then put the suffix back
    pos = string.find(" of ")
    if pos < 0:
        pos = string.find(" from ")
    if pos < 0:
        pos = string.find(" labelled ")
    if pos >= 0:
        prefix = string[0:pos]
        suffix = string[pos:]
        return pluralise(prefix) + suffix

    if string.endswith("us"):
        if string.endswith("lotus") or string.endswith("status"):
            return string + "es"
        else:
            return string[:-2] + "i";
    elif string.endswith("ex"):
        return string[:-2] + "ices"
    elif string.endswith("mosquito") or string.endswith("ss"):
        return string + "es"
    elif string.endswith("cyclops"):
        return string[:-1] + "es"
    elif string.endswith("catoblepas"):
        return string[:-1] + "e"
    elif string.endswith("s"):
        return string
    elif string.endswith("y") and not string.endswith("ey"):
        return string[:-1] + "ies"
    elif string.endswith("staff"):
        return string[:-2] + "ves"
    elif string.endswith("f") and not string.endswith("ff"):
        return string[:-1] + "ves"
    elif string.endswith("mage") and not string.endswith("damage"):
        return string[:-1] + "i"
    elif re.search('(gold|fish|folk|spawn|tengu|sheep|swine|efreet|jiangshi|raiju|meliai)$', string):
        return string
    elif re.search('(ch|sh|x)$', string):
        return string + "es"
    elif re.search('(simulacrum|eidolon)$', string):
        return string[:-2] + "a"
    elif string.endswith("djinni"):
        return string[:-1]
    elif string.endswith("foot"):
        return string[:-4] + "feet"
    elif re.search('(ophan|cherub|seraph)$', string):
        return string + "im"
    elif string.endswith("arachi"):
        return string + "m"
    elif string.endswith("ushabti"):
        return string + "u"
    elif string.endswith("mitl"):
        return string[:-2] + "meh"
    else:
        return string + "s"

# conjugate verb for 3rd person singular (he/she/it)
def conjugate_verb(verb_phrase):
    parts = verb_phrase.split(' ', 1)
    verb = parts[0]
    suffix = ''
    i = 1
    while verb.endswith('ly') and i < len(parts):
        verb += ' ' + parts[i]
        i += 1
    while i < len(parts):
        suffix += ' ' + parts[i]
        i += 1

    if verb == "be" or verb == "are":
        return "is" + suffix
    elif verb.endswith('ss') or verb.endswith('sh') or verb.endswith('ch'):
        return verb + 'es' + suffix
    else:
        return verb + 's' + suffix

def is_text_colour(string):
    colours = ["black", "brown", "grey", "white", "blue",
               "green", "cyan", "red", "magenta", "yellow"];
    string = re.sub("^(light|dark)", "", string.strip())
    return string in colours;

def strip_formatting(string):
    # strip printf format strings
    result = string.replace("%%", "");
    result = re.sub(r'%[\-\+ #0]?[\*0-9]*(\.[\*0-9]*)?(hh|h|l|ll|j|z|t|L)?[diuoxXfFeEgGaAcspn]', '', result)
    # Hexadecimal number indicator
    result = re.sub('0x', '', result);
    return result

def is_only_formatting(string):
    temp = strip_formatting(string)
    return not re.search("[A-Za-z0-9]", temp)

def dump_lines(orig_filename, lines):
    with open(orig_filename + ".tmp", "w") as file:
        for line in lines:
            file.write(line)
            file.write("\n")

################################
# String extraction functions
################################

# remove comments and optionally whitespace
def get_preprocessed_c_lines(filename, strip_whitespace=True):
    with open(filename, 'r') as f:
        text = f.read()
    # strip multiline comments
    text = re.sub(r'/\*.*?\*/', "", text, 0, re.DOTALL)
    result = []
    lines = text.split("\n")
    for line in lines:
        # remove single-line comments
        if "//" in line:
            line = re.sub(r"\s*//.*", "", line)
            if line == "":
                continue
        line = line.rstrip()
        if strip_whitespace:
            line = line.lstrip()
            if line == "":
                continue

        lines = re.sub(r"^\s*#\s*", "#", line)
        if line.startswith("#include"):
            continue

        result.append(line)

    # concatenate lines where appropriate
    lines = result
    result = []
    for line in lines:
        lstripped = line.lstrip()
        if len(result) == 0 or lstripped.startswith("#"):
            result.append(line)
            continue

        concatenate = False
        last = result[-1]

        if last == "" and lstripped == "":
            continue
        elif last.endswith("\\"):
            last = last[:-1].rstrip() + " "
            line = lstripped
            concatenate = True
        elif last.endswith(",") and "(" in last and "-data" not in filename:
            last += " "
            line = lstripped
            concatenate = True
        elif last.endswith('"') and lstripped.startswith('"'):
            last = last[:-1]
            line = lstripped[1:]
            concatenate = True
        elif last.endswith("?") or lstripped.startswith("?") \
                or last.endswith(":") or lstripped.startswith(":"):
            last += " "
            line = lstripped
            concatenate = True

        if concatenate:
            result[-1] = last + line
        else:
            result.append(line)

    return result


# remove code surrounded by #if TAG_MAJOR_VERSION
def remove_conditionally_compiled_code(lines):
    anti_ignore = False
    ignore = False
    result = []
    for line in lines:
        if line.startswith("#ifdef"):
            if "TAG_MAJOR_VERSION" in line or "DEBUG" in line:
                ignore = True
        elif line.startswith("#ifndef"):
            if "TAG_MAJOR_VERSION" in line or "DEBUG" in line:
                ignore = False
                anti_ignore = True
        elif line.startswith("#if") or line.startswith("#elif"):
            if "defined" in line:
                if "!defined(DEBUG" in line:
                    ignore = False
                    anti_ignore = True
                elif "defined(DEBUG" in line:
                    ignore = True
                    anti_ignore = False
            elif "TAG_MAJOR_VERSION" in line:
                ignore = True
                anti_ignore = False
        elif line.startswith("#else"):
            if anti_ignore:
                ignore = True
                anti_ignore = False
            else:
                ignore = False
        elif line.startswith("#end"):
            ignore = False
            anti_ignore = False

        if not ignore or re.match("^#(if|elif|else|end)", line):
            result.append(line)
    return result

# should line be ignored?
def ignore_c_line(line):
    if '"' not in line:
        return True

    if line.startswith('#') and not line.startswith('#define '):
        return True

    if 'AXED' in line:
        return True

    # diagnotic messages
    if re.search(r"(MSGCH_DIAGNOSTIC|dprf|dprintf|debug|DEBUG|ASSERTM|log_print|dump_|fprintf)", line):
        return True
    if re.search(r"(report_error|bad_level_id|arena_error|dgn_veto_exception)", line):
        return True
    if re.search(r"\bdie\s*\(", line):
        return True

    return False

# should string be ignored?
def ignore_string(string):
    stripped = string.strip()
    if len(stripped) < 2:
        return True

    # check explicit ignore list
    if stripped.lower() in IGNORE_STRINGS:
        return True

    if is_only_formatting(stripped):
        return True

    # ignore identifiers
    if '_' in string and not ' ' in string:
        return True

    if re.search(r'\b(buggy|bugginess|BUG|BUGGY|bugger|Bugger|bugdom)\b', string):
        return True

    return False

# should section be ignored?
def ignore_section(filename, section):
    if 'milestone' in section:
        return True
    elif filename in IGNORE_SECTIONS and section in IGNORE_SECTIONS[filename]:
        return True

    return False

def keep_string(string):
    return not ignore_string(string)

def is_section_start(line):
    if line.lstrip() != line:
        return False
    if re.match(r'[#\{\}\[\]]', line):
        return False
    if not re.search(r"[a-zA-Z]+", line):
        return False
    return True

def extract_section_name(line):
    m = re.search(r"([^ ]+) *\(", line)
    if m and m[1]:
        return re.sub(r"[*&]", "", m[1])
    m = re.search(r"([^ ]+) *\=", line)
    if m and m[1]:
        return re.sub(r'[^a-zA-Z0-9_:]', '', m[0])
    return line.strip()

# process art-data.txt
def process_art_data_txt():
    infile = open('art-data.txt')
    data = infile.read()
    infile.close()

    result = []
    lines = data.splitlines()
    name = ''
    desc = None
    brand_desc = None
    has_appearance = False
    for line in lines:
        if line.startswith('#'):
            continue
        elif line.startswith(' '):
            if desc is not None:
                desc += line
            elif brand_desc is not None:
                brand_desc += line
            continue

        if desc is not None:
            result.append('# note: description for ' + name)
            result.append(desc)
            desc = '' if line.startswith('+') else None

        if brand_desc is not None:
            result.append('# note: brand description for ' + name)
            result.append(brand_desc)
            brand_desc = '' if line.startswith('+') else None

        if line.startswith('+'):
            if desc is not None:
                desc += line[1:].strip()
            elif brand_desc is not None:
                brand_desc += line[1:].strip()
        elif line.startswith('NAME:'):
            has_appearance = False
            name = line.replace('NAME:', '').strip()
            if 'DUMMY' in name:
                continue
            #result.append('# note: ' + name)
            if re.search('(boots|gloves|gauntlets|quick blades)', name):
                if not 'pair of ' in name:
                    name = 'pair of ' + name
            result.append(article_the(name))
        elif 'DUMMY' in name:
            continue
        elif line.startswith('APPEAR:'):
            string = line.replace('APPEAR:', '').strip()
            result.append('# note: appearance of ' + name + " before it's identified")
            result.append(article_the(string))
        elif line.startswith('TYPE:'):
            string = line.replace('TYPE:', '').strip()
            result.append('# note: base type of ' + name)
            result.append(article_the(string))
        elif line.startswith('INSCRIP:'):
            string = line.replace('INSCRIP:', '').strip()
            if string.endswith(','):
                string = string[0:-1]
            result.append('# note: annotation for ' + name)
            result.append(string)
        elif line.startswith('DESCRIP:'):
            desc = line.replace('DESCRIP:', '').strip()
        elif line.startswith('DBRAND:'):
            brand_desc = line.replace('DBRAND:', '').strip()

    return { "art_data" : result }

def process_yaml_file(filename):
    return {}

def process_des_or_lua_file(filename):
    return {}

def split_on_newlines(strings):
    result = []
    for string in strings:
        result.extend(string.split("\\n"))
    return result

def dummy_up_keys(line):
    line = re.sub(r"(get[A-Za-z]+String) *\(([^\)]+)\)", "$1(dummy)", line)
    return line

def extract_c_strings(line, filter_results):
    strings = re.findall(STRING_PATTERN, line)
    strings = list(map(lambda x: x.strip('"'), strings))
    strings = split_on_newlines(strings)
    if filter_results:
        strings = list(filter(keep_string, strings))
    return strings

def process_cplusplus_file(filename):
    lines = get_preprocessed_c_lines(filename, False)
    lines = remove_conditionally_compiled_code(lines)
    #if filename == "item-name.cc":
    #    dump_lines(filename, lines)

    results = {}
    section = "none"
    saved_section = None
    results[section] = []

    for line in lines:
        if section == "_fixup_runeorb_entry":
            pass
        if filename == "item-name.cc":
            for special in ["potion_qualifiers", "potion_colours"]:
                if (special + "[] =") in line:
                    saved_section = section
                    section = special
                elif special in line and saved_section != None:
                    section = saved_section
                    saved_section = None
                if section not in results:
                    results[section] = []
        if is_section_start(line):
            section = extract_section_name(line)
            if section not in results:
                results[section] = []
        if ignore_section(filename, section):
            continue
        if ignore_c_line(line):
            continue
        line = dummy_up_keys(line)
        strings = extract_c_strings(line, True)
        if len(strings) == 0:
            continue
        if "simple_god_message" in line or "simple_monster_message" in line:
            if strings[0].startswith(" ") or strings[0].startswith("'"):
                strings[0] = "@Arg@" + strings[0]
        if "attack_strength_punctuation" in line:
            strings[0] = re.sub("%s$", "@punct@", strings[0]);
        if filename == "item-name.cc":
            if section == "jewellery_effect_name" and not re.search("[A-Z]", strings[0]):
                if "RING_" in line:
                    strings[0] = "the ring of " + strings[0]
                elif "AMU_" in line:
                    strings[0] = "the amulet of " + strings[0]
        if strings:
            results[section].extend(strings)

    #print(json.dumps(results, indent=2))
    return results

def process_file(filename):
    if filename == 'art-data.txt':
        results = process_art_data_txt()
    elif filename.endswith('.yaml'):
        results = process_yaml_file(filename)
    elif filename.endswith('.lua') or filename.endswith('.des'):
        results = process_des_or_lua_file(filename)
    else:
        results = process_cplusplus_file(filename)
    return results

#################
# Post-processing
#################

def post_process_feature_data_h(input):

    altars = []
    others = []

    for section, strings in input.items():
        new_strings = []
        for string in strings:
            # ignore internal ids
            if string in ["unseen", "passage of golubria"]:
                continue
            if "altar" in string:
                altars.append(article_the(string))
            else:
                if string.startswith("some "):
                    others.append(string)
                others.append(article_the(string))

    altars.sort()
    others = list(set(others))
    others.sort()

    results = { "altars" : altars, "others": others }
    return results

# this is where most of the item names are
def post_process_item_prop_cc(input):
    results = {}

    for section, strings in input.items():
        results[section] = []
        for string in strings:
            if "%s" in string:
                results[section].append(string)
                continue
            elif string == ' dragon scales':
                continue
            elif section == 'Armour_prop':
                if string in ['gloves', 'boots']:
                    string = 'pair of ' + string
                elif "the tower shield" in results[section]:
                    # the rest after shields are dragon armours
                    string = string + ' dragon scales'
            elif section == 'Staff_prop':
                string = "staff of " + string
            elif section == 'Gem_prop':
                string += " gem"

            results[section].append(article_the(string))
            if string in ['javelin', 'boomerang']:
                results[section].append(article_the("silver " + string))

        # stackable items need a plural with count
        if section == 'Missile_prop':
            extras = []
            for string in results[section]:
                extras.append('@num@ ' + pluralise(remove_article(string)))
            results[section].extend(extras)

    return results

def post_process_item_name_cc(input):
    results = {}
    for section, old_strings in input.items():
        new_strings = []
        for string in old_strings:
            if string in ["Donald", "%s of %s", "x) ", "corpse bug", "Unnamed gizmo"]:
                continue
            elif string.endswith(" of "):
                continue
            elif string == " gem":
                string = "the" + string
            elif string == "enchanted %s":
                string = string.replace("%s", "")
            elif section == "missile_brand_name":
                if string.endswith("ed"):
                    new_strings.append("the " + string + " dart")
                    new_strings.append("@num@ " + string + " darts")
                elif string in ["dispersal", "disjunction"]:
                    new_strings.append("the dart of " + string)
                    new_strings.append("@num@ darts of " + string)
                elif string == "silver":
                    new_strings.append("the " + string + " javelin")
                    new_strings.append("@num@ " + string + " javelins")
                else:
                    new_strings.append(string)
                continue
            elif section == "weapon_brands_verbose":
                if string in ["vampirism", "antimagic", "heavy", "spectralising", "devious"]:
                    # only uses the adjective
                    continue
                else:
                    string = "@item@ of " + string
            elif section == "weapon_brands_adj":
                if string in ["vampiric", "antimagic", "heavy", "spectral", "devious"]:
                    # uses the adjective
                    string += " "
                else:
                    continue
            elif section == "special_armour_type_name":
                if not re.search("[A-Z]", string):
                    string = "@item@ of " + string
            elif section == "_wand_type_name":
                string = "the wand of " + string
            elif section == "wand_primary_string":
                string = "the " + string + " wand"
            elif section == "potion_type_name":
                new_strings.append("the potion of " + string)
                new_strings.append("@num@ potions of " + string)
                continue
            elif section == "scroll_type_name":
                new_strings.append("the scroll of " + string)
                new_strings.append("@num@ scrolls of " + string)
                continue
            elif section == "ring_primary_string":
                string = "the " + string + " ring"
            elif section == "amulet_primary_string":
                string = "the " + string + " amulet"
            elif section == "staff_primary_string":
                string = "the " + string + "staff"
            elif section == "rune_type_name":
                new_strings.append("the " + string + " rune of Zot")
                new_strings.append("the " + string + " rune")
            elif section == "misc_type_name":
                new_strings.append(article_the(string))
                if "Geryon" not in string:
                    new_strings.append(pluralise(string))
                continue
            elif section == "_book_type_name":
                if string == "Fixed Theme":
                    continue
                string = "a book of " + string
            elif section == "sub_type_string":
                if re.match("^(My|the) ", string) or ("'" in string and "Poisoner's" not in string):
                    pass
                elif re.match("^[A-Z]", string):
                    string = article_a(string)
                else:
                    string = article_the(string)
            elif section == "ghost_brand_name":
                if string == "weapon of %s":
                    continue
                else:
                    string = string.replace("%s", "the");
            elif section == "item_def::name_aux":
                if ":" in string:
                    # debugging stuff
                    continue
                elif string == "labelled ":
                    new_strings.append("the scroll labelled @label@")
                    new_strings.append("@num@ scrolls labelled @label@")
                    continue
                elif string == "gold piece":
                    new_strings.append(article_the(string))
                    new_strings.append("1 " + string)
                    new_strings.append("@num@ " + pluralise(string))
                    continue
                elif string == "flux bauble":
                    new_strings.append(article_the(string))
                    new_strings.append("@num@ " + pluralise(string))
                    continue
                elif not string.endswith(" "):
                    string = article_the(string)
            elif section == "potion_colours":
                if "potions_singular" not in results:
                    results["potions_singular"] = []
                if "potions_plural" not in results:
                    results["potions_plural"] = []
                results["potions_singular"].append("the " + string + " potion");
                results["potions_plural"].append("@num@ " + string + " potions");
                continue
            elif section == "_gem_parenthetical":
                if string == " turns":
                    string = "@num@ turns until shattered"
                elif "until shattered" in string:
                    string = "@num1@/@num2@ until shattered";
                elif "shattered" in string:
                    string = "shattered"

            new_strings.append(string)
        results[section] = new_strings

    if "potions_singular" in results:
        results["potions_singular"].sort()
    if "potions_plural" in results:
        results["potions_plural"].sort()

    return results

def is_unique_monster(string):
    # non-uniques with uppercase letters
    specials = [
        'Killer Klown', 'Orb Guardian', 'Brimstone Fiend', 'Ice Fiend',
        'Tzitzimitl', 'Hell Sentinel', 'Executioner', 'Hellbinder',
        'Cloud Mage', 'Statue of Wucad Mu'
    ]
    if string in specials:
        return False

    return re.match('^[A-Z]', string)

def post_process_mon_data_h(input):
    uniques = []
    singles = []
    plurals = []

    for section, strings in input.items():
        new_strings = []
        for string in strings:
            if string == "removed" or string.startswith('test '):
                continue
            if is_unique_monster(string):
                if " the " in string:
                    short_form = re.sub(" the .*", "", string)
                    uniques.append(article_the(short_form))
                string = article_the(string)
                if string not in uniques:
                    uniques.append(string)
            else:
                singles.append(article_the(string))
                plurals.append("@num@ " + pluralise(string))

    uniques.sort()
    singles.sort()
    plurals.sort()

    result = {
        "uniques": uniques,
        "singular": singles,
        "plural": plurals,
    }
    return result

def post_process_spl_data_h(input):
    spells = []

    for section, strings in input.items():
        for string in strings:
            if "Debugging" in string or "nonexistent" in string:
                continue
            if "serpent of hell breath" in string:
                continue
            spells.append(string)

    spells.sort()

    return { "spelldata": spells }

def post_process_zap_data_h(input):
    zaps = []

    for section, strings in input.items():
        for string in strings:
            if "debugging" in string:
                continue
            zaps.append(article_the(string))

    zaps = list(set(zaps))
    zaps.sort()

    return { "zapdata": zaps }

def post_process_generic(input):
    results = {}
    for section, old_strings in input.items():
        new_strings = []
        for string in old_strings:
            if string == "":
                continue
            if section not in ["potion_colours", "ugly_colour_names", "drac_colour_names"]:
                if is_text_colour(string):
                    continue
            # trim trailing space from prompts
            if re.search(r"\? +$", string):
                string = string.rstrip()
            new_strings.append(string)
        results[section] = new_strings
    return results

def transform_messages(input):
    results = {}
    for section, strings in input.items():
        new_strings = []
        for string in strings:
            if string in msg_transforms:
                new_strings.extend(msg_transforms[string])
            else:
                new_strings.append(string)
        results[section] = new_strings

    for section, strings in input.items():
        new_strings = []
        for string in strings:

            count = 0
            while "%s" in string:
                count += 1
                string = string.replace("%s", "@arg" + str(count) + "@", 1)
            if count == 1:
                string = string.replace("@arg1@", "@arg@", 1)
            string = re.sub("^@arg", "@Arg", string)

            count = 0
            while "%d" in string:
                count += 1
                string = string.replace("%d", "@num" + str(count) + "@", 1)
            if count == 1:
                string = string.replace("@num1@", "@num@", 1)

            new_strings.append(string)
        results[section] = new_strings

    return results

specific_post_processing_funcs = {
    'feature-data.h': post_process_feature_data_h,
    'item-prop.cc': post_process_item_prop_cc,
    'item-name.cc': post_process_item_name_cc,
    'mon-data.h': post_process_mon_data_h,
    'spl-data.h': post_process_spl_data_h,
    'zap-data.h': post_process_zap_data_h,
}

def post_process(filename, results):
    results = post_process_generic(results)

    # the strings in some files need special handling
    if filename in specific_post_processing_funcs:
        func = specific_post_processing_funcs[filename]
        results = func(results)

    results = transform_messages(results)

    return results

#################
# Main
#################

files = []
if len(sys.argv) > 1:
    # use list of files specified on command line
    files = sys.argv[1:]
else:
    # build my own list of files

    source_files = glob.glob("*.h")
    source_files.extend(glob.glob("*.cc"))

    # sort source files with .h files before matching .cc files
    for i in range(len(source_files)):
        source_files[i] = source_files[i].replace('.h', '.a')
    source_files.sort()
    for i in range(len(source_files)):
        source_files[i] = source_files[i].replace('.a', '.h')

    yaml_files = glob.glob("dat/species/*.yaml")
    yaml_files.sort()

    lua_files = glob.glob("dat/clua/*.lua")
    lua_files.append("dat/dlua/lm_timed.lua")
    lua_files.append("dat/dlua/lm_tmsg.lua")
    lua_files.append("dat/dlua/lm_trove.lua")
    lua_files.sort()
    source_files.extend(lua_files)

    des_files = glob.glob("dat/des/*/*.des")
    des_files.sort()
    source_files.extend(des_files)

    # put some important files first
    # (because if there are duplicate strings, we want them put under these files)
    files = SPECIAL_FILES.copy()
    files.extend(yaml_files)

    # add wanted source files to list to be processed
    for fname in source_files:
        if fname not in files and \
           fname not in IGNORE_FILES and \
           not re.match('l-', fname) and \
           not re.match('dbg-', fname):
            files.append(fname)

results = {}

for filename in files:
    temp = process_file(filename)
    temp = post_process(filename, temp)
    results[filename] = temp


output = []
for filename, sections in results.items():
    output.append("##################")
    output.append("# " + filename)
    output.append("##################")
    for section_name, strings in sections.items():
        if not strings:
            continue
        output.append("")
        output.append("# section: " + section_name)
        for string in strings:
            if string != string.strip():
                string = '"' + string + '"'
            if string in output:
                string = "#duplicate: " + string
            output.append(string)
    output.append("")

for string in output:
    print(string)
