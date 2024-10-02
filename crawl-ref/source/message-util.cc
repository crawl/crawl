/*
 * Message utility functions
 */

#include "AppHdr.h"
#include "english.h"
#include "gender-type.h"
#include "localise.h"
#include "message-util.h"
#include "mpr.h"
#include "stringutil.h"

/*
 * Add punctuation to a message
 */
string add_punctuation(const string& msg, const string& punctuation, bool localise_msg)
{
    // We need to insert message into punctuation rather than merely append
    // because, for example, English "%s!" becomes "¡%s!" in Spanish.
    // Note: If message is already localised, we don't localise again.
    if (contains(punctuation, "%s"))
        return localise(punctuation, LocalisationArg(msg, localise_msg));
    else
        return localise("%s" + punctuation, LocalisationArg(msg, localise_msg));
}

/*
 * Replace all instances of given @foo@ parameter with localised value
 * If first letter of tag is uppercase then value will be capitalised also
 */
string replace_tag(const string& msg, const string& tag, const string& value,
                   bool localise_msg)
{
    string ret = localise_msg ? localise(msg) : msg;

    size_t tag_pos;
    while ((tag_pos = ret.find(tag)) != string::npos)
    {
        size_t replace_pos = tag_pos;
        size_t replace_len = tag.length();

        // check for context qualifier
        string context;
        if (tag_pos > 2 && ret[tag_pos-1] == '}')
        {
            size_t ctxt_pos = ret.rfind('{', tag_pos-1);
            if (ctxt_pos != string::npos)
            {
                context = ret.substr(ctxt_pos, tag_pos - ctxt_pos);
                replace_pos = ctxt_pos;
                replace_len += context.length();
            }
        }

        string val = localise(context + "%s", value);
        if (tag.length() > 1 && isupper(tag[1]))
            val = uppercase_first(val);
        ret.replace(replace_pos, replace_len, val);
    }
    return ret;
}

/* Returns an actor's name
 *
 * Takes into account actor visibility/invisibility and the type of description
 * to be used (definite, indefinite, possessive, etc.)
 */
string actor_name(const actor *a, description_level_type desc,
                  bool actor_visible)
{
    if (!a)
        return "null"; // bug, @noloc
    return actor_visible ? a->name(desc) : anon_name(desc);
}

/* Returns an actor's pronoun
 *
 * Takes into account actor visibility
 */
string actor_pronoun(const actor *a, pronoun_type pron,
                     bool actor_visible)
{
    if (!a)
        return "null"; // bug, @noloc
    return actor_visible ? a->pronoun(pron) : anon_pronoun(pron);
}

/*
 * Returns an anonymous actor's name
 *
 * Given invisibility (whether out of LOS or just invisible),
 * returns the appropriate noun.
 */
string anon_name(description_level_type desc)
{
    switch (desc)
    {
    case DESC_NONE:
        return "";
    case DESC_YOUR:
    case DESC_ITS:
        return "something's";
    case DESC_THE:
    case DESC_A:
    case DESC_PLAIN:
    default:
        return "something";
    }
}

/*
 * Returns an anonymous actor's pronoun
 *
 * Given invisibility (whether out of LOS or just invisible),
 * returns the appropriate pronoun.
 */
string anon_pronoun(pronoun_type pron)
{
    return decline_pronoun(GENDER_NEUTER, pron);
}

// @noloc section start

/*
 * Get message where subject and object can be any combination of 2nd or 3rd person
 * (1st person doesn't exist in this game)
 * Note: subject must be singular
 */
string make_any_2_actors_message(const string& subject, const string& object,
                                 const string& verb, const string& suffix,
                                 const string& punctuation)
{
    bool you_acting = (lowercase_string(subject) == "you");
    bool reflexive = ends_with(object, "self");
    string finite_verb = conjugate_verb(verb, you_acting);

    // printf-style format string (we custom build this for each scenario rather than using a 
    // generic one with placeholders for everything because the translations may be different)
    string format;

    // final result
    string msg;

    if (you_acting && !reflexive)
    {
        format = "You " + finite_verb + " %s";
        if (!suffix.empty())
            format += " " + suffix;
        msg = localise(format, object);
    }
    else if (you_acting)
    {
        // Shouldn't really happen. Monsters can attack themselves when
        // confused, but players can't.
        format = "You " + finite_verb + " yourself";
        if (!suffix.empty())
            format += " " + suffix;
        msg = localise(format);
    }
    else if (lowercase_string(object) == "you")
    {
        format = "%s " + finite_verb + " you";
        if (!suffix.empty())
            format += " " + suffix;
        msg = localise(format, subject);
    }
    else if (reflexive && localisation_active())
    {
        // i18n: Third person reflexive pronouns (himself, herself, etc.)
        // can be difficult to translate because the gender in the target
        // language may not match the gender in English (in languages
        // where nouns have gender, the gender of the pronoun usually matches
        // the gender of the noun, rather than the the actual gender of the
        // entity as in English).
        // We will hardcode "itself". The translator may have to rephrase in
        // some languages (for example, into passive voice).
        format = "%s " + finite_verb + " itself";
        if (!suffix.empty())
            format += " " + suffix;
        msg = localise(format, subject);
    }
    else
    {
        format = "%s " + finite_verb + " %s";
        if (!suffix.empty())
            format += " " + suffix;
        msg = localise(format, subject, object);
    }

    if (!punctuation.empty())
        msg = add_punctuation(msg, punctuation, false);

    return msg;
}

/*
 * Get message where subject and object can be any combination of 2nd or 3rd person
 * (1st person doesn't exist in this game)
 */
string make_any_2_actors_message(const actor* subject, const actor* object,
                                 const string& verb, const string& suffix,
                                 const string& punctuation)
{
    bool subject_seen = (subject && subject->observable());
    bool object_seen = (object && object->observable());

    string subj = actor_name(subject, DESC_THE, subject_seen);
    string obj;
    if (object && object == subject)
        obj = actor_pronoun(object, PRONOUN_REFLEXIVE, object_seen);
    else
        obj = actor_name(object, DESC_THE, object_seen);
    return make_any_2_actors_message(subj, obj, verb, suffix, punctuation);
}

/*
 * Output message where subject and object can be any combination of 2nd or 3rd person
 * (1st person doesn't exist in this game)
 * Note: subject must be singular
 */
void do_any_2_actors_message(const string& subject, const string& object,
                             const string& verb, const string& suffix,
                             const string& punctuation)
{
    string msg = make_any_2_actors_message(subject, object, verb, suffix,
                                           punctuation);
    mpr_nolocalise(msg);
}

/*
 * Output message where subject and object can be any combination of 2nd or 3rd person
 * (1st person doesn't exist in this game)
 */
void do_any_2_actors_message(const actor* subject, const actor* object,
                             const string& verb, const string& suffix,
                             const string& punctuation)
{
    string msg = make_any_2_actors_message(subject, object, verb, suffix,
                                           punctuation);
    mpr_nolocalise(msg);
}

/*
 * Get message where subject is guaranteed to be 3rd person
 * (Object can be 2nd or 3rd person)
 * Note: supplied messages must match subject number (singular or plural)
 */
string get_3rd_person_message(const string& subject, const string& object,
                              const string& you_obj_msg,
                              const string& other_msg,
                              const string& punctuation)
{
    string msg;
    if (object == "you")
        msg = localise(you_obj_msg, subject);
    else
        msg = localise(other_msg, subject, object);

    if (!punctuation.empty())
        msg = add_punctuation(msg, punctuation, false);

    if (subject == "you")
        msg += " (bug: 2nd person subject unexpected here)";

    return msg;
}

/*
 * Get message where subject is guaranteed to be 3rd person
 * (Object can be 2nd or 3rd person)
 * Note: supplied messages must match subject number (singular or plural)
 */
string get_3rd_person_message(const actor* subject, bool subject_seen,
                              const actor* object, bool object_seen,
                              const string& you_obj_msg,
                              const string& other_msg,
                              const string& punctuation)
{
    string subj = actor_name(subject, DESC_THE, subject_seen);
    string obj = actor_name(object, DESC_THE, object_seen);

    string msg = get_3rd_person_message(subj, obj, you_obj_msg, other_msg,
                                        punctuation);

    if (subject && subject == object)
    {
        // reflexive (acting on self) - we didn't expect that here
        msg += " (bug: reflexive unexpected here)";
    }

    return msg;
}

void do_3rd_person_message(const string& subject, const string& object,
                             const string& you_obj_msg,
                             const string& other_msg,
                             const string& punctuation)
{
    string msg = get_3rd_person_message(subject, object, you_obj_msg, other_msg,
                                        punctuation);

    if (!msg.empty())
        mpr_nolocalise(msg);
}

/*
 * Output message where subject is guaranteed to be 3rd person
 * (Object can be 2nd or 3rd person)
 */
void do_3rd_person_message(const actor* subject, bool subject_seen,
                           const actor* object, bool object_seen,
                           const string& you_obj_msg,
                           const string& other_msg,
                           const string& punctuation)
{
    string msg = get_3rd_person_message(subject, subject_seen,
                                        object, object_seen,
                                        you_obj_msg, other_msg, punctuation);

    if (!msg.empty())
        mpr_nolocalise(msg);
}

// @noloc section end