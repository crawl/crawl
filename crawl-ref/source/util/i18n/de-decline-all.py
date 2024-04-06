###############################################################################
#
# German declensions
# Produce accusative/dative/genitive files from nominative file
# Usage: de-decline.py <input file>
#
###############################################################################
import logging;
import os;
import re;
import sys;
from enum import Enum

logging.basicConfig(level=logging.WARNING)

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
        if re.search(r"[s'%]s$", english):
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
    word = re.sub(r'%s\-?', '', word)
    word = re.sub(r'\{[^\}]*\}', '', word)
    # special case of adjectives starting with capital
    if word.startswith("Lernäisch") or word.startswith("Verrückt"):
        return False
    elif word.lower() == "etwas":
        # really a pronoun, but we wnat to treat as a noun
        return True
    # otherwise, words that start with a capital are probably nouns
    return starts_with_capital(word) and not is_determiner(word)

definite_articles = \
{
    Gender.MASCULINE: ["der", "den", "dem", "des"],
    Gender.FEMININE: ["die", "die", "der", "der"],
    Gender.NEUTER: ["das", "das", "dem", "des"],
    Gender.PLURAL: ["die", "die", "den", "der"],
}

indefinite_articles = \
{
    Gender.MASCULINE: ["ein", "einen", "einem", "eines"],
    Gender.FEMININE: ["eine", "eine", "einer", "einer"],
    Gender.NEUTER: ["ein", "ein", "einem", "eines"],
    Gender.PLURAL: ["", "", "", ""],
}

yours = \
{
    Gender.MASCULINE: ["Euer", "Euren", "Eurem", "Eures"],
    Gender.FEMININE: ["Eure", "Eure", "Eurer", "Eurer"],
    Gender.NEUTER: ["Euer", "Euer", "Eurem", "Eures"],
    Gender.PLURAL: ["Eure", "Eure", "Euren", "Eurer"],
}

def decline_determiner(word, gender, target_case):
    if word == "the":
        return definite_articles[gender][target_case.value]
    elif word == "a":
        return indefinite_articles[gender][target_case.value]
    elif word == "your":
        return yours[gender][target_case.value]
    else:
        return word

def decline_noun(word, gender, target_case, proper):
    logging.debug("decline_noun start: " + word)

    if target_case == Case.NOMINATIVE:
        return word

    if gender == Gender.PLURAL and target_case == Case.DATIVE:
        if not re.search(r'[nsai]$', word):
            logging.debug("decline_noun: add -n to dative plural")
            return word + "n"

    if gender == Gender.PLURAL or gender == Gender.FEMININE:
        return word

    if target_case == Case.GENITIVE and word.lower() == "etwas":
        return "des Etwas"

    if gender == Gender.NEUTER and word.endswith('herz'):
        if target_case == Case.GENITIVE:
            return word + "ens"
        elif target_case == Case.DATIVE:
            return word + "en"
        else:
            return word

    result = word
    if gender == Gender.MASCULINE:
        # weak masculine nouns are declined in all cases except nominative
        result = re.sub(r'(Prinz|Mensch|[Bb]är)$', r'\1en', result)
        result = re.sub(r'([Ss]chamane|[Ss]chütze|Sklave)$', r'\1n', result)
        result = re.sub(r'([Rr]iese|[Aa]ffe|[Dd]rache)$', r'\1n', result)
        result = re.sub(r'([Hh]err)$', r'\1n', result)
        result = re.sub(r'([Dd]ruide|[Hh]eilige|Ahne)$', r'\1n', result)
        result = re.sub(r'([^e]ist)$', r'\1en', result)
        result = re.sub(r'(ant|taur|myzet)$', r'\1en', result)
        result = re.sub(r'(loge|mycete)$', r'\1n', result)

    if result != word:
        return result

    if target_case == Case.GENITIVE:
        if re.search(r'[Gg]eist$', word):
            result = word + "es"
        # standard genitive change for masculine/neuter
        elif word.endswith("s") and proper:
            result = word + "'"
        elif word.endswith("ss") or word.endswith("sch"):
            result = word + "es"
        elif not word.endswith("s"):
            result = word + "s"

    return result

adj_weak = \
{
    Gender.MASCULINE: ["e", "en", "en", "en"],
    Gender.FEMININE: ["e", "e", "en", "en"],
    Gender.NEUTER: ["e", "e", "en", "en"],
    Gender.PLURAL: ["en", "en", "en", "en"],
}

adj_mixed = \
{
    Gender.MASCULINE: ["er", "en", "en", "en"],
    Gender.FEMININE: ["e", "e", "en", "en"],
    Gender.NEUTER: ["es", "es", "en", "en"],
    Gender.PLURAL: ["en", "en", "en", "en"],
}

adj_strong = \
{
    Gender.MASCULINE: ["er", "en", "em", "en"],
    Gender.FEMININE: ["e", "e", "er", "er"],
    Gender.NEUTER: ["es", "es", "em", "en"],
    Gender.PLURAL: ["e", "e", "en", "er"],
}

def decline_adjective(word, gender, target_case, declension):
    ending = "e"
    if declension == Declension.STRONG:
        ending = adj_strong[gender][target_case.value]
    elif declension == Declension.MIXED:
        ending = adj_mixed[gender][target_case.value]
    elif declension == Declension.WEAK:
        ending = adj_weak[gender][target_case.value]

    word = re.sub(r'e[rs]?$', ending, word)

    return word

# decline adjective context marker
def decline_adj_context(s, gender, target_case, declension):

    if '%ss ' in s or '%s-' in s:
        return s

    context = "adj-en"
    if target_case == Case.DATIVE and declension == Declension.STRONG:
        if gender == Gender.FEMININE:
            context = "adj-er"
        elif gender != Gender.PLURAL:
            context = "adj-em"
    elif target_case == Case.GENITIVE and declension == Declension.STRONG:
        if gender == Gender.FEMININE or gender == Gender.PLURAL:
            context = "adj-er"
    elif target_case == Case.ACCUSATIVE or target_case == Case.NOMINATIVE:
        if gender == Gender.FEMININE:
            context = "adj-e"
        elif gender == Gender.NEUTER:
            if declension == Declension.WEAK:
                context = "adj-e"
            else:
                context = "adj-es"
        elif gender == Gender.PLURAL:
            if declension == Declension.STRONG:
                context = "adj-e"
        elif gender == Gender.MASCULINE and target_case == Case.NOMINATIVE:
            if declension == Declension.WEAK:
                context = "adj-e"
            else:
                context = "adj-er"

    # replace the context marker
    s = re.sub(r'\{adj-[a-z]+\}', '{'+context+'}', s, 1)

    return s

# decline string
def decline(english, german, determiner, target_case):

    if english.startswith('" of ') or english.startswith('of ') \
        or not re.search('[A-Za-z]', english.replace('%s', '')):
        # don't decline this
        return german

    # extract and save any suffix (" of whatever"), which has its own case
    suffix = ""
    m = re.search(r' (mit|von|der|des) .*$', german)
    if m:
        suffix = m.group(0)
        if suffix == " der Ork":
            # special case where article of main noun is not at the start
            suffix = ""
        elif suffix != "":
            german = german.replace(suffix, '')

    # determine declension
    declension = Declension.STRONG
    if determiner == "the":
        declension = Declension.WEAK
    elif determiner == "a" or determiner == "your":
        declension = Declension.MIXED

    # determine gender
    gender = Gender.UNKNOWN;
    if german.startswith(('der ', 'Der ')):
        gender = Gender.MASCULINE
    elif german.startswith(('die ', 'Die ')):
        if is_plural(english):
            gender = Gender.PLURAL
        else:
            gender = Gender.FEMININE
    elif german.startswith(('das ', 'Das')):
        gender = Gender.NEUTER
    elif german.startswith('Blork der '):
        gender = Gender.MASCULINE
    else:
        declension = Declension.STRONG
        if german.startswith('%d '):
            gender = Gender.PLURAL
        elif is_plural(english):
            gender = Gender.PLURAL
        elif german.startswith('Blork') or german.startswith('Prinz'):
            gender = Gender.MASCULINE
        elif german.endswith('Yiuf') or german.endswith('Geist'):
            gender = Gender.MASCULINE
        elif german.startswith('Hexenmeisterin') or german.endswith('Illusion'):
            gender = Gender.FEMININE
        elif german.endswith('Hydra') or german.endswith('schlange'):
            gender = Gender.FEMININE
        else:
            logging.info("Unknown gender for: " + german)
            gender = Gender.MASCULINE

    if gender == Gender.PLURAL and determiner == "":
        german = german.replace('%d ', '')

    # split german up into individual words
    words = german.split()

    logging.debug("Gender is " + gender.name)

    # decline
    proper_noun = (re.match(r'^[A-Z]', english) != None)
    german = ""
    before_noun = True
    for i, word in enumerate(words):
        if before_noun:
            if is_determiner(word):
                word = decline_determiner(determiner.lower(), gender, target_case)
            elif is_noun(word) and not (i < len(words) - 1 and words[i+1] == "Drakonier"):
                if word == "Prinz":
                    # decline noun, but don't stop declining later words
                    word = decline_noun(word, gender, target_case, proper_noun)
                elif word == "Gelée":
                    # don't decline this word or anything following
                    before_noun = False
                elif not proper_noun or i == len(words) - 1:
                    # decline noun but nothing else following
                    word = decline_noun(word, gender, target_case, proper_noun)
                    before_noun = False
            else:
                word = decline_adjective(word, gender, target_case, declension)

        if german != "" and word != "":
            german += " "
        german += word

    german = decline_adj_context(german, gender, target_case, declension)

    # restore saved suffix
    german += suffix

    return german


def decline_file(infile_name, target_case, determiner, english_possessive = False):

    outfile_name = infile_name

    if target_case == Case.ACCUSATIVE:
        outfile_name = outfile_name.replace("-nom", "-acc")
    elif target_case == Case.DATIVE:
        outfile_name = outfile_name.replace("-nom", "-dat")
    elif target_case == Case.GENITIVE:
        if english_possessive:
            outfile_name = outfile_name.replace("-nom", "-gen1")
        else:
            outfile_name = outfile_name.replace("-nom", "-gen2")

    if determiner == "a":
        outfile_name = outfile_name.replace("-def", "-indef")
    elif determiner == "your":
        outfile_name = outfile_name.replace("-def", "-your")
    elif determiner == "":
        outfile_name = outfile_name.replace("-def", "-plain")
        outfile_name = outfile_name.replace("-counted", "-plain")

    print("Reading from {} and writing to {}".format(infile_name, outfile_name))

    if outfile_name == infile_name:
        logging.error("Infile and outfile have the same name. Aborting...")
        return

    infile = open(infile_name, "r")
    outfile = open(outfile_name, "w")

    lines = infile.readlines()
    last_written = ""
    english = ""
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
            if 'check' in line:
                continue

            if target_case == Case.DATIVE:
                outline = line.replace('nominative', 'dative (where different to nominative)')
            elif target_case == Case.ACCUSATIVE:
                outline = line.replace('nominative', 'accusative (where different to nominative)')
            elif target_case == Case.GENITIVE:
                if english_possessive:
                    outline = line.replace('nominative', 'genitive (translating English possessive)')
                else:
                    outline = line.replace('nominative', 'genitive (where different to nominative)')
            else:
                outline = line

            if determiner == "a":
                outline = outline.replace('definite article', 'indefinite article');
            elif determiner == "your":
                outline = outline.replace('definite article', 'your');
            elif determiner == "":
                outline = outline.replace('definite article', 'plain');
                outline = outline.replace('counted', 'plain');

            writeline(outfile, outline)
            last_written = outline
        elif english == "":
            # save the English key
            english = line
        else:
            # Now we have the German and the English strings, so we can process

            if english.startswith("%d "):
                if determiner == "":
                    english = re.sub(r'^%d ', '', english)
                else:
                    english = re.sub(r'^%d ', determiner + ' ', english)
            else:
                if determiner == "":
                    english = re.sub(r'^[Tt]he ', '', english)
                else:
                    english = re.sub(r'^[Tt]he ', determiner + ' ', english)
                    if determiner == "a":
                        english = re.sub(r'^a ([AEIOUaeiou])', r'an \1', english)
                        english = re.sub(r'^an one', r'a one', english)

            if english_possessive:
                # DCSS always adds "'s" even if the noun ends in s
                english += "'s"

            nominative = line
            if determiner != "the" and determiner != "%d":
                nominative = decline(english, nominative, determiner, Case.NOMINATIVE)

            german = ""
            if target_case == Case.NOMINATIVE:
                german = nominative
            else:
                german = decline(english, line, determiner, target_case)

            if target_case == Case.NOMINATIVE or german != nominative:
                if target_case == Case.ACCUSATIVE:
                    outfile.write('{acc}')
                elif target_case == Case.DATIVE:
                    outfile.write('{dat}')
                elif target_case == Case.GENITIVE and not english_possessive:
                    outfile.write('{gen}')
                writeline(outfile, english)
                writeline(outfile, german)
                last_written = german

            english = ""


# process command line args
if len(sys.argv) != 2:
    sys.exit("Usage: de-decline.py <input file>")
infile_name = sys.argv[1];

if 'plural' in infile_name:
    if 'items' in infile_name:
        decline_file(infile_name, Case.NOMINATIVE, "")
        decline_file(infile_name, Case.DATIVE, "")

    decline_file(infile_name, Case.DATIVE, "%d")
elif 'unique' in infile_name:
    decline_file(infile_name, Case.NOMINATIVE, "")
    decline_file(infile_name, Case.ACCUSATIVE, "the")
    decline_file(infile_name, Case.DATIVE, "the")

    if 'monsters' in infile_name:
        decline_file(infile_name, Case.GENITIVE, "the", True)
        decline_file(infile_name, Case.GENITIVE, "the", False)
else:
    decline_file(infile_name, Case.ACCUSATIVE, "the")
    decline_file(infile_name, Case.DATIVE, "the")

    decline_file(infile_name, Case.NOMINATIVE, "")

    if not 'player' in infile_name:
        decline_file(infile_name, Case.NOMINATIVE, "a")
        decline_file(infile_name, Case.ACCUSATIVE, "a")
        decline_file(infile_name, Case.DATIVE, "a")

        decline_file(infile_name, Case.NOMINATIVE, "your")
        decline_file(infile_name, Case.ACCUSATIVE, "your")
        decline_file(infile_name, Case.DATIVE, "your")

    if 'monsters' in infile_name:
        decline_file(infile_name, Case.GENITIVE, "the", True)
        decline_file(infile_name, Case.GENITIVE, "a", True)
        decline_file(infile_name, Case.GENITIVE, "your", True)

        decline_file(infile_name, Case.GENITIVE, "the", False)
        decline_file(infile_name, Case.GENITIVE, "a", False)
        decline_file(infile_name, Case.GENITIVE, "your", False)
