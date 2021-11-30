/*
 * Variant message types
 * Messages that change based on the grammatical person of the subject and object
 * (e.g. "You hit <the monster>", "<The monster> hits you", "<The monster> hits <the monster>")
 */
#pragma once

enum msg_variant_type
{
    MV_YOU_SUBJECT, // you are the subject (and the object, if there is one, is 3rd person)
    MV_YOU_OBJECT, // you are the object (and the subject is 3rd person)
    MV_THIRD_PARTIES, // you are not involved (e.g. monster vs. monster)
    MV_YOURSELF, // you do something to yourself
    MV_ITSELF, // 3rd person does something to itself (avoid using itself/himself/herself
               // in the message- it's hard to translate)
};

enum variant_msg_type
{
    VMSG_NONE,

    // elemental effects
    VMSG_MELT,
    VMSG_BURN,
    VMSG_FREEZE,
    VMSG_ELECTROCUTE,

    VMSG_CRUSH,
    VMSG_SHATTER,
    VMSG_ENVENOM,
    VMSG_DRAIN,
    VMSG_BLAST,
};
