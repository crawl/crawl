#include "AppHdr.h"

#include "demigod-portfolio.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mpr.h"
#include "notes.h"
#include "player.h"
#include "state.h"
#include "stringutil.h"

const char* demigod_portfolio_type_name(demigod_portfolio type)
{
    switch (type)
    {
        case DEMIGOD_PORTFOLIO_ARCHERY:
            return "Archery";
            break;
        case DEMIGOD_PORTFOLIO_FIRE:
            return "Fire";
            break;
        case DEMIGOD_PORTFOLIO_POISON:
            return "Poison";
            break;
        default:
            return "INVALID_DEMIGOD_PORTFOLIO";
    }
}

bool gain_portfolio()
{
    std::vector<demigod_portfolio> valid_portfolio;

    valid_portfolio.push_back(DEMIGOD_PORTFOLIO_ARCHERY);
    valid_portfolio.push_back(DEMIGOD_PORTFOLIO_FIRE);
    valid_portfolio.push_back(DEMIGOD_PORTFOLIO_POISON);

    if (valid_portfolio.size() <= 0)
    {
        return false;
    }

    mprf(MSGCH_PROMPT, "Choose your divine portfolio!");
    string portrait_list_message;
    string portrait_string;

    for (unsigned int j = 0; j < (unsigned int)valid_portfolio.size(); j++)
    {
        portrait_string = make_stringf("(%c) %s ", 'a' + j, demigod_portfolio_type_name(valid_portfolio[j]));
        portrait_list_message.append(portrait_string.c_str());        
    }

    mprf(MSGCH_PROMPT,"%s",portrait_list_message.c_str());

    int keyin;
    while (true)
    {
        keyin = getchm();
        int index = -1;
        if (keyin >= 'a' && keyin <= 'z')
        {
            index = keyin - 'a';
        }
        else if (keyin >= 'A' && keyin <= 'Z')
        {
            index = keyin - 'A';
        }

        if (index >= 0 && index < (int)valid_portfolio.size())
        {
            you.demigod_portifolio = valid_portfolio[index];            
            mprf(MSGCH_PLAIN, "You've got %s portfolio", demigod_portfolio_type_name(you.demigod_portifolio));
            return true;
        }
        else
        {
            mprf(MSGCH_PLAIN, "invalid_input");
        }        
    }
    return false;
}

int your_demigod_portfolio_level(demigod_portfolio type)
{
    if (you.species != SP_DEMIGOD || you.demigod_portifolio != type) 
        return 0;
    return you.skill(SK_INVOCATIONS);
}

bool you_have_demigod_portfolio_level(demigod_portfolio type, int level)
{
    if (your_demigod_portfolio_level(type) < level) 
        return false;
    return true;
}

