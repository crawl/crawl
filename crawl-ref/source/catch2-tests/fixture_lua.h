#pragma once
#include <string>

class FixtureLua
{
public:
    FixtureLua();

    ~FixtureLua();
};

void dlua_exec(const std::string& cmd, int nresults = 0,
               const std::string& context = "base");
