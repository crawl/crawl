/*
 * localise.h
 * High-level localisation functions
 */

#pragma once

#include <string>
using std::string;

#include <vector>
using std::vector;
/*
 * Structure describing a localisation argument
 */
struct LocalisationArg
{
public:
    string stringVal;
    string plural;

    // count of items, etc.
    int count;

    int intVal;
    long longVal;
    long long longLongVal;
    double doubleVal;
    long double longDoubleVal;

    // should this argument be translated? (defaults to true)
    bool translate;

    LocalisationArg();
    LocalisationArg(const string& value, bool translate = true);
    LocalisationArg(const char* value, bool translate = true);
    LocalisationArg(const string& value, const string& plural_val, const int count, bool translate = true);
    LocalisationArg(const int value, bool translate = true);
    LocalisationArg(const long value, bool translate = true);
    LocalisationArg(const long long value, bool translate = true);
    LocalisationArg(const double value, bool translate = true);
    LocalisationArg(const long double value, bool translate = true);

private:
    void init();
};

/**
 * Initialize the localisation system
 */
void init_localisation(const string& lang);

/**
 * Temporarily stop translating
 */
void pause_localisation();

/**
 * Resume translating
 */
void unpause_localisation();

// Get the current localisation language
const string& get_localisation_language();

/**
 *  Is localisation active?
 *
 *  @returns true if language is not English and localisation not paused.
 */
bool localisation_active();

/**
 *  Localise a list of args.
 *  @param args The list of arguments.
 *              If there's more than one, the first one is expected to be a printf-style format string.
 */
string localise(const vector<LocalisationArg>& args);

// localise using va_list (yuk!)
string vlocalise(const string& fmt_str, va_list args);

// convenience functions
template<typename... Ts>
static string localise(const string& first, Ts const &... rest)
{
    vector<LocalisationArg> v {first, rest...};
    return localise(v);
}

/**
 * Get the localised equivalent of a single-character
 * (Mostly for prompt answers like Y/N)
 */
int localise_char(char ch);


/**
 * Localise a string with embedded @foo@ style parameters
 */
string localise(const string& text, const map<string, string>& params,
                bool localise_text = true);


/**
 * Localise a string with a specific context
 */
string localise_contextual(const string& context, const string& text_en);

/**
 * Localise a player title
 */
string localise_player_title(const string& text);
