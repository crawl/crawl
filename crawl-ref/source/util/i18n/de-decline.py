###############################################################################
#
# German: Generate dative from nominative
#
###############################################################################
import os;
import re;
import sys;
from enum import Enum

class Gender(Enum):
    UNKNOWN = 0
    MASCULINE = 1
    FEMININE = 2
    NEUTER = 3
    PLURAL = 4

class Case(Enum):
    NOMINATIVE = 0
    ACCUSATIVE = 1
    DATIVE = 2
    GENITIVE = 3

class Declension(Enum):
    STRONG = 1
    MIXED = 2
    WEAK = 3

def writeline(file, line):
    file.write(line)
    file.write('\n')

def is_plural(english):
    if english.startswith('%d '):
        return True
    if re.match(r'^[A-Z]', english):
        # probably a proper name like Agnes
        return False

    # remove suffix so that something like "servant of whispers" is not
    # erroneously recognised as plural
    english = re.sub(r' of .*$', '', english)
    if english.endswith('s'):
        if english.endswith('ss'):
            return False
        elif english.endswith('us'):
            if english.endswith('tengus') or english.endswith('bennus'):
                return True
            else:
                # probably a Latin word
                return False
        elif english.endswith('catoblepas'):
            return False
        elif english.endswith('apis'):
            return False
        elif english.endswith('cyclops'):
            return False
        else:
            return True
    else:
        return False

def is_definite_article(word):
    return word.lower() in ['der', 'die', 'das', 'den', 'dem', 'des']

def is_determiner(word):
    if is_definite_article(word):
        return True
    lword = word.lower()
    if lword in ['ein', 'eine', 'einem', 'einer', 'eines']:
        return True
    elif lword in ['euer', 'eure', 'euren', 'eurem', 'eures']:
        return True
    else:
        return False

def starts_with_capital(word):
    return re.match(r'^[A-ZÄÖÜ]', word)

def is_noun(word):
    # special case of adjectives starting with capital
    if word.startswith("Lernäisch") or word.startswith("Verrückt"):
        return False
    # otherwise, words that start with a capital are probably nouns
    return starts_with_capital(word) and not is_determiner(word)

def decline_determiner(word, gender, target_case):
    lword = word.lower()
    result = ""
    if target_case == Case.GENITIVE:
        if lword in ["der", "das"]:
            result = "des"
        elif lword == "die":
            result = "der"
        elif lword == "ein":
            result = "eines"
        elif lword == "eine":
            result = "einer"
        elif lword == "euer":
            result = "eures"
        elif lword == "eure":
            result = "eurer"
    elif target_case == Case.DATIVE:
        if lword in ["der", "das"]:
            result = "dem"
        elif lword == "die":
            result = "den" if gender == Gender.PLURAL else "der"
        elif lword == "ein":
            result = "einem"
        elif lword == "eine":
            result = "einer"
        elif lword == "euer":
            result = "eurem"
        elif lword == "eure":
            result = "euren" if gender == Gender.PLURAL else "eurer"
    elif target_case == Case.ACCUSATIVE and gender == Gender.MASCULINE:
        if lword == "der":
            result = "den"
        elif lword == "ein":
            result = "einen"
        elif lword == "euer":
            result = "euren"

    if result == "":
        return word
    elif starts_with_capital(word):
        return result.capitalize()
    else:
        return result

def decline_noun(word, gender, target_case):
    if gender == Gender.PLURAL and target_case == Case.DATIVE:
        if not re.search(r'[nsi]$', word):
            return word + "n"

    if gender in [Gender.FEMININE, Gender.PLURAL]:
        return word
 
    # masculine, neuter or unknown
    result = word
    if gender != Gender.NEUTER:
        # weak masculine nouns are declined in all cases except nominative
        result = re.sub(r'(Prinz|Mensch|[Bb]är)$', r'\1en', result)
        result = re.sub(r'([Ss]chamane|[Ss]chütze|Sklave)$', r'\1n', result)
        result = re.sub(r'([Rr]iese|[Aa]ffe|[Dd]rache)$', r'\1n', result)
        result = re.sub(r'([Hh]err)$', r'\1n', result)
        result = re.sub(r'([Dd]ruide|[Hh]eilige|Ahne)$', r'\1n', result)
        result = re.sub(r'([^e]ist)$', r'\1en', result)
        result = re.sub(r'(ant|myzet)$', r'\1en', result)
        result = re.sub(r'(loge|mycete)$', r'\1n', result)

    # if genitive and not changed above then do the standard change
    if target_case == Case.GENITIVE and result == word:
        if word.endswith("ss"):
            result = word + "es"
        elif not word.endswith("s"):
            result = word + "s"

    return result

def decline_adjective(word, gender, target_case, declension):
    if target_case in [Case.DATIVE, Case.GENITIVE]:
        if declension != Declension.STRONG:
            # weak/mixed declension: always -en
            word = re.sub(r'e[rs]?$', 'en', word)
        elif gender == Gender.FEMININE:
            # feminine strong: -er in both cases
            word = re.sub(r'e$', 'er', word)
        elif gender == Gender.PLURAL:
            # plural strong
            if target_case == Case.GENITIVE:
                word = re.sub(r'e$', 'er', word)
            else:
                word = re.sub(r'e$', 'en', word)
        else:
            # masc/neuter strong
            if target_case == Case.GENITIVE:
                word = re.sub(r'e[rs]$', 'en', word)
            else:
                word = re.sub(r'e[rs]$', 'em', word)
    elif target_case == Case.ACCUSATIVE and gender == Gender.MASCULINE:
        # ending is always -en regardless of declension
        word = re.sub(r'er?$', 'en', word)

    return word

# process command line args
if len(sys.argv) != 4:
    sys.exit("Usage: de-decline.py acc|dat|gen <infile> <outfile>")
case_str = sys.argv[1]
target_case = Case.DATIVE
if case_str.startswith('acc') or case_str.startswith('akk'):
    target_case = Case.ACCUSATIVE
elif case_str.startswith('gen'):
    target_case = Case.GENITIVE
infile_name = sys.argv[2];
outfile_name = sys.argv[3];
print("Reading from {} and writing to {}".format(infile_name, outfile_name))

infile = open(infile_name, "r")
outfile = open(outfile_name, "w")

lines = infile.readlines()
last_written = ""
english = ""
known_gender = {}
for line in lines:
    line = line.strip('\n\r')
    if re.match(r'\s*$', line):
        # blank line
        writeline(outfile, line)
        last_written = line
    if re.match(r'%%%%', line):
        # start/end entry
        english = ""
        if last_written != line:
            writeline(outfile, line)
            last_written = line
    elif re.match(r'\s*#', line):
        # comments
        if target_case == Case.DATIVE:
            outline = line.replace('nominative', 'dative (where different to nominative)')
        elif target_case == Case.ACCUSATIVE:
            outline = line.replace('nominative', 'accusative (where different to nominative)')
        elif target_case == Case.GENITIVE:
            outline = line.replace('nominative', 'genitive (where different to nominative)')
        writeline(outfile, outline)
        last_written = outline
    elif english == "":
        # save the English key
        english = line
    else:
        # Now we have the German and the English strings, so we can process
        german = line

        # extract and save any suffix (" of whatever"), which has its own case
        suffix = ""
        m = re.search(r' (von|der|des) .*$', german)
        if m:
            suffix = m.group(0)
            if suffix == " der Ork":
                # special case where article of main noun is not at the start
                suffix = ""
            elif suffix != "":
                german = german.replace(suffix, '')
        
        # split german up into individual words
        words = german.split()

        # try to determine gender and declension
        gender = Gender.UNKNOWN;
        declension = Declension.STRONG
        if english.startswith('%d '):
            gender = Gender.PLURAL
            declension = Declension.STRONG

        i = 0
        while gender == Gender.UNKNOWN and i < len(words):
            word = words[i]
            if (is_determiner(word)):
                if is_definite_article(word):
                    declension = Declension.WEAK
                else:
                    declension = Declension.MIXED
                lword = word.lower()
                if lword == 'der':
                    gender = Gender.MASCULINE
                elif lword == 'eine':
                    gender = Gender.FEMININE
                elif lword == 'das':
                    gender = Gender.NEUTER
                elif lword in ['die', 'eure']:
                    if is_plural(english):
                        gender = Gender.PLURAL
                    else:
                        gender = Gender.FEMININE
            elif not is_noun(word):
                # probably an adjective, so check for gendered ending
                if word.endswith("er"):
                    gender = Gender.MASCULINE
                elif word.endswith("es"):
                    gender = Gender.NEUTER
                elif word.endswith("e"):
                    if is_plural(english):
                        gender = Gender.PLURAL
                    else:
                        gender = Gender.FEMININE
            i += 1

        if gender == Gender.UNKNOWN and is_plural(english):
            gender = Gender.PLURAL

        # save/lookup known genders
        key = german
        key = re.sub('^(der|die|das) ', '', key)
        key = re.sub('^(eine|ein) ', '', key)
        key = re.sub('^(Euer|Eure) ', '', key)
        if gender == Gender.UNKNOWN:
            if key in known_gender:
                gender = known_gender[key]
        else:
            known_gender[key] = gender

        # decline
        german = ""
        for word in words:
            if is_determiner(word):
                word = decline_determiner(word, gender, target_case)
            elif is_noun(word):
                word = decline_noun(word, gender, target_case)
            else:
                word = decline_adjective(word, gender, target_case, declension)
            
            if german != "":
                german += " "
            german += word

        # restore saved suffix
        german += suffix

        if german != line:
            if target_case == Case.ACCUSATIVE:
                outfile.write('{acc}')
            if target_case == Case.DATIVE:
                outfile.write('{dat}')
            if target_case == Case.GENITIVE:
                outfile.write('{gen}')
            writeline(outfile, english)
            writeline(outfile, german)
            last_written = german
        
        english = ""
