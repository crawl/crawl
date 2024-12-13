################################################################################
#
# This script extracts strings from C++ (and Lua and .des) source code
#
# By default, all strings are extracted, unless there's a reason to ignore them.
# However for files named in LAZY_FILES, strings are only extracted if
# there's an explicit reason to do so.
#
# It understands certain directives placed in single-line comments:
#   @locnote: blah = include a comment for the translators with the extracted strings
#   @noloc = do not extract strings on this line
#   @noloc section start = stop extracting strings from this line onwards
#   @noloc section end = resume extracting strings
#   @localise = DO extract strings on this line, even if in a noloc section or lazy
#               file (not required if there's a call to the localise() function)
#
# These strings are always ignored:
#   - map keys (but not values)
#   - anything that looks like a key or identifier
#   - tags of the form "<foo>" (on their own)
#   - file names
#
################################################################################

import glob
import re
import sys

# pattern for recognising strings
# handles escaped double-quotes
STRING_PATTERN = r'"(\\\\|\\"|[^"])*"'

# strings to ignore
IGNORE_STRINGS = [
    'the', 'the ', ' the ',
    'a', 'a ', 'an', 'an ',
    'you', 'you ', 'your', 'your ', 'its ',
    ' of ', ' of', 'of ', 's',
    'The ', 'Your ',
    'debugging ray', 'debug', 'bugger',
    'bug', 'null', 'invalid', 'DEAD MONSTER',
    'true', 'false', 'veto',
    'You hear the sound of one hand!',
    # suffixes for walking verb
    'ing', 'er',
    # property keys
    'Brand', 'BAcc', 'BDam', 'nupgr', 'cap-',
    # text colour tags
    'lightgrey', 'darkgrey', 'lightgreen', 'darkgreen', 'lightcyan', 'darkcyan',
    'lightred', 'darkred', 'lightmagenta', 'darkmagenta', 'lightyellow', 'darkyellow'
]

# These files need special handling because they define data structures
# containing strings (normally names of things)
# (stringutil.h is an exception - it contains strings related to list building)
SPECIAL_FILES = [
    'stringutil.h', 'mon-data.h',
    'spl-data.h', 'zap-data.h', 'feature-data.h',
    'item-prop.cc', 'item-name.cc',
    'art-data.txt', # art-data.h generated from art-data.txt
    'job-data.h', 'form-data.h'
]

# These files are evaluated differently. We ignore all strings unless we have a reason to extract them,
# as opposed to extracting all strings unless we have a reason to ignore them.
LAZY_FILES = [
    'dgn-overview.cc', 'delay.h', 'end.cc', 'files.cc','fineff.cc',
    'god-passive.cc', 'god-prayer.cc', 'macro.cc', 'main.cc', 'tilereg-dgn.cc'
]

SKIP_FILES = [
    # art-data.h generated from art-data.txt
    'art-data.h',
    # generated from yaml files
    'species-data.h',
    # covered in a way that doesn't use the literal strings from the file
    'mutant-beast.h',
    # these just contain a bunch of compile flags, etc.
    'AppHdr.h', 'AppHdr.cc',
    'build.h', 'compflag.h',
    'version.h', 'version.cc',
    # json tags should not be translated
    'branch-data-json.h', 'branch-data-json.cc',
    'json.h', 'json.cc', 'json-wrapper.h',
    'tileweb.h', 'tileweb.cc', 'tileweb-text.h', 'tileweb-text.cc',
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
    'xlate.h', 'xlate.cc',
    'localise.h', 'localise.cc',
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

def replace_last(s, old, new):
    return new.join(s.rsplit(old, 1))

def conjugate_verb(verb):
    if verb == "be" or verb == "are":
        return "is"
    elif verb.endswith('ss') or verb.endswith('sh'):
        return verb + 'es'
    else:
        return verb + 's'

def do_any_2_actors_message(verb, suffix):
    verb3p = conjugate_verb(verb)
    strings = []
    strings.append("You " + verb + " %s" + suffix)
    strings.append("%s " + verb3p + " you" + suffix)
    strings.append("%s " + verb3p + " %s" + suffix)
    strings.append("%s " + verb3p + " itself" + suffix)
    return strings

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
            result.append('the ' + name)
        elif 'DUMMY' in name:
            continue
        elif line.startswith('APPEAR:'):
            string = line.replace('APPEAR:', '').strip()
            result.append('# note: appearance of ' + name + " before it's identified")
            result.append(article_a(string))
        elif line.startswith('TYPE:'):
            string = line.replace('TYPE:', '').strip()
            result.append('# note: base type of ' + name)
            result.append(article_a(string))
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

    return result

def process_yaml_file(filename):
    MAIN_KEYS = ["name", "short_name", "adjective", "genus", "walking_verb", "altar_action"]


    infile = open(filename)
    data = infile.read()
    infile.close()

    species = {}
    species["mutations"] = []
    lines = data.splitlines()
    for line in lines:
        line = line.strip()
        if line.startswith('#') or ":" not in line:
            continue

        tokens = line.split(':')
        if len(tokens) != 2:
            continue

        key = tokens[0].replace('-', '').strip()
        if key in MAIN_KEYS:
            value = tokens[1].strip().replace('"', '')
            species[key] = value

            # by default short_name is first 2 chars of name
            if key == "name" and "short_name" not in species:
                species["short_name"] = value[0:2]

        elif key in ["long", "short"]:
            value = tokens[1].strip().replace('"', '')
            species["mutations"].append(value)

    result = []
    for key in MAIN_KEYS:
        if key in species:
            if key == "walking_verb":
                result.append(species["walking_verb"] + "ing")
                result.append(species["walking_verb"] + "er")
            else:
                result.append(species[key])
    result.extend(species["mutations"])

    return result

# strip (potentially) multi-line comments (i.e. /*...*/)
def strip_multiline_comments(data):
    result = ""
    escaped = False
    in_string = False
    in_char = False
    in_multiline_comment = False
    in_line_comment = False
    prev = '\0'
    length = len(data)
    for i in range(length):
        ch = data[i]

        if ch == '\\' and not escaped:
            escaped = True
        else:
            escaped = False

        if in_string:
            in_string = (ch != '"' or escaped)
        elif in_char:
            in_char = (ch != "'" or escaped)
        elif in_multiline_comment:
            in_multiline_comment = (ch != "/" or prev != '*')
            prev = ch
            continue
        elif in_line_comment:
            in_line_comment = (ch != "\n" and ch != "\r")
        elif ch == '"':
            in_string = True
        elif ch == "'":
            in_char = True
        elif ch == '/' and i+1 < length:
            if data[i+1] == "*":
                in_multiline_comment = True
            elif data[i+1] == "/":
                in_line_comment = True

        if not in_multiline_comment:
            result += ch
        prev = ch

    return result

# strip single-line comments (i.e. //...)
def strip_line_comment(line):
    escaped = False
    in_string = False
    for i in range(len(line)):
        ch = line[i]

        if ch == '\\' and not escaped:
            escaped = True
        else:
            escaped = False

        if ch == '"' and not escaped:
            in_string = not in_string
        elif ch == '/' and not in_string:
            if i > 0 and line[i-1] == '/':
                # comment
                return line[0:i-1].rstrip()
    # no comment - return whole line
    return line

# start of conditionally compiled section that can be skipped (debug or obsolete code)?
def is_skippable_if(line):
    return re.search(r'^\s*#\s*ifdef .*(DEBUG|VERBOSE)', line) or \
       re.search(r'^\s*#\s*if +defined *\(DEBUG', line) or \
       re.search(r'^\s*#\s*if\s*TAG_MAJOR_VERSION\s*==\s*34', line)

# strip out stuff that is excluded by #ifdef's, etc.
def strip_uncompiled(lines):
    skip = False
    result = []
    ifs = []
    skips = []

    for line in lines:

        if re.search(r'^\s*#', line):
            if re.search(r'^\s*#\s*if', line):
                ifs.append(line)
                # skip parts that are only included in DEBUG build
                if is_skippable_if(line):
                    skip = True
                    skips.append(skip)
                    continue
                else:
                    skips.append(skip)
            elif re.search(r'^\s*#\s*else', line):
                if skips[-1] == True:
                    if len(skips) < 2 or skips[-2] == False:
                        skip = False
                        skips[-1] = False
                elif 'DEBUG' in ifs[-1]:
                    # the if block is the non-debug part, so the else block is the debug part
                    skip = True
                    skips[-1] = True
                if is_skippable_if(ifs[-1]):
                    continue
            elif re.search(r'^\s*#\s*endif', line):
                if_line = ifs.pop()
                skips.pop()
                if len(skips) == 0:
                    skip = False
                else:
                    skip = skips[-1]
                if is_skippable_if(if_line):
                    continue

        if not skip:
            result.append(line)
            #sys.stderr.write(line + "\n")

    return result

def dump_lines(filename, lines):
    with open(filename, 'w') as f:
        for line in lines:
            f.write(f"{line}\n")

# get file contents as list of lines
# uncompiled sections are stripped out
# comments are stripped out, apart from those containing directives for this script
def get_cleaned_file_contents(filename):
    infile = open(filename)
    data = infile.read()
    infile.close()

    data = strip_multiline_comments(data)
    lines = strip_uncompiled(data.splitlines())

    result = []
    for line in lines:

        # strip single-line comments, apart from those that have directives for this script
        if '//' in line:
            if not re.search(r'//.*(locnote|noloc)', line) and not re.search(r'//[ @]*localise\b', line):
                line = strip_line_comment(line)
                if line == '':
                    continue

        result.append(line)

    return result


# replace strings that should not be extracted with dummies
def do_dummy_string_replacements(lines):
    result = []
    in_map = False
    for line in lines:
        if '//' in line and re.search('//.*noloc(?! *section)', line):
            # line marked as not to be localised - replace strings with dummies
            line = re.sub(STRING_PATTERN, '"_dummy_"', line);
            # noloc comment no longer needed, and might be in the way
            line = strip_line_comment(line)

        # ignore keys (but not values) in map initialisation
        if re.search(r'map<string,[^>]+> +[A-Za-z0-9_]+\s+=', line):
            in_map = True
        if in_map:
            # surround with @'s so it looks like a param name (later code will skip it)
            line = re.sub(r'\{\s*"([^"]+)"\s*,', r'{"@\1@",', line)
            #sys.stderr.write("NERFED MAP KEY: " + line + "\n")
        # NOTE: map end could be same line as map start
        if re.search(r'}\s*;', line):
            in_map = False

        result.append(line)

    return result


# convert a string containing a parameter into a list of strings for every
# possible value of the param
def expand_param(string, param, values):
    if param not in string:
        return [string]

    result = []
    for value in values:
        result.append(string.replace(param, value))

    return result


# First-stage line processing:
#   replace strings that should not be extracted with dummies (outside noloc sections)
#   join statements that are split over multiple lines
#   join consecutive strings (in C++, "foo" "bar" is the same as "foobar")
#   strip trailing whitespace
#   strip out blank lines
def do_first_stage_line_processing(lines):
    result = []

    # this must be done before joining multi-line statements,
    # because it's possible for only part of the statement to be marked with @noloc
    lines = do_dummy_string_replacements(lines)

    for line in lines:

        line = line.rstrip()

        # skip blank lines (might get in the way of statement-joining)
        if line == '':
            continue

        if len(result) > 0:
            last = result[-1]
            if not (last.endswith(';') or last.endswith('}') or last.endswith('{')):
                # check for statements split over multiple lines
                curr = line.lstrip()

                # join strings distributed over several lines
                if last.endswith('"') and curr.startswith('"'):
                    result[-1] = last[0:-1] + curr[1:]
                    continue

                join = False
                if '(' in last and last.count('(') > last.count(')'):
                    # join function calls split over multiple lines
                    join = True
                elif last.endswith('?') or curr.startswith('?') or last.endswith(':') or curr.startswith(':'):
                    # join ternary operator split over multiple lines
                    if last.endswith(':') and (re.search('\bcase\b', last) or re.search(r'(public|protected|private|default)\s*:$', last)):
                        # false positive
                        pass
                    else:
                        join = True
                #elif last.endswith('&&') or curr.startswith('&&') or last.endswith('||') or curr.startswith('||'):
                #    join = True
                elif last.endswith('=') and not curr.startswith('{'):
                    # assignment
                    join = True
                elif curr.startswith('='):
                    # assignment
                    join = True

                if join:
                    result[-1] = last + ' ' + curr
                    continue

        # join consecutive strings on same line (this is what the C++ compiler will do)
        if '"' in line:
            line = re.sub(r'(?!\\)"\s+"', '', line)

        result.append(line)

    return result


# insert section markers
# inserts a comment like "// @locsestion: foo"
# recognised sections are classes, functions, and static array initialisations
def insert_section_markers(filename, lines):
    result = []
    section = None
    last_section = None
    for line in lines:
        if '(' in line and not line.endswith(';') and re.search('^[a-zA-Z]', line):
            # function/method
            section = re.sub('^.*[ *]', '', re.sub(' *\(.*', '', line))
        elif line.startswith('class '):
            # class
            section = re.sub('[ :].*', '', re.sub('^class *', '', line))
        elif line.startswith('static ') and re.search('\[\] *=', line):
            # static data
            section = re.sub('^.*[ *]', '', re.sub('\[\] *=.*', '', line))

        # Ewwwwww!
        if filename == 'item-name.cc':
            if section in ['armour_ego_name', 'jewellery_effect_name'] and 'else' in line:
                section += '(terse)'
            elif section == 'item_def::name_aux' and 'potion_colours[]' in line:
                section = 'potion_colours'
            elif section == 'potion_colours' and 'COMPILE_CHECK' in line:
                section = 'item_def::name_aux'

        if section != last_section:
            if len(result) > 0 and '@locnote' in result[-1]:
                # make sure note is within the section
                result.insert(-1, '// @locsection: ' + section)
            else:
                result.append('// @locsection: ' + section)
            last_section = section

        result.append(line)

    return result

# return only lines that have
#   a) strings that might need to be extracted
#   b) directives for this script
# any leading or trailing whitespace is removed
def get_relevant_lines(filename, lines):
    result = []
    ignoring = False
    section = ''
    for line in lines:
        # ignore sections explicitly marked as not to be extracted
        if 'noloc section' in line:
            if 'noloc section start' in line:
                ignoring = True
            if 'noloc section end' in line:
                ignoring = False
            continue

        if ignoring and not re.search(r'//[ @]*(localise|locsection)\b', line):
            continue

        if '@locsection' in line:
            section = re.sub('.*@locsection: *', '', line);

        if filename == 'acquire.cc':
            # ignore - debug messages
            if section == '_why_reject':
                continue
        elif filename == 'artefact.cc':
            if section in ['replace_name_parts']:
                continue
        elif filename == 'delay.cc':
            # ignore - internal identifiers
            if section == 'activity_interrupt_names':
                continue
        elif filename == 'lookup-help.cc':
            # ignore - db keys
            if re.match(r'^_(get|recap)[a-z_]*keys?$', section):
                continue

        if '//' in line:
            result.append(line.strip())
            continue

        if not '"' in line:
            continue

        # calls to mpr_nolocalise(), etc.
        if '_nolocalise' in line and not 'you.hand_act' in line:
            continue

        if filename == 'job-data.h':
            # special handling - only take the line with the job abbreviation and name
            if not re.search(r'"[A-Z][a-zA-Z]"', line):
                continue

        line = line.strip()

        # ignore pre-compiler directives, apart from #define
        if line.startswith('#') and not re.match(r'#\s*define', line):
            continue

        # ignore extern "C"
        if line.startswith('extern'):
            continue

        result.append(line)

    return result


# should string be ignored?
def ignore_string(string):
    # ignore empty string
    if string == '':
        return True

    # ignore articles, pronouns, etc.
    if string in IGNORE_STRINGS:
        return True

    # the name of the game
    if string.startswith('Crawl'):
        return True

    # ignore strings that are just whitespace
    if re.match(r'^\s*$', string):
        return True

    # ignore opengl functions
    if re.match(r'^gl[A-Z]', string):
        return True

    # ignore HTML and formatted text tags
    if re.match(r'^(\s|\[|\]|\(|\))*</?[^<>/]+>(\s|\[|\]|\(|\))*$', string):
        return True

    # ignore variable names
    if re.match(r'^\s*@[A-Za-z0-9_]+@?\s*$', string):
        return True

    # ignore identifiers
    if '_' in string and re.match(r"^[A-Za-z0-9_\- ']+$", string):
        return True
    if 'Gozag bribe' in string or 'Gozag permabribe' in string:
        return True
    if string == 'passage of golubria': # display name has uppercase G
        return True

    # ignore bug-catching stuff
    if 'INVALID' in string or 'DUMMY' in string or 'eggplant' in string:
        return True
    if re.search('bug', string, re.I) and 'bug-like' not in string \
       and 'bug report' not in string and 'program bug' not in string \
       and not re.search('debug', string, re.I):
        return True

    # ignore debug stuff
    if 'gdb' in string or 'Git' in string:
        return True

    # ignore filenames and file extensions
    if re.match(r'^[A-Za-z0-9_\-\/]*\.[A-Za-z]{1,4}$', string):
        return True

    # ignore format strings without any actual text
    temp = re.sub(r'%[\-\+ #0]?[\*0-9]*(\.[\*0-9]*)?(hh|h|l|ll|j|z|t|L)?[diuoxXfFeEgGaAcspn]', '', string)
    temp = re.sub('0x', '', temp); # Hexadecimal number indicator
    if not re.search(r'(?<!\\)[a-zA-Z]', temp):
        return True

    # ignore punctuation
    #if re.match(r'^[!\.\?]+$', string):
    #    return True

    return False


# extract strings from Lua line (can be enclosed in single or double quotes)
# inclosing quotes are included in the results
def extract_lua_strings(line):

    quote = None
    start_pos = None
    results = []

    for i in range(0, len(line)):
        if quote is None:
            if line[i] in "'\"":
                quote = line[i]
                start_pos = i
        elif line[i] == quote and line[i-1] != "\\":
            # end of string
            results.append(line[start_pos:i+1])
            quote = None

    return results

# where a name is overriden in a .des file, extract the new name and inflections
def extract_strings_from_des_rebadge_line(line):

    strings = []

    # multiple monsters can be on the same line separated by slashes
    # process them separately
    if '/' in line:
        lines = line.split('/')
        for l in lines:
            if re.search(r'\bname:', l):
                strings.extend(extract_strings_from_des_rebadge_line(l))
        return strings

    # remove any existing quotes
    line = line.replace('"', '')

    if re.search(r'\bshop\b', line):
        # Handle shop names
        line = re.sub(r'\s*\.\.\s*([a-zA-Z_]+)\s*\.\.', r'@\1@', line)
        line = line.replace('@smithy@', '@owner@')

        # extract owner name
        m = re.search(r'(?<=\bname:)[^ ]+', line)
        if not m:
            #print("DEBUG: " + line)
            return []
        owner = m.group()
        owner = owner.replace('_', ' ')

        # extract shop type
        m = re.search(r'(?<=\btype:)[^ ]+', line)
        if not m:
            #print("DEBUG: " + line)
            return []
        shop_type = m.group()

        name = owner + "'s " + shop_type

        # extract shop suffix
        m = re.search(r'(?<=\bsuffix:)[^ ]+', line)
        if m:
            name += " " + m.group()

        return [name]

    # extract base (original) name
    m = re.search(r'[a-z][a-z \-]+[a-z](?=\s)', line)
    if not m:
        return []
    base_name = m.group()

    # extract override
    m = re.search(r'(?<=\bname:)[^ ]+', line)
    if not m:
        return []
    override = m.group()
    override = override.replace('_', ' ')

    string = ""
    is_adjective = False
    if 'name_adjective' in line or 'n_adj' in line:
        if override in ["phase", "dire", "giga", "sulfuric", "bog"]:
            # generate the full name
            string = override + " " + base_name
        else:
            # just take the adjective
            string = override + " "
            is_adjective = True
    elif 'name_suffix' in line or 'n_suf' in line:
        string = base_name + " " + override
    else:
        string = override

    if string == "":
        return []

    # if adjective, just return this single string as is
    if is_adjective:
        strings.append(string)
        return strings

    if " " in string:
        for adj in ["rotten ", "ancient ", "large "]:
            if string.startswith(adj):
                strings.append(adj)
                string = string.replace(adj, '')
                break

    append_monster_permutations(strings, string)
    return strings


def process_lua_file(filename):

    is_des = filename.endswith('.des')
    is_portal = '/portals/' in filename

    infile = open(filename)
    data = infile.read()
    infile.close()

    raw_lines = data.splitlines()
    lines = []

    # a line ending in backslash means the statement continues on the next line
    for line in raw_lines:
        line = line.strip()
        if lines and lines[-1].endswith('\\'):
            if is_des and line.startswith(":"):
                line = line[1:].lstrip()
            lines[-1] = lines[-1][:-1].rstrip() + " " + line
        else:
            lines.append(line)

    raw_lines = lines
    lines = []

    ignore = is_des
    for line in raw_lines:
        if line.startswith('--') or line.startswith('#'):
            # skip comments
            continue
        elif line == '':
            continue

        if is_des:
            # lua code is surroned by double curlies
            if line.startswith(':'):
                lines.append(line)
                continue
            elif re.search(r'\bname:', line):
                lines.append(line)
                continue

            if '{{' in line and '}}' in line:
                lines.append(line)
                continue
            elif '{{' in line:
                ignore = False
            elif '}}' in line:
                lines.append(line)
                ignore = True

        if ignore:
            continue

        concatenate = False
        if len(lines) > 0:
            if line.startswith('..') or lines[-1].endswith('..'):
                concatenate = True
            elif line.startswith(',') or lines[-1].endswith(','):
                concatenate = True
            elif line.startswith('and ') or lines[-1].endswith(' and'):
                concatenate = True
            elif line.startswith('or ') or lines[-1].endswith(' or'):
                concatenate = True
            elif re.search(r'=\s*\{$', lines[-1]):
                concatenate = True

        if concatenate:
            lines[-1] += ' '  + line
        else:
            lines.append(line)

    wizlab_descs = []

    # for portals
    noise = None
    noisemaker = None

    strings = []
    section = ''
    for line in lines:
        if line.startswith('function '):
            section = re.sub(r'^function\s*', '', line)
            section = re.sub(r'\s*\(.*', '', section)
            strings.append('# section: ' + section)
            continue

        # don't extract map keys
        line = re.sub(r'\["[^"]*"\]', '[dummy]', line)

        # extract wizlab descriptions
        if filename.endswith('wizlab.des') and 'wizlab_milestone' in line and '"' in line:
            m = re.search('(?<=")[^"]+(?=")', line)
            if m and m.group(0):
                wizlab_descs.append(m.group(0))

        if '"' not in line and "'" not in line and 'name:' not in line:
            continue

        if is_portal:
            match = re.search(r'(?<=verb)\s*=\s*[\'"][^\'"]+(?=[\'"])', line)
            if match and match.group(0):
                noise = re.sub(r'\s*=\s*[\'"]', '', match.group(0))

            match = re.search(r'(?<=noisemaker)\s*=\s*[\'"][^\'"]+(?=[\'"])', line)
            if match and match.group(0):
                noisemaker = re.sub(r'\s*=\s*[\'"]', '', match.group(0))

            line = re.sub(r'(?:entity|dstname)\s*=\s*["\'][^"\']+["\']', '', line)
            line = re.sub(r'(?:noisemaker|verb)\s*=\s*["\'][^"\']+["\']', '', line)

            if noise is not None and noisemaker is not None:
                prefix = "You hear the @adjective@"
                strings.append(prefix + noise + " of " + article_a(noisemaker) + ".")
                strings.append(prefix + noise + " of a distant " + noisemaker + ".")
                strings.append(prefix + noise + " of a very distant " + noisemaker + ".")
                strings.append(prefix + noise + " of " + article_a(noisemaker) + " nearby.")
                strings.append(prefix + noise + " of " + article_a(noisemaker) + " very nearby.")
                noise = None
                noisemaker = None

        if is_des:
            skip = True
            if 'crawl.mpr' in line or 'crawl.god_speaks' in line:
                skip = False
            elif re.search(r'\bset_feature_name', line):
                # first param is a key
                line = re.sub('"[^"]",', 'dummy,', line)
                skip = False
            elif re.search(r'\bname:', line):
                strings.extend(extract_strings_from_des_rebadge_line(line))
                continue
            elif re.search('(?:msg|prompt)\s*=', line):
                skip = False
            elif is_portal and re.search(r'ranges\s*=', line):
                skip = False
            if skip:
                continue

        if 'debug' in section or 'dry_run ~= nil' in line or line.startswith('assert'):
            # debug stuff
            continue

        if 'note_payed' in section:
            # note
            continue

        if filename.endswith('lm_tmsg.lua') and section == 'TimedMessaging:init':
            continue

        if section.startswith('TroveMarker:search'):
            continue

        if section == 'TroveMarker:item_name':
            # this replicates the C++ item_name (I hope)
            continue

        if line.startswith('error') or line.startswith('flag_order = '):
            continue

        if line.endswith(' then'):
            # if or else
            continue

        if 'CLASS =' in line or '__index =' in line:
            continue

        if 'dgn_event_type' in line:
            continue

        # don't extract strings that are just used for comparison/search
        line = re.sub(r'==\s*\"[^"]*\"', '== dummy', line)
        line = re.sub(r'~=\s*\"[^"]*\"', '~= dummy', line)
        line = re.sub(r'find\s*\([^\)]+\)', 'find(dummy)', line)
        line = re.sub(r'match\s*\([^\)]+\)', 'match(dummy)', line)

        line = line.replace('dgn.feature_desc_at(x, y, "The")', 'the_feature')

        # join strings that are joined at runtime
        if '..' in line:
            line = re.sub(r'"\s*\.\.\s*"', '', line)
            line = re.sub(r"'\s*\.\.\s*'", '', line)

        # turn joins of variables, etc. into embedded params
        if '..' in line and 'name:' not in line:
            # embedded conditional
            line = re.sub(r'\.\.\s*\([^\)]+\)', '.. param', line)

            # dont make self a param in concatenation (e.g. msg = msg .. whatever)
            line = re.sub(r'([a-z]+)\s*=\s*\1\s*\.\.', r'\1 =', line)
            # param in middle of string
            line = re.sub(r'"\s*\.\.\s*([^"]*?)\s*\.\.\s*"', r'@\1@', line)
            # param at end of string
            line = re.sub(r'"\s*\.\.\s*(.*)', r'@\1@"', line)
            # param at start of string
            line = re.sub(r'(return\s*|mpr\(\s*)(.*?)\s*\.\.\s*"', r'\1"@\2@', line)
            line = re.sub(r'(.*?)\s*\.\.\s*"', r'"@\1@', line)
            #line = re.sub(r'"\s*\.\.\s*([a-zA-Z_]+)\s*\.\.\s*"', r'@\1@', line)
            line = re.sub(r'\s+\.\.\s+', '@@', line)
            line = line.replace('@AUTOMAGIC_SPELL_SLOT@', '@slot@')
            line = re.sub('@[^@]*AUTOMAGIC_SPELL_SLOT[^@]*@', '@spell_name@', line)
            line = re.sub(r'@[^@]+[\.:]([^@]+)@', r'@\1@', line)
            line = re.sub(r'\[([a-zA-Z0-9_]+)\]@', r'_\1@', line)
            line = line.replace('()@', '@')
            line = re.sub('\s*[\-\+]\s*[0-9]\s*@', '@', line)
            # verb is actually a noun
            line = line.replace('@chk_2@@verb@', '@adjective@@noise@')
            line = re.sub('@[^@]+the_feature@', '@the_feature@', line)
            line = re.sub('@([^@ ]+?)[\(\)][^@]*@', r'@\1@', line)

        if 'crawl.mpr' in line:
            # we don't want to extract the second parameter - it's the channel
            line = re.sub(r',\s*"[^"]*"\s*\);?$', ', channel)', line)
            line = re.sub(r",\s*'[^']*'\s*\);?$", ', channel)', line)

        matches = extract_lua_strings(line)
        for match in matches:
            string = match[1:-1] # remove quotes
            if len(string) < 2:
                continue
            if 'ERROR' in string or 'Error' in string or 'buggy' in string:
                continue
            if 'marker' in string or 'Marker' in string:
                continue
            if '_' in string and not '@' in string:
                # identifier
                continue
            if filename.endswith('automagic.lua') and string == " enabled,":
                continue
            if string in [" and", " the @name@", "@feat@/@dur@", r"AUTOMAGIC_SPELL_SLOT = '@slot@'\n"]:
                continue

            if filename.endswith('nemelex_the_gamble.des'):
                # expand
                if string.startswith(r'\"') and not string.startswith(r'\"Beware'):
                    string = 'Nemelex Xobeh says, ' + string
                elif string.startswith('Nemelex Xobeh says, @'):
                    continue

            # make sure double quotes are escaped
            string = re.sub(r'(?<!\\)"', r'\"', string)

            # make sure inital param is capitalised
            string = re.sub('^@the_', '@The_', string)

            if string == "no spell currently":
                strings.append("# note: @spell_name@ when no spell in chosen slot")

            # split on newlines
            substrings = string.split("\\n")
            for ss in substrings:
                if ss != "":
                    strings.append(ss)
                    if re.search(r'\bset_feature_name', line):
                        if ss.startswith('a '):
                            strings.append(ss[2:]);
                        elif ss.startswith('an '):
                            strings.append(ss[3:]);
                        elif not re.match('^([A-Z]|the |some )', ss):
                            strings.append(article_a(ss));

    # separate and clean up annotations
    if filename.endswith('stash.lua'):
        raw_strings = strings
        strings = []
        for string in raw_strings:
            if '} {' in string:
                substrings = string.split('} {')
                for ss in substrings:
                    if '@res@' in string:
                        # expanded below (cold, corrosion, etc.)
                        continue
                    ss = ss.replace('{', '').replace('}', '').strip()
                    strings.append(ss)
            elif '+' in string or ('-' in string and '-handed' not in string):
                substrings = string.split(" ")
                for ss in substrings:
                    strings.append(ss.replace('+', '').replace('-', ''))
            else:
                string = string.replace('{', '').replace('}', '').strip()
                if string in ['melee', 'ranged']:
                    string += ' weapon'
                elif string in ['cold', 'corrosion', 'electricity', 'fire', 'mutation', 'negative energy', 'poison']:
                    strings.append('resist ' + string)
                    strings.append(string + ' resistance')
                    continue
                elif string == '@subtype@ armor':
                    strings.append('body armour')
                else:
                    strings.append(string)

    # expand params
    raw_strings = strings
    strings = []
    for string in raw_strings:
        if "@" not in string and "$F" not in string:
            strings.append(string)
            continue

        # ignore if just a param and nothing else
        if re.match(r'^@[^@]*@$', string):
            continue

        alternatives = [string]
        if '@plural@' in string:
            alternatives = expand_param(string, "@plural@", ["", "s"])
        elif '@caught@' in string:
            alternatives = expand_param(string, "@caught@", ['held in a net', 'caught in a web'])
        elif filename.endswith('automagic.lua') and '@message@' in string:
            alternatives = expand_param(string, "@message@", ["", " enabled,"])
        elif filename.endswith('bailey.des') and "$F{The}" in string:
            alternatives = expand_param(string, "$F{The}", ["The portcullis"])
        elif "the wizard's @param@cell" in string:
            alternatives = expand_param(string, "@param@", ["", "empty "])
        elif '@spawn_dir@' in string:
            alternatives = expand_param(string, "@spawn_dir@", ["north", "south", "east", "west"])
        elif 'sudden vision@msg@' in string:
            alternatives = expand_param(string, "@msg@", [" of the Swamp and the Snake Pit", \
                                                          " of the Swamp and the Spider Nest", \
                                                          " of the Shoals and the Snake Pit", \
                                                          " of the Shoals and the Spider Nest" \
                                                         ])
        elif '@wizlab_desc@' in string:
            alternatives = expand_param(string, "@wizlab_desc@", wizlab_descs)
        elif filename.endswith('pan.des') and '@name@ resides here' in string:
            alternatives = expand_param(string, "@name@", ["Cerebov", "Mnoleg", "Lom Lobon", "Gloorx Vloq"])
        elif filename.endswith('pan.des') and '@runes_name@' in string:
            alternatives = expand_param(string, "@runes_name@", ["fiery", "glowing", "magical", "dark"])
        elif '@noise@ of @noisemaker@' in string:
            # will be covered under each specific portal
            alternatives = []

        strings.extend(alternatives)

    return strings

# special handling for strings in feature-data.h
def special_handling_for_feature_data_h(strings):
    output = []
    adjectives = []
    for string in strings:
        if string.startswith('#'):
            output.append(string)
        elif string.endswith(' door') or string.endswith(' gate'):
            # we handle door adjectives as separate strings
            words = separate_adjectives(string)
            for i in range(len(words)):
                if i == len(words) - 1:
                    output.append(words[i]);
                else:
                    adjectives.append(words[i])
        elif string.endswith("golubria"):
            # the version with a small g is an internal id
            continue
        elif string in ['explore horizon', 'unseen']:
            output.append(string)
        elif string.startswith('some '):
            output.append(string)
        elif string.startswith('the '):
            output.append(string)
        elif string.startswith('a '):
            output.append("the " + string[2:])
        else:
            output.append("the " + string)

    output.append("# section: door/gate adjectives")
    for string in adjectives:
        output.append(string)

    # do we need plurals?

    return output

# special handling for strings in mon-data.h
def special_handling_for_mon_data_h(strings):
    output = []
    names = []
    unique_names = []
    adjectives = []

    # separate unqiue from non-unique
    for string in strings:
        if string.startswith('#'):
            continue
        if string.endswith(' '):
            adjectives.append(string)
        elif is_unique_monster(string):
            unique_names.append(string)
        else:
            names.append(string)

    names.sort()
    unique_names.sort()

    # adjectives
    output.append("# section: adjectives")
    for string in adjectives:
        output.append(string)

    # singular non-unique
    output.append("# section: non-unique monsters, singular")
    for string in names:
        output.append('the ' + string)

    # singular unique
    output.append("# section: unique monsters")
    for string in unique_names:
        if string.startswith('the '):
            output.append(string)
        else:
            output.append('the ' + string)

    # possessive non-unique
    output.append("# section: non-unique monsters, singular possessive")
    for string in names:
        output.append('the ' + possessive(string))

    # possessive unique
    output.append("# section: unique monsters, possessive")
    for string in unique_names:
        if string.startswith('the '):
            output.append(possessive(string))
        else:
            output.append(possessive(string))

    # plural non-unique
    output.append("# section: non-unique monsters, plural")
    for string in names:
        output.append('%d ' + pluralise(string))

    return output

# special handling for strings in item-prop.cc
def special_handling_for_item_prop_cc(strings):
    output = []
    plurals = []

    for string in strings:
        if string.startswith('#'):
            # comment
            output.append(string)
            continue
        elif string in ['steam', 'acid', 'quicksilver', 'swamp', 'fire', 'ice', 'pearl', 'storm', 'shadow', 'gold']:
            string = string + ' dragon scales'
        elif string == ' dragon scales':
            # all possibilities covered above
            continue
        elif string in ['gloves', 'boots']:
            string = 'pair of ' + string
        elif string in ['javelin', 'boomerang']:
            output.append("the " + string)
            string = 'silver ' + string

        # stackable items need a plural with count
        if is_missile(string) or "potion" in string or "scroll" in string:
            plurals.append('%d ' + pluralise(string))

        output.append("the " + string)

    output.extend(plurals)
    return output

# special handling for strings in item-name.cc
# you'd think from the filename that everything in here would be a name, but you'd be wrong
def special_handling_for_item_name_cc(strings):
    result = []
    extras1 = []
    extras2 = []
    section = ''

    for string in strings:
        if string.startswith('# section:'):
            # new section starts
            result.extend(extras1)
            result.extend(extras2)
            extras1 = []
            extras2 = []
            section = string.replace('# section:', '').strip()
        elif string.startswith('#'):
            string = string # null op
        elif section in ['_random_vowel', '_random_cons', '_random_consonant_set', 'make_name']:
            # random-name generation - ignore
            continue
        elif '_test' in section:
            # test stuff
            continue
        elif section.endswith('_secondary_string') or section == 'staff_primary_string':
            # extract adjective as separate word
            if not string.endswith(' '):
                string += ' '
        elif section.endswith('_primary_string'):
            # primary adjective (closest to the noun)
            noun = re.sub('_.*', '', section)
            if not string.endswith(' '):
                string += ' '
            string = 'the ' + string + noun
        elif section == 'item_def::name':
            if string == ' (in ':
                result.append(' (in hand)')
                result.append(' (in claw)')
                result.append(' (in tentacle)')
                continue
            elif string in ['right', 'left']:
                result.append(' (' + string + ' hand)');
                result.append(' (' + string + ' claw)');
                result.append(' (' + string + ' paw)');
                result.append(' (' + string + ' tentacle)');
                result.append(' (' + string + ' branch)');
                result.append(' (' + string + ' front leg)');
                result.append(' (' + string + ' blade hand)');
                continue
        elif section == 'missile_brand_name':
            if string.endswith(' dart'):
                extras1.append(pluralise(string))
                extras2.append('%d ' + pluralise(string))
                string = 'the ' + string
        elif section == 'weapon_brands_terse':
            if string == 'confuse':
                # not a real weapon brand - used on hands for confusing touch
                continue
            elif string == 'flame':
                # terse version also used after "of" (see _item_ego_name in religion.cc)
                result.append(' of ' + string)
        elif section == 'weapon_brands_verbose':
            if string == 'confusion':
                # not a real weapon brand - used on hands for confusing touch
                continue
            elif string in ['vampirism', 'antimagic', 'vorpality', 'spectralizing']:
                # verbose name is never used (see brand_prefers_adj)
                continue
            string = ' of ' + string
        elif section == 'weapon_brands_adj':
            # adjectives defined for all, but only used for some (see brand_prefers_adj)
            if string in ['vampiric', 'antimagic', 'vorpal', 'spectral']:
                string = string + ' '
            else:
                continue
        elif section == 'armour_ego_name':
            string = ' of ' + string
        elif section == 'armour_ego_name(terse)':
            if string == 'rC+ rF+':
                # handled as two separate strings
                continue
            # the plus is handled separately
            string = re.sub(r'\+.*', '', string)
        elif section == '_wand_type_name':
            string = 'wand of ' + string
            if not string.endswith('removedness'):
                # uncounted plural for known items menu
                extras1.append(pluralise(string))
            string = 'the ' + string
        elif section == 'potion_type_name':
            string = 'potion of ' + string
            # uncounted plural for known items menu
            extras1.append(pluralise(string))
            # counted plural for stacks
            extras2.append('%d ' + pluralise(string))
            string = 'the ' + string
        elif section == 'scroll_type_name':
            string = 'scroll of ' + string
            # uncounted plural for known items menu
            extras1.append(pluralise(string))
            # counted plural for stacks
            extras2.append('%d ' + pluralise(string))
            string = 'the ' + string
        elif section == 'jewellery_effect_name':
            string = ' of ' + string
        elif section == 'jewellery_effect_name(terse)':
            # the plus is handled separately
            string = re.sub(r'\+.*', '', string)
        elif section == 'rune_type_name':
            if string in ['mossy', 'elven']:
                # obsolete
                continue
            else:
                extras1.append('the ' + string + ' rune')
        elif section == 'misc_type_name':
            # uncounted plural for known items menu
            if string != 'horn of Geryon':
                extras1.append(pluralise(string))
            string = 'the ' + string
        elif section == '_book_type_name':
            if string == 'Fixed Level' or string == 'Fixed Theme':
                continue
            string = 'a book of ' + string
        elif section == 'sub_type_string':
            if string == 'manual':
                result.append(string)
                string = pluralise(string)
            elif is_spellbook(string):
                string = add_spellbook_article(string)
        elif section == 'staff_type_name':
            extras1.append('staves of ' + string)
            string = 'the staff of ' + string
        elif section == 'ghost_brand_name':
            if string == '%s weapon':
                string = 'the weapon'
            elif string == 'weapon of %s':
                # suffixes handles separately
                continue
            elif string == '%s touch':
                # there's only one possibility
                string = 'confusing touch'
        elif section == 'potion_colours':
            if not string.endswith(' '):
                string += ' '
        elif section == 'display_runes':
            if string == "green":
                # text colour tag
                continue
        elif section == 'item_prefix':
            # undisplayed, but (supposedly) searchable prefixes
            # many of these don't even work in English
            continue

        if string in ['wand of ', 'potion of ', 'scroll of', 'ring of', 'amulet of', 'staff of ', 'book of ']:
            # all subtypes already covered above
            continue
        elif string in [' wand', ' potion', ' ring', ' amulet', ' rune']:
            # all subtypes already covered above
            continue
        elif string == "Orb of Zot":
            string = "the " + string
        elif string in ['manual of ', '%s of %s', ' of ', 'of '] or (string.endswith(' of Zot') and string != "The Orb of Zot"):
            # other "of <foo>" suffixes are handled separately
            continue
        elif string == "gold piece":
            result.append('the ' + string)
            result.append('%d ' + pluralise(string))
            continue
        elif string == 'enchanted %s':
            # will be handled the other way round, with "enchanted" as added adjective
            string = 'enchanted '
        elif string == "damnation ":
            # there's only one possibility
            result.append("the damnation bolt")
            result.append('%d damnation bolts')
            continue
        elif string == "labelled ":
            result.append("the scroll labelled %s")
            string = "%d scrolls labelled %s"
        elif string == "x) ":
            # ignore - just used for size
            continue
        elif string == "pair of ":
            # handled in item-prop.cc
            continue
        elif string == "decaying skeleton":
            # dbname (just used as a lookup key, not displayed)
            continue
        elif "bug" in string or "bad item" in string or "bogus" in string:
            # case that should never happen - ignore
            continue

        result.append(string)

    result.extend(extras1)
    result.extend(extras2)

    return result

def process_cplusplus_file(filename):
    lazy = (filename in LAZY_FILES)

    strings = []

    lines = get_cleaned_file_contents(filename)
    lines = do_first_stage_line_processing(lines)
    lines = insert_section_markers(filename, lines)
    lines = get_relevant_lines(filename, lines)

    #if filename == 'item-name.cc':
    #    dump_lines('extract-text.1.dump', lines)

    section = ''
    last_section = ''
    for line in lines:
        #sys.stderr.write(line + "\n")

        if '//' in line:
            if 'locnote' in line:
                if section != last_section:
                    strings.append('# section: ' + section)
                    last_section = section
                note = re.sub(r'^.*locnote: *', '# note: ', line)
                strings.append(note)
                line = strip_line_comment(line)
                line = line.strip()
            elif '@locsection' in line:
                section = re.sub(r'^.*locsection:? *', '', line)
                continue

        if '"' not in line:
            continue

        extract = False

        if 'localise' in line:
            extract = True
        elif 'simple_monster_message' in line or 'simple_god_message' in line:
            extract = True
        elif 'any_2_actors_message' in line or '3rd_person_message' in line:
            extract = True
        elif 'MSGCH_DIAGNOSTICS' in line:
            # ignore diagnostic messages - these are for devs
            continue
        elif re.search(r'mpr[a-zA-Z_]* *\(', line):
            # extract mpr, mprf, etc. messages
            if 'MSGCH_ERROR' in line:
                # Error messages mostly relate to programming errors, so we
                # keep the original English for the user to report to the devs.
                # The only exception is file system-related messages, which
                # relate to the user's own environment.
                if 'file' not in line and 'directory' not in line \
                   and 'writ' not in line and ' read ' not in line \
                   and 'lock' not in line and 'load' not in line \
                   and ' save' not in line and 'open' not in line:
                    continue
            extract = True
        elif re.search(r'(prompt|msgwin_get_line)[a-zA-Z_]* *\(', line) or 'yesno' in line \
             or 'yes_or_no' in line:
            # extract prompts
            extract = True
        elif re.match(r'\s*end *\(', line) and not 'DEBUG' in line:
            extract = True
        elif re.search(r'\bsave_game *\(', line):
            extract = True
        elif re.search(r'\bhand_act *\(', line):
            extract = True
        elif 'cant_cmd_' in line:
            extract = True
        elif 'get_num_and_char' in line:
            extract = True

        if lazy:
            # ignore strings unless we have a specific reason to extract them
            if not extract:
                continue

        # we don't want to extract the db key used with getSpeakString(), etc.,
        # but we don't necessarily want to ignore the whole line because
        # sometimes there are other strings present that we do want to extract
        if re.search(r'\bget[a-zA-Z]*String', line):
            line = re.sub(r'\b(get[a-zA-Z]*String) *\(.*\)', r'\1()', line)

        # we don't want to extract the context key used with localise_contextual()
        if 'localise_contextual' in line:
            line = re.sub(r'localise_contextual *\(.*,', 'localise_contextual(dummy,', line)

        if '"' not in line:
            continue

        if not extract:
            # if we get here then we are not in lazy mode
            # extract strings unless we have reason to ignore them

            # ignore debug messages
            if re.search(r'\bdie(_noline)? *\(', line) or \
               re.search(r'dprf? *\(', line) or \
               re.search(r'dprintf *\(', line) or \
               re.search(r'(debuglog|debug_dump_item|dump_test_fails) *\(', line) or \
               re.search(r'bad_level_id', line) or \
               'game_ended_with_error' in line or \
               re.search(r'ASSERTM? *\(', line) or \
               'DEBUG' in line or \
               'log_print' in line or \
               re.search(r'fprintf *\(', line):
                continue

            # ignore axed stuff
            if 'AXED' in line:
                continue

            # ignore file operations (any strings will be paths/filenames/modes)
            if 'fopen' in line or 'freopen' in line:
                continue
            if '_hs_open' in line or 'lk_open' in line:
                continue
            if 'catpath' in line or 'sscanf' in line:
                continue

            # internal scorefile stuff that is never displayed
            if 'add_field' in line or 'str_field' in line or 'int_field' in line:
                continue
            if 'death_source_flags' in line:
                continue

            # ignore lua code
            if 'execfile' in line:
                continue
            if re.search(r'^[^"]*lua[^"]*\(', line):
                continue

            # Leave notes/milsones in English
            if 'milestone' in line or 'mile_text' in line:
                continue
            if re.search('take_note', line) or re.search('note *=', line):
                continue
            if re.search('delete[a-z_]*mutation', line):
                continue
            if re.search(r'mutate\s*\(', line):
                continue
            if re.search(r'\bbanish(ed)?\s*\(', line):
                continue

            # score file stuff
            if re.search(r'\bouch\s*\(', line) or re.search(r'\bhurt\s*\(', line) \
               or re.search(r'\bparalyse\s*\(', line) \
               or re.search(r'\bpetrify\s*\(', line) \
               or re.search(r'\bmiscast_effect\s*\(', line) \
               or 'aux_source' in line:
                continue

            # skip tags/keys
            if re.search(r'^[^"]*_tag\(', line) and not re.search('text_tag', line):
                continue
            if re.search('tag\s*=\s*"', line):
                continue
            if re.search(r'strip_tag_prefix *\(', line):
                continue
            if 'annotate_string' in line:
                continue
            if 'json_' in line:
                continue
            if 'tiles.write_message' in line:
                continue
            if 'serialize' in line:
                continue
            if '_chunk' in line:
                continue
            if '_id =' in line:
                continue
            if 'push_ui_layout' in line or 'ui_state_change' in line:
                continue
            if re.search(r'\bmenu_colour *\(', line):
                continue
            if re.search(r'\bprops\.erase *\(', line):
                continue
            if 'getLongDescription' in line:
                continue
            if '_print_converted_orc_speech' in line:
                continue
            if '_get_xom_speech' in line or 'XOM_SPEECH' in line:
                continue
            if '_get_species_insult' in line:
                continue
            if 'show_specific_help' in line:
                continue
            if 'print_hint' in line or 'tutorial_msg' in line:
                continue
            if re.search('^[^"]*property[A-Za-z_]* *\(', line):
                continue
            if re.match(r'^\s*key[A-Za-z_]*\.[A-Za-z_]*\(', line):
                continue
            if re.search(r'set_sync_id\s*\(', line):
                continue
            if re.search(r'compare_item', line):
                continue
            if re.search(r'^# *define.*KEY', line):
                continue
            if 'desc_key' in line:
                continue
            if 'GetModuleHandle' in line:
                continue
            if re.search(r'\bcreate_item_named *\(', line):
                continue

            # find or compare
            if re.search(r'\bstrstr\s*\(', line):
                continue
            if 'search_stashes' in line:
                continue
            if re.search(r'\bstrn?i?cmp\b', line):
                continue
            if '_strip_to' in line:
                continue

            if re.search(r'\bstrlen\s*\(', line):
                continue

        extract = True

        if 'any_2_actors_message' in line:
            temp = re.sub('.*any_2_actors_message *\(', '', line);
            temp = re.sub('\).*', '', line);
            args = temp.split(',')
            verb_pos = -1
            verb = ''
            suffix = ''
            if len(args) >= 3 and len(args) <= 5 and args[2].strip()[0] == '"':
                verb_pos = 2
            elif len(args) >= 5 and args[4].strip()[0] == '"':
                verb_pos = 4
            if verb_pos > 0:
                verb = args[verb_pos].strip().replace('"', '')
                if len(args) >= verb_pos + 2:
                    suffix = args[verb_pos+1].strip().replace('"', '')
            if suffix != '':
                suffix = ' ' + suffix
            if verb != '':
                strings += do_any_2_actors_message(verb, suffix)
                continue



        # tokenize line into string and non-string
        tokens = []
        token = ""
        escaped = False
        in_string = False
        for i in range(len(line)):
            ch = line[i]
            if ch == '"' and not escaped:
                if in_string:
                    token += ch
                    tokens.append(token)
                    token = ""
                    in_string = False
                else:
                    if token != "":
                        tokens.append(token)
                    token = ch
                    in_string = True
                continue

            if ch == '\\' and not escaped:
                escaped = True
            else:
                escaped = False

            token += ch

        if token != "":
            tokens.append(token)

        for i in range(len(tokens)):
            token = tokens[i]
            if len(token) < 3 or token[0] != '"' or token[-1] != '"':
                continue;

            string = token[1:-1]
            if string in strings:
                continue

            if i != 0:
                last = tokens[i-1]

                # skip (in)equality tests (assume string is defined elsewhere)
                if re.search(r'[=!]=\s*$', last):
                    continue
                if re.search(r'\bstr(case)?cmp\b', last):
                    continue


                # skip map keys
                if re.search(r'\[\s*$', last):
                    continue

                if '(' in last:
                    # another type of equality test
                    if re.search(r'\bstarts_with\s*\([^,"]+,\s*$', last):
                        continue
                    if re.search(r'\bends_with\s*\([^,"]+,\s*$', last):
                        continue
                    if re.search(r'\bfind\s*\(\s*(string\()?$', last):
                        continue
                    if re.search(r'\bexists\s*\(\s*$', last):
                        continue
                    if re.search(r'\bcontains\s*\(', last):
                        continue

                    if re.search(r'\bsplit_string\s*\(', last):
                        continue
                    if re.search(r'\bstrip_suffix\s*\(', last):
                        continue
                    if re.search(r'\bsend_exit_reason\s*\(', last):
                        continue
                    if re.search(r'\bsend_dump_info\s*\(', last):
                        continue
                    if re.search(r'\bmons_add_blame\s*\(', last):
                        continue
                    if re.search(r'\breplace[a-zA-Z_]*\s*\(', last):
                        continue

            # simple_god/monster_message may contain an implied %s
            if string.startswith(" ") or string.startswith("'"):
                if 'simple_god_message' in line or 'simple_monster_message' in line \
                  or '_spell_retribution' in line \
                  or (filename == 'mon-abil.cc' and section == 'ugly_thing_mutate') \
                  or (filename == 'mon-cast.cc' and section == '_cast_cantrip') \
                  or (filename == 'mon-death.cc' and section == 'monster_die') \
                  or (filename == 'monster.cc' and section == 'monster::do_shaft'):
                    string = '%s' + string

            if 'wu_jian_sifu_message' in line:
                # this function adds a prefix to the message parameter
                string = 'Sifu %s' + string
            elif '3rd_person_message' in line:
                # also do the version where "you" is the object
                strings.append(replace_last(string, '%s', 'you'))

            if 'held_status' in line and 'while %s' in string:
                # there are only two possibilities
                strings.append(string.replace('while %s', 'while held in a net'))
                strings.append(string.replace('while %s', 'while caught in a web'))
                continue

            if 'convert_input_to_english' in line:
                # separate input chars
                for c in string:
                    strings.append(c)
                continue

            if section != last_section:
                strings.append('# section: ' + section)
                last_section = section

            if filename == 'ability.cc':
                if string.startswith('Sacrifice '):
                    if string == 'Sacrifice ':
                        continue
                    # also used with 'Sacrifice ' removed for the cost
                    strings.append(string.replace('Sacrifice ', ''))
            elif filename == 'delay.cc':
                if string.startswith(' ') and section in ['_monster_warning', '_abyss_monster_creation_message']:
                    string = "%s" + string
            elif filename == 'describe-spells.cc':
                if section == "_ability_type_vulnerabilities":
                    # will be joined to strings from _abil_type_vuln_core before translation
                    if string == ", which are affected by %s":
                        continue
                elif section == "_abil_type_vuln_core":
                    # will be joined to string from _ability_type_vulnerabilities before translation
                    string = ", which are affected by " + string
            elif filename == 'god-wrath.cc':
                # god wratch names are mostly used only in notes.
                # the exception is the Wu Jian one
                if section == '_god_wrath_adjectives':
                    if string == 'rancor' or string == 'rancour':
                        string = 'the ' + string + ' of the Wu Jian Council'
                    else:
                        continue
                elif section == '_god_wrath_name':
                        continue
            elif filename == 'hints.cc':
                if section == '_replace_static_tags':
                    # error messages for translator, not end user
                    continue
                elif section == '_hints_target_mode':
                    # literal commands (and % placeholder)
                    continue
            elif filename == 'item-name.cc':
                if section == 'missile_brand_name':
                    if 'MBN_NAME' in line:
                        # first string is long name, second is terse
                        if not line.endswith('"' + string + '";'):
                            # used as adjective on darts
                            string += ' dart'
                    elif 'MBN_TERSE' in line:
                        # first string is terse name, second is long name
                        if line.endswith('"' + string + '";'):
                            # used as suffix
                            string = ' of ' + string
                    else:
                        # string acts as both long and terse name
                        if string == 'silver':
                            strings.append(string + ' ')
                        else:
                            strings.append(' of ' + string)
            elif filename == 'message.cc':
                if section == 'wu_jian_sifu_message':
                    # this function adds a prefix to the message parameter
                    # this script will add it when extracting the message at the point of call (see above)
                    continue
            elif filename == 'mon-cast.cc':
                if section in ['_speech_keys', '_speech_message']:
                    # these are just keys for getSpeakString()
                    continue
                elif section in ['_speech_fill_target', 'mons_cast_noise']:
                    # filling params for speech strings, but we don't want to directly translate
                    # the prepositions (it's handled in monspeak.txt)
                    if 'target' not in line:
                        continue
                    if string == "nothing" or string.upper() == string:
                        # dummy placeholder
                        continue
            elif filename == 'mon-death.cc':
                if section == '_milestone_kill_verb':
                    # only used for milesone
                    continue
                elif section == '_killer_type_name':
                    # internal keys
                    continue
            elif filename == 'monster.cc':
                if section == '_invalid_monster_str':
                    # debugging stuff
                    continue
            elif filename == 'mon-util.cc':
                if section in ['ugly_colour_names', 'drac_colour_names']:
                    # adjectives
                    string += ' '
                elif section == "_get_sound_string":
                    # we need the full thing (e.g. "says to @foe@") and also the verb alone
                    verb = string.replace(" at @foe@", "").replace(" to @foe@", "")
                    strings.append(verb)
            elif filename == 'output.cc':
                if section == 's_equip_slot_names' or section == 'equip_slot_to_name':
                    # equipment slots are only ever displayed in lowercase form
                    # and the specific ring slots are all just displayed as "ring"
                    if string.endswith(" Ring"):
                        string = "ring"
                    else:
                        string = string.lower()
            elif filename == 'spl-goditem.cc':
                if string.startswith('a scroll of '):
                    string = string.replace('a scroll', 'the scroll')
            elif filename == 'throw.cc' and section == '_setup_missile_beam':
                if string in ["explosion of ", " fragments"]:
                    # beam for explosive brand - now only used for Damnation artefact, and not displayed
                    continue
            elif filename in ['art-func.h', 'attack.cc']:
                if string in ["melt", "burn", "freeze", "electrocute", "crush"]:
                    strings += do_any_2_actors_message(string, "")
                    continue

            # strip channel information
            string = re.sub(r'(PLAIN|SOUND|VISUAL|((VISUAL )?WARN|ENCHANT|SPELL)):', '', string)

            if string == " god" and "PRONOUN_POSSESSIVE" in line:
                strings.append("his god")
                strings.append("her god")
                strings.append("its god")
                strings.append('# note: singular "their"')
                strings.append("their god")
                continue

            if "\\n" in string:
                # split lines
                substrings = string.split("\\n")
                for ss in substrings:
                    strings.append(ss)
            else:
                if 'our @hand' in string:
                    # create strings for one and two hands (coz Ru)
                    string = string.replace('your @hand', '@your_hand')
                    string = string.replace('Your @hand', '@Your_hand')
                    string2 = string.replace('hands@', 'hand@')
                    string2 = string2.replace('@hand_conj@', 's')
                    string = string.replace('@hand_conj@', '')
                    strings.append(string2)
                strings.append(string)
                if filename == 'spl-miscast.cc' and "'s body" in string:
                    # string is also used with that substring for monsters that don't have a body
                    strings.append(string.replace("'s body", ""))

    return strings


# get rid of unnecessary section markers
def remove_unnecessary_section_markers(strings):
    section = None
    output = []
    for string in strings:
        if string.startswith('# section:'):
            section = string
        else:
            if section is not None:
                output.append(section)
                section = None
            output.append(string)
    return output

# separate adjectives from noun
# adjectives will have space appended
def separate_adjectives(string):
    words = string.split()
    for i in range(len(words)):
        if i != len(words) - 1:
            words[i] = words[i] + " "
        else:
            words[i] = "the " + words[i]
    return words

def article_the(string):
    if string.startswith("the "):
        return string
    else:
        return "the " + string

def article_a(string):
    if re.search('^[aeiouAEIOU]', string) and not string.startswith('one-'):
        return "an " + string
    else:
        return "a " + string

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
        return string[:-4] + feet
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

def possessive(string):
    return string + "'s"

def is_unique_monster(string):
    # non-uniques with uppercase letters in them
    specials = [
        'Killer Klown', 'Orb Guardian', 'Brimstone Fiend', 'Ice Fiend',
        'Tzitzimitl', 'Hell Sentinel', 'Executioner', 'Hellbinder',
        'Cloud Mage', 'Statue of Wucad Mu', 'mad acolyte of Lugonu'
    ]

    if not re.search('[A-Z]', string):
        return False

    for special in specials:
        if special in string:
            return False;

    return True

def is_unique_noun(string, is_monster = False):
    if is_monster:
        return is_unique_monster(string)
    elif string.startswith("the "):
        return True
    elif re.search("[A-Z]", string):
        return True
    else:
        return False

def is_missile(string):
    for s in ["dart", "boomerang", "javelin", "throwing net", "stone", "large rock", "bullet", "arrow", "bolt"]:
        if string.endswith(s):
            return True
    return False

def is_spellbook(string):
    if string.startswith("book of ") and not string == "book of ":
        return True

    return string in [
        "Necronomicon",
        "Grand Grimoire",
        "Everburning Encyclopedia",
        "Ozocubu's Autobiography",
        "Young Poisoner's Handbook",
        "Fen Folio",
        "Inescapable Atlas",
        "There-And-Back Book",
        "Great Wizards, Vol. II",
        "Great Wizards, Vol. VII",
        "Trismegistus Codex",
        "the Unrestrained Analects",
    ]

def add_spellbook_article(string):
    if string.startswith("the "):
        # already has article
        return string
    elif string.startswith("Great Wizards") or string == "Ozocubu's Autobiography":
        # never gets an article
        return string
    else:
        # can have a/an
        return article_a(string)

def get_noun_permutations(string, is_monster = False):
    list = []

    if string.startswith("The "):
        list.append(string)
        if is_monster:
            list.append(string + "'s")
    else:
        is_unique = is_unique_noun(string, is_monster)
        base = re.sub("^(the|a|an) ", "", string)

        full = "the " + base
        list.append(full)

        # possessive (for monsters)
        if is_monster:
            if is_unique:
                list.append(possessive(string))
            else:
                list.append(possessive(full))

    return list

def append_monster_permutations(list, string):
    list.extend(get_noun_permutations(string, True))

def add_strings_to_output(filename, strings, output):
    if len(strings) == 0:
        return

    output.append("")
    output.append("##################")
    output.append("# " + filename)
    output.append("##################")
    for string in strings:
        # in some cases, string needs to be quoted
        #   - if it has leading or trailing whitespace
        #   - if it starts with # (because otherwise it looks like a comment)
        #   - if it starts and ends with double-quotes
        if string.startswith('# section:'):
            output.append(string)
            continue
        elif '# note' in string:
            output.append(string)
            continue
        elif re.search('^(\s|#)', string) or  re.search('\s$', string) \
           or (string.startswith(r'\"') and string.endswith('"')):
            string = '"' + string + '"'
        else:
            string = string.replace(r'\"', '"')
        string = string.replace(r'\\', '\\')

        if string in output:
            output.append('# duplicate: ' + string)
        else:
            output.append(string)

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
    yaml_deprec = glob.glob("dat/species/*deprecated*.yaml")
    yaml_files = list(set(yaml_files) - set(yaml_deprec))
    yaml_files.sort()

    lua_files = glob.glob("dat/clua/*.lua")
    lua_files.append("dat/dlua/lm_timed.lua")
    lua_files.append("dat/dlua/lm_tmsg.lua")
    lua_files.append("dat/dlua/lm_trove.lua")
    lua_files.sort()
    source_files.extend(lua_files)

    des_files_raw = glob.glob("dat/des/*/*.des")
    des_files = []
    for filename in des_files_raw:
        if not '/builder/' in filename:
            des_files.append(filename)
    des_files.sort()
    source_files.extend(des_files)

    # put some important files first
    # (because if there are duplicate strings, we want them put under these files)
    files = SPECIAL_FILES.copy()
    files.extend(yaml_files)

    # add wanted source files to list to be processed
    for fname in source_files:
        if fname not in files and \
           fname not in SKIP_FILES and \
           not re.match('l-', fname) and \
           not re.match('dbg-', fname):
            files.append(fname)

output = []

for filename in files:

    strings = []
    if filename == 'art-data.txt':
        strings = process_art_data_txt()
    elif filename.endswith('.yaml'):
        strings = process_yaml_file(filename)
    elif filename.endswith('.lua') or filename.endswith('.des'):
        strings = process_lua_file(filename)
    else:
        strings = process_cplusplus_file(filename)

    # the strings in some files need special handling
    canonicalised = True
    if filename == 'feature-data.h':
        strings = special_handling_for_feature_data_h(strings)
    elif filename == 'item-name.cc':
        strings = special_handling_for_item_name_cc(strings)
    elif filename == 'item-prop.cc':
        strings = special_handling_for_item_prop_cc(strings)
    elif filename == 'mon-data.h':
        strings = special_handling_for_mon_data_h(strings)
    elif filename != 'art-data.txt':
        canonicalised = False
        filtered_strings = []
        for string in strings:
            if string.startswith('# note:') or string.startswith('# section:'):
                filtered_strings.append(string)
            elif filename == 'species-data.h' and string == "Yak":
                # error condition
                continue
            elif string == "Walk":
                # species walk verb and associated noun
                filtered_strings.append(string + "ing")
                filtered_strings.append(string + "er")
            elif string == "runed door":
                # should be covered by feature-data.h, but just in case...
                words = separate_adjectives(string)
                filtered_strings.extend(words)
            elif string.startswith("shaped "):
                # separate "shaped" out as an adjective
                filtered_strings.append("shaped ");
                filtered_strings.append("@monster@ shaped ");
                append_monster_permutations(filtered_strings, string.replace("shaped ", ""))
            elif string in ["spectre", "wavering orb of destruction"]:
                # treat like monsters in mon-data.h
                append_monster_permutations(filtered_strings, string)
            elif string == " the pandemonium lord":
                filtered_strings.append(string.strip())
            elif string == "deck of " or string == "decks of ":
                if string == "deck of ":
                    string = "the " + string
                for suffix in ["destruction", "escape", "summoning", "punishment"]:
                    filtered_strings.append(string + suffix);
            elif string == "your stacked deck":
                filtered_strings.append("the stacked deck")
                IGNORE_STRINGS.append("a stacked deck")
                IGNORE_STRINGS.append("stacked deck")
            elif string.endswith(" Sword"):
                # alternative names for Singing Sword based on mood
                filtered_strings.append("the " + string)
            else:
                filtered_strings.append(string)
        strings = filtered_strings

    # remove duplicates and strings that should be ignored
    filtered_strings = []
    for string in strings:
        if string.startswith("# section") or string.startswith("# note"):
            filtered_strings.append(string)
        elif not ignore_string(string) and string not in filtered_strings:
            filtered_strings.append(string)
    strings = filtered_strings

    strings = remove_unnecessary_section_markers(strings)

    add_strings_to_output(filename, strings, output)

    if canonicalised:
        # We will store in canonical form. Other forms will be auto-generated from that.
        # Add other forms to ignore list so they don't get picked up anywhere else.
        for string in strings:
            if re.search("^(the|a|an) ", string) and not string.endswith("'s"):
                string2 = re.sub("^(the|a|an) ", "", string)
                IGNORE_STRINGS.append(string2)
                if string.startswith("the "):
                    IGNORE_STRINGS.append(article_a(string2))

for line in output:
    print(line)
