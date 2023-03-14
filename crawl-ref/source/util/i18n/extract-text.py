import glob
import re
import sys

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
                return line[0:i-1]
    # no comment - return whole line
    return line

# start of conditionally compiled section that can be skipped (debug or obsolete code)?
def is_skippable_if(line):
    return re.search(r'^\s*#\s*ifdef .*DEBUG', line) or \
       re.search(r'^\s*#\s*ifdef .*VERBOSE', line) or \
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

    return result

SKIP_FILES = [ 
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
    'mon-pathfind.cc', 'ng-init.cc', 'ng-input.cc',
    # error messages that probably aren't worth translating
    'fontwrapper-ft.cc', 'game-options.h', 'game-options.cc', 'glwrapper-ogl.cc',
    'libw32c.h', 'libw32c.cc', 'package.cc',
    # nonsense
    'lang-fake.h', 'lang-fake.cc'
]

# These files define data structures containing strings (normally names of things)
# (stringutil.h is an exception - it contains strings related to list building)
SPECIAL_FILES = [
    'stringutil.h', 'mon-data.h',
    'spl-data.h', 'zap-data.h', 'feature-data.h',
    'item-prop.cc', 'item-name.cc', 'art-data.h', 
    'species-data.h', 'job-data.h', 'form-data.h', 'variant-msg.cc'
]

# These files are evaluated differently. We ignore all strings unless we have a reason to extract them,
# as opposed to extracting all strings unless we have a reason to ignore them.
LAZY_FILES = [
    'dgn-overview.cc', 'end.cc', 'files.cc','fineff.cc', 'god-passive.cc', 
    'god-prayer.cc', 'hiscores.cc', 'macro.cc', 'main.cc'
]

IGNORE_STRINGS = [
    'the', 'the ', ' the ',
    'a', 'a ', 'an', 'an ',
    'you', 'you ', 'your', 'your ',
    'bug', 'null'
]

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

    lazy = (filename in LAZY_FILES)

    strings = []

    with open(filename) as infile:
        data = infile.read()

        data = strip_multiline_comments(data)

        lines_raw = strip_uncompiled(data.splitlines())
        lines = []

        # join split lines and remove comments
        ignoring = False
        in_map = False
        for line in lines_raw:

            # ignore strings explicitly marked as not to be extracted
            if 'noloc' in line:
                if 'noloc section start' in line:
                    ignoring = True
                if 'noloc section end' in line:
                    ignoring = False
                continue

            if ignoring and not 'localise' in line:
                continue

            # remove comment
            if '//' in line and not re.search(r'// *(localise|locnote)', line):
                line = strip_line_comment(line)

            line = line.strip()
            if line == '':
                continue

            # avoid false string delimiter
            line = line.replace("'\"'", "'\\\"'")

            if filename == 'job-data.h':
                # special handling - only take the line with the job abbreviation and name
                if re.search(r'"[A-Z][a-zA-Z]"', line):
                    lines.append(line)
                continue

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

            if len(lines) > 0:        
                last = lines[-1]

                # join strings distributed over several lines
                if line[0] == '"' and last[-1] == '"':
                    lines[-1] = last[0:-1] + line[1:]
                    continue

                # join function calls split over multiple lines (because we want to filter out some function calls)
                if '(' in last:
                    if last[-1] == ',' or last[-1] == ':' or last[-1] == '?' or \
                        last[-1] == '(' or last[-1] == '*' or \
                        line[0] == ',' or line[0] == ':' or line[0] == '?':
                        lines[-1] = last + line
                        continue

            lines.append(line)

        for line in lines:

            if 'locnote' in line:
                note = re.sub(r'^.*locnote: *', '# locnote: ', line)
                strings.append(note)
                line = strip_line_comment(line)
                line = line.strip()

            if '"' not in line:
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
            elif re.search(r'prompt[a-zA-Z_]* *\(', line) or 'yesno' in line \
                 or 'yes_or_no' in line:
                # extract prompts
                extract = True
            elif re.match(r'\s*end *\(', line):
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

                if '"' not in line:
                    continue


            if not extract:
                # if we get here then we are not in lazy mode
                # extract strings unless we have reason to ignore them

                # ignore precompiler directives, except #define
                if line[0] == '#' and not re.match(r'^#\s*define', line):
                    continue

                if re.search('extern +"C"', line):
                    continue

                # ignore debug messages
                if re.search(r'\bdie(_noline)? *\(', line) or \
                   re.search(r'dprf? *\(', line) or \
                   re.search(r'dprintf *\(', line) or \
                   re.search(r'debug_dump_item *\(', line) or \
                   re.search(r'dump_test_fails *\(', line) or \
                   re.search(r'ASSERTM? *\(', line) or \
                   'DEBUG' in line or \
                   'log_print' in line or \
                   re.search(r'fprintf *\(', line):
                    continue

                # ignore axed stuff
                if 'AXED' in line:
                    continue

                # ignore file open (will have file name and mode as strings)
                if re.search(r'\bfopen(_u)? *\(', line):
                    continue

                # ignore lua code
                if 'execfile' in line:
                    continue
                if re.search(r'^[^"]*lua[^"]*\(', line):
                    continue

                # Leave notes/milsones in English
                if re.search('take_note', line) or re.search('mark_milestone', line):
                    continue
                if re.search(r'mutate\s*\(', line):
                    continue
                if re.search(r'\bbanish\s*\(', line):
                    continue

                # score file stuff
                if re.search(r'\bouch\s*\(', line) or re.search(r'\bhurt\s*\(', line) \
                   or re.search(r'\bparalyse\s*\(', line) \
                   or re.search(r'\bpetrify\s*\(', line) \
                   or 'aux_source' in line:
                    continue

                # skip tags/keys
                if re.search(r'^[^"]*_tag\(', line) and not re.search('text_tag', line):
                    continue
                if re.search(r'strip_tag_prefix *\(', line):
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
                if 'show_specific_help' in line:
                    continue
                if 'print_hint' in line:
                    continue
                if re.search('^[^"]*property[A-Za-z_]* *\(', line):
                    continue
                if re.match(r'^\s*key[A-Za-z_]*\.[A-Za-z_]*\(', line):
                    continue
                if re.search(r'set_sync_id\s*\(', line):
                    continue
                if re.search(r'compare_item', line):
                    continue

                # just a find or compare
                if re.search(r'\bstrstr\s*\(', line):
                    continue
                if 'search_stashes' in line:
                    continue
                if re.search(r'\bstrn?i?cmp\b', line):
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

                # strip channel information
                string = re.sub(r'(PLAIN|SOUND|VISUAL|((VISUAL )?WARN|ENCHANT|SPELL)):', '', string)

                if "\\n" in string:
                    # split lines
                    substrings = string.split("\\n")
                    for ss in substrings:
                        strings.append(ss)
                else:
                    strings.append(string)
            

    # filter out strings we want to ignore
    filtered_strings = []
    for string in strings:
        # ignore empty string
        if string == '':
            continue

        if 'locnote' in string:
            filtered_strings.append(string)
            continue

        # ignore articles, pronouns, etc.
        if string.lower() in IGNORE_STRINGS:
            continue

        # ignore strings that are just whitespace
        if re.match(r'^(\\n|\s)*$', string):
            continue

        # ignore opengl functions
        if re.match(r'^gl[A-Z]', string):
            continue
        
        # ignore HTML and formatted text tags
        if re.match(r'^(\\n|\s)*</?[^<>/]+>(\\n|\s)*$', string):
            continue

        # ignore variable names
        if re.match(r'^(\\n|\s)*@[A-Za-z0-9_]+@?(\\n|\s)*$', string):
            continue

        # ignore identifiers
        if '_' in string and re.match(r'^[A-Za-z0-9_\- ]+$', string):
            continue
        if 'Gozag bribe' in string or 'Gozag permabribe' in string:
            continue

        # ignore filenames and file extensions
        if re.match(r'^[A-Za-z0-9_\-\/]*\.[A-Za-z]{1,4}$', string):
            continue

        # ignore format strings without any actual text
        temp = re.sub(r'%[\-\+ #0]?[\*0-9]*(\.[\*0-9]*)?(hh|h|l|ll|j|z|t|L)?[diuoxXfFeEgGaAcspn]', '', string)
        if not re.search('[a-zA-Z]', temp):
            continue

        # ignore punctuation
        if re.match(r'^[!\.\?]+$', string):
            continue

        # ignore bug-catching stuff
        if 'INVALID' in string or re.search(r'bugg(il)?y', string, re.I) or \
           'DUMMY' in string or 'eggplant' in string or \
           re.search(r'bugginess', string, re.I):
            continue

        filtered_strings.append(string)

    if len(filtered_strings) > 0:
        output.append("")
        output.append("##################")
        output.append("# " + filename)
        output.append("##################")
        for string in filtered_strings:
            # in some cases, string needs to be quoted
            #   - if it has leading or trailing whitespace
            #   - if it starts with # (because otherwise it looks like a comment)
            #   - if it starts and ends with double-quotes
            if '# locnote' in string:
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

for line in output:
    print(line)

