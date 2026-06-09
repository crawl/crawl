#pragma once

// Actions that a god may outright forbid (as opposed to merely disliking, see
// conduct_type). A forbidden action is blocked before it happens, rather than
// docking piety or handing out penance after the fact.
enum forbidden_act_type
{
    FORBID_NONE,
    FORBID_EVIL,            // evil magic or items (good gods)
    FORBID_HOLY,            // holy magic or items (Yredelemnul)
    FORBID_CHAOS,           // chaotic magic or items (Zin)
    FORBID_TRANSFORMATION,  // transformation/mutation (Zin)
    FORBID_HASTY,           // hasting effects or items (Cheibriados)
    FORBID_SPELLCASTING,    // casting any spell (Trog)
    FORBID_TRAIN_MAGIC,     // training magic skills (Trog)
    FORBID_WIZARDLY_ITEM,   // staves and other wizardly items (Trog)
    NUM_FORBIDS
};
