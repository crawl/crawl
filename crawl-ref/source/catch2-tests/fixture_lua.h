#pragma once

#include <string>

class FixtureLua
{
public:
    explicit FixtureLua(uint64_t seed=729);

    ~FixtureLua();
};

void dlua_exec(const std::string& cmd, int nresults = 0,
               const std::string& context = "base");
