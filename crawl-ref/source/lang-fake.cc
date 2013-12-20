/**
 * @file
 * @brief Translation filters.
 **/

#include "AppHdr.h"
#include "lang-fake.h"

#include "libutil.h"
#include "options.h"
#include "unicode.h"

#define UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define LOWER "abcdefghijklmnopqrstuvwxyz"
#define LETTERS UPPER LOWER

// Any resemblance to the speech of a particular character from Rich Burlew's
// work is entirely accidental.  Suggestion that this table has been produced
// by transcribing the whole corpus of that character's speech and making it
// produce the same output from regular English must be dismissed as a rumour.

// For Perl version, "apt-get install filters".
static const char* dwarven[][4] =
{
  {"^yes$","aye"},            {"there","thar"},             {"eir$","ar"},
  {"about","aboot"},          {"^he$","'e"},                {"them","'em"},
  {"^him","'im"},             {"out of$","outta"},          {"of course","'course"},
  {"^of$","o'"},              {"^and$","an'"},              {"to$","ta"},
  {"tog","tag"},              {"that","tha"},               {"the","tha"},
  {"wouldn't","wouldn'ta"},   {"cannot","cannae"},          {"can't","cannae"},
  {"don't","dinnae"},         {"'re$","r"},                 {"for$","fer"},
  {"ver$","'er"},             {"ber$","b'r"},               {"every$","ev'ry"},
  {"^is$","be"},
  {"en$","'n"},               {"^if$","if'n"},              {"enl","'nl"},
  {"eng","'ng"},              {"ing","in'"},                {"ment","mn't"},
  {"^es","'s"},               {"^ex","'s"},                 {"^not$","na"},
  {"^no$","nay"},             {"n't have","n'tve"},
  {"^are$","be"},             {"have","haf"},               {"abl","'bl"},
  {"^you$","ye"},             {"^your","yer"},              {"^you'","ye'"},
  {"noth","nuth"},            {"^this$","'tis"},            {"^here","'ere"},
  {"doesn't","don't"},        {"at a$","atta"},             {"ith$","it'"},
  {"ered$","'red"},           {"into$","inta"},             {"^before","'fore"},
  {"wit' '","wit '"},         {"wit' t","wit t"},           {"wit' w","wit w"},
  {"wit' y","wit y"},         {"get a","git a"},            {"ally$","'lly"},
  {"^my","me"},               {"^i think$","methinks"},     {"nay w","na w"},
  {"^one$","'un"},            {"^'un a","one a"},           {"at ta$","atta"},
  {"ot ta$","otta"},          {"^isn't$","ain't"},          {"^so th","s'th"},
  {"ned$","n'd"},             {"^because","'cause"},
  {0}
};

// Resemblances to the manner of speech of Jägermonsters from "Girl Genius" by
// Phil and Kaja Foglio are... hrm dammit, using the proper name gives it out.
// And s/dammit/gotterdammerung/.
static const char* jager[][4] =
{
  {"this","dis"},
  {"that","dat"},
  {"they","dey"},
  {"their","der"},
  {"there","dere"},
  {"these","dese"},
  {"those","doze"},
  {"the$","de"},
  {"then","den"},
  {"them","dem"},
  {"yes","yah"},
  {"you","hyu"},
  {"hyur","you"},
  {"have","hef"},
  {"wh","v"},
  {"^w","v"},
  {"w","v", "o"},
  {"^ha","he"},
  {"ha","he", "c"},
  {"any","henny"},
  {"smart","schmot"},
  {"n't","n'"},
  {"ain'","hain't"},
  {"am not","hain't"},
  {"isn'","hain't"},
  {"is not","hain't"},
  {"aren'","hain't"},
  {"are not","haint't"},
  {"good","goot"},
  {"dad","papa"},
  {"stuff","stoff"},
  {"going to","gunna"},
  {"going","gun"},
  {"people","pipple"},
  {"little","liddle"},
  {"love","luv"},
  {"nice","nize"},
  {"bug","bog"},
  {"let me","lemme"},
  {"give me","gimmee"},
  {"castle","kestle"},
  {"clank","klenk"},
  {"better","betta"},
  {"darling","dollink"},
  {"some","zum"},
  {"ouble","ubbel"},
  {"bble","bbel"},
  {"^ex","hex"},
  {"exc","eks"},
  {"ex","eks"},
  {"accident","exident"},
  {"once","vunce"},
  {"id$","eed"},
  {"^one","vun"},
  {"kiss","kees"},
  {"^c","k", 0, "h"},
  {"am$","em"},
  {"^th","t"},
  {"th","dd", "!" LETTERS, "!" LETTERS},
  {"unix","yoonix"},
  {"^un","on"},
  {"^sh","sch", 0, "e"},
  {"^sl","schl"},
  {"^sm","schm"},
  {"^sn","schn"},
  {"^sp","schp"},
  {"^st","scht"},
  {"of","uf"},
  {"qu","qv"},
  {"ing","ink"},
  {"irl","url"},
  {"ish","eesh"},
  {"^i","hy", 0, ")" LETTERS},
  {"ve$","ff", "!" LETTERS},
  {"ect$","eck"},
  {"and","und"},
  {"^all","hall"},
  {"mister","herr"},
  {"mr.","herr"},
  {"^is$","iz"},
  {"god damn it","gotterdammerung"},
  {"goddammit","gotterdammerung"},
  {"goddamnit","gotterdammerung"},
  {"damn it","gotterdammerung"},
  {"goddam","gotterdammerung"},
  {"god damn","gotterdammerung"},
  {"god damn it","gotterdammerung"},
  {"damn","dem"},
  {"god","gott"},
  {"okay","hokay"},
  {"^ok$","hokay"},
  {"just","chust"},
  {0}
};

static const char* cyrillic[][4] =
{
  {"a",   "а"},
  {"b",   "б"},
  {"v",   "в"},
  {"wr",  "р"},
  {"w",   "у"},
  {"g",   "г"},
  {"d",   "д"},
  {"ye",  "е"},
  {"ie",  "е"},
  {"eu",  "ев"},
  {"e",   "е"},
  {"io",  "ё"},
  {"yo",  "ё"},
  {"zh",  "ж"},
  {"z",   "з"},
  {"j",   "й"},
  {"x",   "кс"},
  {"q",   "к"},
  {"l",   "л"},
  {"m",   "м"},
  {"n",   "н"},
  {"o",   "о"},
  {"p",   "п"},
  {"rr",  "р"},
  {"r",   "р"},
  {"s",   "с"},
  {"t",   "т"},
  {"u",   "у"},
  {"f",   "ф"},
  {"kh",  "х"},
  {"h",   "х"},
  {"k",   "к"},
  {"ts",  "ц"},
  {"chr", "кр"},
  {"ch",  "ч"},
  {"c",   "ц"},
  {"shch","щ"},
  {"sh",  "ш"},
  {"e",   "э"},
  {"yu",  "ю"},
  {"iu",  "ю"},
  {"ya",  "я"},
  {"ia",  "я"},
  {"i",   "и"},
  {"y",   "ы"},
  {0}
};

static const char* runes[][4] =
{
  {"ae",  "ᛇ"},
  {"a",   "ᚨ"},
  {"b",   "ᛒ"},
  {"c",   "ᚲ"},
  {"d",   "ᛞ"},
  {"e",   "ᛖ"},
  {"f",   "ᚠ"},
  {"g",   "ᚷ"},
  {"h",   "ᚺ"},
  {"i",   "ᛁ"},
  {"j",   "ᛃ"},
  {"k",   "ᚲ"},
  {"l",   "ᛚ"},
  {"m",   "ᛗ"},
  {"ng",  "ᛜ"},
  {"n",   "ᚾ"},
  {"o",   "ᛟ"},
  {"ph",  "ᚠ"},
  {"p",   "ᛈ"},
  {"q",   "ᛩ"},
  {"r",   "ᚱ"},
  {"s",   "ᛊ"},
  {"th",  "ᚦ"},
  {"t",   "ᛏ"},
  {"u",   "ᚢ"},
  {"v",   "ᚡ"},
  {"w",   "ᚹ"},
  {"x",   "ᚲᛊ"},
  {"y",   "ᚤ"}, // Anglo-Saxon, sometimes medieval
  {"z",   "ᛉ"},
  {0}
};

static void _replace_cap_variants(string &str,
                                  string a,
                                  string b,
                                  const char* not_after = 0,
                                  const char* not_before = 0)
{
    ASSERT(!a.empty());
    if (a[0] == '^')
    {
        a.erase(0, 1);
        ASSERT(!not_after);
        not_after = LETTERS;
    }

    ASSERT(!a.empty());
    if (a[a.length() - 1] == '$')
    {
        a.erase(a.length() - 1);
        ASSERT(!not_before);
        not_before = LETTERS;
    }

    ASSERT(!a.empty());

    bool yes_after = false;
    if (not_after && *not_after == '!')
        yes_after = true, not_after++;

    bool yes_before = false;
    if (not_before && *not_before == '!')
        yes_before = true, not_before++;

    for (int captype = 0; captype < 3; captype++)
    {
        string A;
        switch (captype)
        {
        case 0: A = lowercase(a); break;
        case 1: A = uppercase_first(a); break;
        case 2: A = uppercase(a); break;
        default: die("lolwut?");
        }

        size_t pos = 0;
        while ((pos = str.find(A, pos)) != string::npos)
        {
            if (not_after && pos > 0
                && (yes_after == !strchr(not_after, str[pos - 1])))
            {
                pos++;
                continue;
            }

            size_t epos = pos + A.length();
            if (not_before && epos < str.length()
                && (yes_before == !strchr(not_before, str[epos])))
            {
                pos++;
                continue;
            }

            string B;
            switch (captype)
            {
            case 0: B = lowercase(b); break;
            case 1: B = uppercase_first(b); break;
            case 2: B = uppercase(b); break;
            default: die("lolwut?");
            }

            str.erase(pos, A.length());
            str.insert(pos, B);
            pos += B.length();
        }
    }
}

static void _german(string &txt)
{
    /*
    The European Commission has just announced an agreement whereby English will
    be the official language of the European Union rather than German, which was
    the other possibility.

    As part of the negotiations with the Germans, the British Government
    conceded that English spelling had some room for improvement and has
    accepted a 5-year phase-in plan that would become known as "Euro-English".

    In the first year, "s" will replace the soft "c". Sertainly, this will make
    the sivil servants jump with joy.

    The hard "c" will be dropped in favour of "k". This should klear up
    konfusion, and keyboards kan have one less letter.
    */
    for (unsigned int i = 0; i < txt.length(); i++)
        if (txt[i] == 'c' || txt[i] == 'C')
            switch (tolower(txt[i+1]))
            {
            case 'h': case 'z':
                break;
            case 'e': case 'i': case 'y':
                txt[i] = isalower(txt[i]) ? 's' : 'S';
                break;
            default:
                txt[i] = isalower(txt[i]) ? 'k' : 'K';
            }
    /*
    There will be growing publik enthusiasm in the sekond year when the
    troublesome "ph" will be replaced with "f". This will make words like
    fotograf 20% shorter.
    */
    _replace_cap_variants(txt, "ph", "f");
    /*
    In the 3rd year, publik akseptanse of the new spelling kan be expekted to
    reach the stage where more komplikated changes are possible.

    Governments will enkourage the removal of double letters which have always
    ben a deterent to akurate speling.
    */
    for (int i = txt.length() - 2; i > 0; i--)
        if (txt[i] == txt[i + 1])
            txt.erase(i, 1);
    /*
    Also, al wil agre that the horibl mes of the silent "e" in the languag is
    disgrasful and it should go away.
    */
    for (int i = txt.length() - 2; i > 0; i--)
        if (txt[i]=='e' || txt[i]=='E')
        {
            if (i < 3)
                continue;
            if (isaalpha(txt[i + 1]))
                continue;
            if (txt[i-1]=='e' || txt[i-1]=='E')
                continue;
            if (!isaalpha(txt[i-1])
              || !isaalpha(txt[i-2])
              || !isaalpha(txt[i-3]))
                continue;
            txt.erase(i, 1);
        }
    /*
    By the 4th yer people wil be reseptiv to steps such as replasing "th" with
    "z" and "w" with "v".
    */
    _replace_cap_variants(txt, "th", "z");
    _replace_cap_variants(txt, "w", "v");
    /*
    During ze fifz yer, ze unesesary "o" kan be dropd from vords containing "ou"
    and after ziz fifz yer, ve vil hav a reil sensibl riten styl.
    */
    _replace_cap_variants(txt, "ou", "u");
    /*
    Zer vil be no mor trubl or difikultis and evrivun vil find it ezi tu
    understand ech oza. Ze drem of a united urop vil finali kum tru.

    Und efter ze fifz yer, ve vil al be speking German like zey vunted in ze
    forst plas.
    */
    // TODO :p
}

static const char* german[][4] =
{
  {"sh",    "sch"},
  {"^is$",  "ist"},
  {"an$",   "ann"},
  {"^and$", "und"},
  {"^one$", "ein"},
  {0}
};

static void _wide(string &txt)
{
    string out;

    for (size_t i = 0; i < txt.length(); i++)
    {
        if (txt[i] == ' ')
            out += "　"; // U+3000 rather than U+FF00
        else if (txt[i] > 32 && txt[i] < 127)
        {
            char buf[4];
            int r = wctoutf8(buf, txt[i] + 0xFF00 - 32);
            for (int j = 0; j < r; j++)
                out.push_back(buf[j]);
        }
        else
            out.push_back(txt[i]);
    }

    txt = out;
}

void filter_lang(string &str)
{
    const char* (*repl)[4];

    switch (Options.lang)
    {
    case LANG_DWARVEN:
        repl = dwarven;
        break;
    case LANG_JAGERKIN:
        repl = jager;
        break;
    case LANG_KRAUT:
        _german(str), repl = german;
        break;
    case LANG_CYRILLIC:
        repl = cyrillic;
        break;
    case LANG_WIDE:
        return _wide(str);
    case LANG_FUTHARK:
        repl = runes;
        break;
    default:
        return;
    }

    for (; **repl; repl++)
        _replace_cap_variants(str, (*repl)[0], (*repl)[1], (*repl)[2], (*repl)[3]);
}

string filtered_lang(string str)
{
    filter_lang(str);
    return str;
}
