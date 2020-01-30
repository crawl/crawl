/**
 * @file
 * @brief Key remapping class
**/

#include "AppHdr.h"

#include "keymapper.h"

extern macromap Keymaps[KMC_CONTEXT_COUNT];

static macromap empty_map;

keymapper::keymapper(const macromap& keymap) : m_keymap(keymap) { }

keymapper::keymapper(KeymapContext context)
    : m_keymap(context == KMC_NONE ? empty_map : Keymaps[context]) { }

void keymapper::add_key(int key)
{
    m_buffer.push_back(key);
}

bool keymapper::is_matching() const
{
    for (const auto& kv : m_keymap)
        if (is_partial_match(kv.first))
            return true;
    return false;
}

bool keymapper::get_mapped_keys(deque<int>& out)
{
    const auto subst = m_keymap.find(m_buffer);
    const auto did_map = subst != m_keymap.end();
    const auto& mapped = did_map ? subst->second : m_buffer;

    if (subst != m_keymap.end())
        ASSERT(!subst->second.empty());

    for (auto key : mapped)
        out.push_back(key);
    m_buffer.clear();

    return did_map;
}

bool keymapper::is_partial_match(const keyseq& buf) const
{
    if (m_buffer.size() >= buf.size())
        return false;
    for (size_t i = 0; i < m_buffer.size(); ++i)
        if (buf[i] != m_buffer[i])
            return false;
    return true;
}
