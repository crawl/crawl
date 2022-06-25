#pragma once

enum class explore_greedy_options
{
    // Default, same as before the change, greedy visits piles of items
    EG_PILE, 
    // Greedy explore now visits enchanted items both in stacks and not,
    // ignores piles of mundane items
    EG_ENCHANTED, 
    // Greedy explore visits both piles and single enchanted items
    EG_BOTH,
};
