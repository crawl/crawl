/**
 * @file
 * @brief Key remapping class
**/

#pragma once

#include "KeymapContext.h"

typedef deque<int> keyseq;
typedef map<keyseq,keyseq> macromap;

class keymapper
{
public:
    keymapper(const macromap& keymap);
    keymapper(KeymapContext context);

    void add_key(int key);
    bool is_matching() const;
    bool get_mapped_keys(deque<int>& out);

private:
    bool is_partial_match(const deque<int>& buf) const;

    deque<int> m_buffer;
    const macromap &m_keymap;
};
