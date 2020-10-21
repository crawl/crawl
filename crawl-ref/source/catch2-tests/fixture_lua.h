#pragma once

class FixtureLua
{
public:
    FixtureLua();

    ~FixtureLua();
};

void require_execstring(const std::string& cmd, int nresults = 0,
                        const std::string& context = "base");
