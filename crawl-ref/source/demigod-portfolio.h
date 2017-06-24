#pragma once

enum demigod_portfolio
{
    DEMIGOD_PORTFOLIO_NO_PORTFOLIO = 0,
    DEMIGOD_PORTFOLIO_ARCHERY,
    DEMIGOD_PORTFOLIO_FIRE,
    DEMIGOD_PORTFOLIO_POISON,    
    NUM_DEMIGOD_PORTFOLIO,
};

const char* demigod_portfolio_type_name(demigod_portfolio type);
bool gain_portfolio();
int your_demigod_portfolio_level(demigod_portfolio type);
bool you_have_demigod_portfolio_level(demigod_portfolio type,int level);

const int DEMIGOD_PORTFOLIO_POISON_LEVEL_SLOW_POISON = 1;
const int DEMIGOD_PORTFOLIO_POISON_LEVEL_GIVE_POISON_RES = 8;
const int DEMIGOD_PORTFOLIO_POISON_LEVEL_GIVE_POISON_IMMUNE = 16;
const int DEMIGOD_PORTFOLIO_POISON_LEVEL_AUGMENT_POISON = 12;
const int DEMIGOD_PORTFOLIO_POISON_LEVEL_WEAKEN_POISON = 18;
const int DEMIGOD_PORTFOLIO_POISON_LEVEL_POTENT = 27;
