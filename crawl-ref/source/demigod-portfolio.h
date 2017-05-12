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