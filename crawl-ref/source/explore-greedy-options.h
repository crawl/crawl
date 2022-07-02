#pragma once

enum explore_greedy_options
{
    // Greedy explore doesn't visit any non-autopickup items
    EG_NONE = 0x00000,

    // Greedy explore visits stacks of items
    EG_STACK = 0x00001,

    // Greedy explore visits glowing items both in stacks and not,
    EG_GLOWING = 0x0002,

    // Greedy explore visits artefact items
    EG_ARTEFACT = 0x0004,
};
