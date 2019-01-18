#pragma once

enum class confirm_prompt_type
{
    cancel,             // automatically answer 'no', i.e. disallow
    prompt,             // prompt
    none,               // automatically answer 'yes'
};
