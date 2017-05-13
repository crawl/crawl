/**
 * @file
 * @brief Functions used to print information about gods.
 **/

#include "AppHdr.h"

#include "describe-god.h"

#include <iomanip>

#include "ability.h"
#include "branch.h"
#include "cio.h"
#include "database.h"
#include "describe.h"
#include "describe-demigod-portfolio.h"
#include "demigod-portfolio.h"
#include "english.h"
#include "eq-type-flags.h"
#include "food.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "god-prayer.h"
#include "god-type.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "religion.h"
#include "skills.h"
#include "spl-util.h"
#include "stringutil.h"
#include "unicode.h"
#include "xom.h"

int demigod_portfolio_colour(demigod_portfolio portfolio)
{
	switch (portfolio)
	{
	case DEMIGOD_PORTFOLIO_ARCHERY:
		return LIGHTCYAN;
	case DEMIGOD_PORTFOLIO_FIRE:
		return LIGHTRED;
	case DEMIGOD_PORTFOLIO_POISON:
		return GREEN;
	default:
		break;
	}
	return YELLOW;
}

static const char *demigod_portfolio_ability_desc(demigod_portfolio portfolio,int level)
{
	switch (portfolio)
	{
	case DEMIGOD_PORTFOLIO_ARCHERY:
		switch (level)
		{
		case 1:
			return "You can shoot farther.";
		default:
			break;
		}
		break;
	case DEMIGOD_PORTFOLIO_FIRE:
		break;
	case DEMIGOD_PORTFOLIO_POISON:
		switch (level)
		{
		case DEMIGOD_PORTFOLIO_POISON_LEVEL_SLOW_POISON:
			return "Your poison also slows target shortly.";
		case DEMIGOD_PORTFOLIO_POISON_LEVEL_GIVE_POISON_RES:
			return "You are resistant to poison.";
		case DEMIGOD_PORTFOLIO_POISON_LEVEL_AUGMENT_POISON:
			return "Your poison spell is more powerful.";
		case DEMIGOD_PORTFOLIO_POISON_LEVEL_GIVE_POISON_IMMUNE:
			return "You are immune to poison.";
		case DEMIGOD_PORTFOLIO_POISON_LEVEL_WEAKEN_POISON:
			return "Your poison also weakens target shortly.";
		case DEMIGOD_PORTFOLIO_POISON_LEVEL_POTENT:
			return "You can poison almost everything with weapon or advanced spell.";
		default:
			break;
		}		
		break;
	default:
		break;
	}
	return "";
}


void describe_demigod_portfolio()
{
	clrscr();

	if (you.demigod_portifolio == DEMIGOD_PORTFOLIO_NO_PORTFOLIO)
	{
		textcolour(LIGHTGREY);
		cprintf("You have no divine portfolio.");
	}
	else
	{
		int portfolio_colour = demigod_portfolio_colour(you.demigod_portifolio);
		textcolour(WHITE);
		cprintf("Your divine portfolio : ", demigod_portfolio_type_name(you.demigod_portifolio));
		textcolour(portfolio_colour);
		cprintf("%s\n\n", demigod_portfolio_type_name(you.demigod_portifolio));

		textcolour(LIGHTGREY);

		int divinity = you.skill(SK_INVOCATIONS);
		cprintf("Divine power : %d\n",divinity);
		cprintf("Your divine power gives you following effects.\n\n");		
		for (int j = 0; j <= MAX_SKILL_LEVEL; j++)
		{
			string desc = demigod_portfolio_ability_desc(you.demigod_portifolio, j);
			if (desc.length() <= 0)
				continue;
			if (j <= divinity)
			{
				textcolour(portfolio_colour);
			}
			else
			{
				textcolour(DARKGRAY);
			}
			cprintf("Level %d : %s\n", j,desc.c_str());
		}
	}
	textcolour(LIGHTGREY);

	getchm();
}
