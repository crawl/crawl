################################################################################
#
# This script extracts strings from C++ source code
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
    'the', 'the ', ' the ', 'its ',
    'a', 'a ', 'an', 'an ',
    'you', 'you ', 'your', 'your ',
    'debugging ray', 'debug',
    'bug', 'null',
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
    'species-data.h', 'job-data.h', 'form-data.h', 'variant-msg.cc'
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
    'lang-fake.h', 'lang-fake.cc'
]


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
            if not has_appearance and name != '' and 'DUMMY' not in name:
                if name == 'ring of the Octopus King':
                    result.append('# note: unlike other unrands, ' + name + " is not unique")
                    result.append(article_a('%s' + name))
                else:
                    result.append('# note: appearance of ' + name + " before it's identified")
                    result.append(article_a(name))
            has_appearance = False
            name = line.replace('NAME:', '').strip()
            if 'DUMMY' in name:
                continue
            #result.append('# note: ' + name)
            if re.search('(boots|gloves|gauntlets|quick blades)', name):
                if not 'pair of ' in name:
                    name = 'pair of ' + name
            result.append('%s' + name)
            result.append('the %s' + name)
        elif 'DUMMY' in name:
            continue
        elif line.startswith('APPEAR:'):
            string = line.replace('APPEAR:', '').strip()
            result.append('# note: appearance of ' + name + " before it's identified")
            result.append(string)
            result.append(article_a(string))
        elif line.startswith('TYPE:'):
            string = line.replace('TYPE:', '').strip()
            result.append('# note: base type of ' + name)
            result.append(string)
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
    if string.lower() in IGNORE_STRINGS:
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
    if 'INVALID' in string or re.search(r'bugg(il)?y', string, re.I) or \
       'DUMMY' in string or 'eggplant' in string or \
       re.search(r'bugginess', string, re.I):
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


# special handling for strings in item-name.cc
def special_handling_for_item_name_cc(section, line, string, strings):
    if section in ['_random_vowel', '_random_cons', '_random_consonant_set', 'make_name']:
        # random-name generation - ignore
        return
    elif '_test' in section:
        # test stuff
        return
    elif section.endswith('_secondary_string') or section == 'staff_primary_string':
        # extract adjective as separate word
        if not string.endswith(' '):
            string += ' '
    elif section.endswith('_primary_string'):
        # extract as adjective + noun
        noun = re.sub('_.*', '', section)
        if not string.endswith(' '):
            string += ' '
        string = '%s' + string + noun
    elif section == 'item_def::name':
        if string == ' (in ':
            strings.append(' (in hand)')
            strings.append(' (in claw)')
            strings.append(' (in tentacle)')
            return
        elif string in ['right', 'left']:
            strings.append(' (' + string + ' hand)');
            strings.append(' (' + string + ' claw)');
            strings.append(' (' + string + ' paw)');
            strings.append(' (' + string + ' tentacle)');
            strings.append(' (' + string + ' branch)');
            strings.append(' (' + string + ' front leg)');
            strings.append(' (' + string + ' blade hand)');
            return
    elif section == 'missile_brand_name':
        if string == 'poisoned' or string.endswith('-tipped'):
            # hard code dart types
            string = string + ' dart'
        elif string == 'dispersal':
            string = ' of ' + string
        elif 'MBN_NAME' in line:
            # first string is long name, second is terse
            if not line.endswith('"' + string + '";'):
                string += ' '
        elif 'MBN_TERSE' in line:
            # first string is terse name, second is long name
            if line.endswith('"' + string + '";'):
                string += ' '
        else:
            # string acts as both long and terse name
            if string == 'silver':
                strings.append(string + ' ')
            else:
                strings.append(' of ' + string)
    elif section == 'weapon_brands_terse':
        if string == 'confuse':
            # not a real weapon brand - used on hands for confusing touch
            return
        elif string == 'flame':
            # terse version also used after "of" (see _item_ego_name in religion.cc)
            strings.append(' of ' + string)
    elif section == 'weapon_brands_verbose':
        if string == 'confusion':
            # not a real weapon brand - used on hands for confusing touch
            return
        elif string in ['vampirism', 'antimagic', 'vorpality', 'spectralizing']:
            # verbose name is never used (see brand_prefers_adj)
            return
        string = ' of ' + string
    elif section == 'weapon_brands_adj':
        # adjectives defined for all, but only used for some (see brand_prefers_adj)
        if string in ['vampiric', 'antimagic', 'vorpal', 'spectral']:
            string = string + ' '
        else:
            return
    elif section == 'armour_ego_name':
        string = ' of ' + string
    elif section == 'armour_ego_name(terse)':
        if string == 'rC+ rF+':
            # handled as two separate strings
            return
        # the plus is handled separately
        string = re.sub(r'\+.*', '', string)
    elif section == '_wand_type_name':
        string = 'wand of ' + string
    elif section == 'potion_type_name':
        string = 'potion of ' + string
    elif section == 'scroll_type_name':
        string = 'scroll of ' + string
    elif section == 'jewellery_effect_name':
        if 'AMU_' in line:
            string = '%samulet of ' + string
        else:
            string = '%sring of ' + string
    elif section == 'jewellery_effect_name(terse)':
        # the plus is handled separately
        string = re.sub(r'\+.*', '', string)
    elif section == 'rune_type_name':
        if string in ['mossy', 'elven']:
            # obsolete
            return
        else:
            strings.append(string + ' rune')
    elif section == '_book_type_name':
        if string == 'Fixed Level' or string == 'Fixed Theme':
            return
        string = 'book of ' + string
    elif section == 'staff_type_name':
        string = '%sstaff of ' + string
    elif section == 'ghost_brand_name':
        if string == '%s weapon':
            string = '%sweapon'
        elif string == 'weapon of %s':
            return
        elif string == '%s touch':
            # there's only one possibility
            string = 'confusing touch'
    elif section == 'potion_colours':
        if not string.endswith(' '):
            string += ' '
    elif section == 'display_runes':
        if string == "green":
            # text colour tag
            return
    elif section == 'item_prefix':
        # undisplayed, but (supposedly) searchable prefixes
        # many of these don't even work in English
        return

    if string in ['wand of ', 'potion of ', 'scroll of ', 'ring of', 'amulet of', 'staff of ', 'book of ']:
        # all subtypes already covered above
        return
    elif string in [' wand', ' potion', ' ring', ' amulet', ' rune']:
        # all subtypes already covered above
        return
    elif string in ['manual of ', '%s of %s', ' of ', 'of '] or (string.endswith(' of Zot') and string != "The Orb of Zot"):
        # other "of <foo>" suffixes are handled separately
        return
    elif string == 'figurine of a ziggurat':
        string = '%s' + string
    elif string == 'enchanted %s':
        # will be handled the other way round, with "enchanted" as added adjective
        string = 'enchanted '
    elif string == "damnation ":
        # there's only one possibility
        string = "damnation bolt"
    elif string == "labelled ":
        string = "scroll labelled %s"
    elif string == "x) ":
        # ignore - just used for size
        return
    elif string == "pair of ":
        # handled in item-prop.cc
        return
    elif string == "decaying skeleton":
        # dbname (just used as a lookup key, not displayed)
        return
    elif "bug" in string or "bad item" in string or "bogus" in string:
        # case that should never happen - ignore
        return

    strings.append(string)


# get rid of unnecessary section markers
def remove_unnecessary_section_markers(strings):
    section = None
    strings_temp = []
    for string in strings:
        if string.startswith('# section:'):
            section = string
        else:
            if section is not None:
                strings_temp.append(section)
                section = None
            strings_temp.append(string)
    strings.clear()
    strings.extend(strings_temp)

def article_a(string):
    if re.search('^[aeiouAEIOU]', string) and not string.startswith('one-'):
        return "an " + string
    else:
        return "a " + string

def is_unique_monster(string):
    if not re.search('[A-Z]', string):
        return False
    elif string in ['Killer Klown', 'Orb Guardian', 'Brimstone Fiend', 'Ice Fiend', 'Tzitzimitl', 'Hell Sentinel', 'Executioner', 'Hellbinder', 'Cloud Mage']:
        return False
    else:
        return True


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

    # we need to add extra strings for names of monsters/features/items
    # in addition to "foo", we need "the foo", "a foo", "your foo"
    # for monsters, we also need possessives ("the foo's", "a foo's", "your foo's")
    if filename in ['mon-data.h', 'feature-data.h', 'item-prop.cc', 'item-name.cc']:

        # separate unique and non-unique names because they will be treated differently
        names = []
        unique_names = []
        adjectives = []
        for string in strings:
            if string.startswith('# note:') or string.startswith('# section:'):
                continue
            elif string.endswith(' '):
                adjectives.append(string)
            elif filename == 'item-name.cc':
                if ' of ' in string and re.search('^[a-z]', string) and 'Geryon' not in string:
                    names.append(string)
                elif string.startswith('%s') or re.search(' (dart|bolt|rune)$', string):
                    names.append(string)
                elif string in ['lightning rod', 'quad damage', 'phantom mirror', 'condenser vane', "piece from Xom's chessboard"]:
                    names.append(string)
            elif (filename == 'mon-data.h' and is_unique_monster(string)):
                unique_names.append(string)
            elif not re.search('^(a|an|the|some) ', string) and string not in ['explore horizon', 'unseen']:
                names.append(string)

        # names prefixed with definite article (the)
        for string in names:
            output.append("the " + string)

        # names prefixed with indefinite article (a/an)
        for string in names:
            if string not in ['lava', 'shallow water', 'deep water']:
                output.append(article_a(string))

        # names prefixed with "your"
        if filename in ['mon-data.h', 'item-prop.cc']:
            for string in names:
                output.append("your " + string)

        # possessives
        if filename == 'mon-data.h':
            # possessives with definite article (the)
            for string in names:
                output.append("the " + string + "'s")

            # possessives with indefinite article (a/an)
            for string in names:
                output.append(article_a(string) + "'s")

            # possessives with "your"
            for string in names:
                output.append("your " + string + "'s")

            # possessives for unique monsters
            for string in unique_names:
                output.append(string + "'s")


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

    # put some important files first
    # (because if there are duplicate strings, we want them put under these files)
    files = SPECIAL_FILES.copy()

    # add wanted source files to list to be processed
    for fname in source_files:
        if fname not in files and \
           fname not in SKIP_FILES and \
           not re.match('l-', fname) and \
           not re.match('dbg-', fname):
            files.append(fname)

output = []

for filename in files:

    if filename == 'art-data.txt':
        strings = process_art_data_txt()
        add_strings_to_output(filename, strings, output)
        continue

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

        if filename == 'delay.cc':
            # ignore internal identifiers
            if section == 'activity_interrupt_names':
                continue
        elif filename == 'lookup-help.cc':
            # ignore db keys
            if re.match(r'^_(get|recap)[a-z_]*keys?$', section):
                continue

        extract = False

        if 'localise' in line:
            extract = True
        elif 'simple_monster_message' in line or 'simple_god_message' in line:
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
               re.search(r'debug_dump_item *\(', line) or \
               re.search(r'dump_test_fails *\(', line) or \
               re.search(r'bad_level_id', line) or \
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
            if 'serialize' in line:
                continue
            if '_id =' in line:
                continue
            if 'push_ui_layout' in line or 'ui_state_change' in line:
                continue
            if re.search(r'\bmenu_colour *\(', line):
                continue
            if re.search(r'\bprops\.erase *\(', line):
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
            if 'simple_god_message' in line or 'simple_monster_message' in line:
                if string != "" and (string[0] == " " or string[0] == "'"):
                    string = '%s' + string

            if section != last_section:
                strings.append('# section: ' + section)
                last_section = section

            if filename == 'item-name.cc':
                special_handling_for_item_name_cc(section, line, string, strings)
                continue
            elif filename == 'mon-util.cc' and section in ['ugly_colour_names', 'drac_colour_names']:
                # adjectives
                string += ' '
            elif filename == 'output.cc':
                if section == 's_equip_slot_names' or section == 'equip_slot_to_name':
                    # equipment slots are only ever displayed in lowercase form
                    # and the specific ring slots are all just displayed as "ring"
                    if string.endswith(" Ring"):
                        string = "ring"
                    else:
                        string = string.lower()

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

    # filter out strings we want to ignore
    filtered_strings = []
    for string in strings:

        if string.startswith('# note:') or string.startswith('# section:'):
            filtered_strings.append(string)
            continue

        if ignore_string(string):
            continue

        # some names have adjectives added for display
        if filename == 'mon-data.h':
            if string in ['ugly thing', 'very ugly thing', 'slime creature', 'hydra']:
                string = '%s' + string
            elif string == 'the Lernaean hydra':
                string = 'the %sLernaean hydra'
        elif filename == 'feature-data.h':
            # we handle door adjectives as separate strings
            if string.endswith(' door'):
                words = string.split()
                for word in words:
                    if word == 'door':
                        word = '%s' + word
                    else:
                        word = word + ' '
                    if not word in filtered_strings:
                        filtered_strings.append(word);
                continue
            elif string.startswith('some '):
                string2 = re.sub('^some ', '', string)
                if string2 not in filtered_strings:
                    filtered_strings.append(string2)
        elif filename == 'item-prop.cc':
            if string in ['steam', 'acid', 'quicksilver', 'swamp', 'fire', 'ice', 'pearl', 'storm', 'shadow', 'gold']:
                string = '%s' + string + ' dragon scales'
            elif string == ' dragon scales':
                continue
            elif string in ['gloves', 'boots']:
                string = '%spair of %s' + string
            elif string in ['javelin', 'boomerang']:
                filtered_strings.append(string)
                filtered_strings.append('silver ' + string)
                continue
            elif string not in ['dart', 'stone', 'arrow', 'bolt', 'large rock', 'sling bullet', 'throwing net']:
                string = '%s' + string
        else:
            # this should be already covered above (feature-data.h)
            if string == 'runed door' and '"runed "' in output and '%sdoor' in output:
                continue

        if string not in filtered_strings:
            filtered_strings.append(string)

    remove_unnecessary_section_markers(filtered_strings)

    add_strings_to_output(filename, filtered_strings, output)

for line in output:
    print(line)
