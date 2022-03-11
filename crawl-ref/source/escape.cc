/**
 * @file
 * @brief Print an appropriate message about your character when escaping.
**/

//todo(rosstin) what needs included?


#include "escape.h"

#include <sstream>
#include <cstring>
#include <string>

#include "AppHdr.h"

#include "database.h"
#include "describe.h"
#include "english.h"
#include "enum.h"
#include "invent.h"
#include "item-status-flag-type.h"
#include "libutil.h"
#include "message.h"
#include "stringutil.h"
#include "tag-version.h"
#include "unicode.h"
		
void print_escape_message(){
	int num_runes = static_cast<int>(you.runes.count());

	if(player_has_orb() && num_runes >= you.obtainable_runes){
		mprf("You have escaped, carrying the Orb and all %d runes of Zot!", num_runes);
		print_win_message();
	}
	else if(player_has_orb()){
		mprf("You have escaped, carrying the Orb and %d runes of Zot!", num_runes);
		print_win_message();
	}
	else{
		mpr("You have escaped!");
	}
}

void print_win_message(){
	if(you.species == SP_DEMIGOD){
		const skill_type sk = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
		mpr(win_messages_demigod(sk));
	}
	else {
		mpr(win_messages_religion(you.religion));
	}
}

std::string win_messages_religion(god_type god){
	switch(god){
		case GOD_ASHENZARI:
			return "";
		case GOD_BEOGH:
			return "";
		case GOD_CHEIBRIADOS:
			return "";
		case GOD_DITHMENOS:
			return "";
		case GOD_ELYVILON:
			return "";
		case GOD_FEDHAS:
			return "";
		case GOD_GOZAG:
			return "";
		case GOD_HEPLIAKLQANA:
			return "";
		case GOD_IGNIS:
			return "";
		case GOD_JIYVA:
			return getMiscString("epilogue_jiyva");
		case GOD_KIKUBAAQUDGHA:
			return "";
		case GOD_LUGONU:
			return "";
		case GOD_MAKHLEB:
			return "";
		case GOD_NEMELEX_XOBEH:
			return "";
		case GOD_OKAWARU:
			return "";
		case GOD_QAZLAL:
			return "";
		case GOD_RU:
			return "";
		case GOD_SIF_MUNA:
			return "";
		case GOD_TROG:
			return "";
		case GOD_USKAYAW:
			return "";
		case GOD_VEHUMET:
			return "";
		case GOD_WU_JIAN:
			return "";
		case GOD_XOM:
			return "";
		case GOD_YREDELEMNUL:
			return "";
		case GOD_ZIN:
			return "You spread the word of Zin far and wide.";
		case GOD_SHINING_ONE:
			return "You spread the holy wrath of the Shining One throughout the world.";
		case GOD_NO_GOD:
		default:
			return "Your name is spread far and wide as the retriever of the Orb.";
			break;
}
}

std::string win_messages_demigod(skill_type sk){
	switch(sk){
		// fighting and melee weapons
		case SK_FIGHTING:
			return getMiscString("epilogue_demigod_fighting");
		case SK_SHORT_BLADES:
			return getMiscString("epilogue_demigod_short_blades");
		case SK_LONG_BLADES:
			return getMiscString("epilogue_demigod_long_blades");
		case SK_AXES:
			return getMiscString("epilogue_demigod_axes");
		case SK_MACES_FLAILS:
			return getMiscString("epilogue_demigod_maces_flails");
		case SK_POLEARMS:
			return getMiscString("epilogue_demigod_polearms");
		case SK_STAVES: 
			// check if your stave is magical? use your equipped/inventory weapon info in some way?
			return getMiscString("epilogue_demigod_staves");
		case SK_UNARMED_COMBAT: // Usk, Wu Jian
			if(you.strength() >= you.dex()){
				return getMiscString("epilogue_demigod_unarmed_strength");
			}
			else{
				return getMiscString("epilogue_demigod_unarmed_dex");
			}
		// ranged weapons
		case SK_SLINGS:
			return getMiscString("epilogue_demigod_slings");
		case SK_BOWS:
			return getMiscString("epilogue_demigod_bows");
		case SK_CROSSBOWS:
			return getMiscString("epilogue_demigod_crossbows");
		case SK_THROWING:
			return getMiscString("epilogue_demigod_throwing");
		// defensive skills
		case SK_ARMOUR:
			return getMiscString("epilogue_demigod_armour");
		case SK_DODGING: 
			return getMiscString("epilogue_demigod_dodging");
		case SK_STEALTH:
			return getMiscString("epilogue_demigod_stealth");
		case SK_SHIELDS:
			return getMiscString("epilogue_demigod_shields");
		// spellcasting skills
		case SK_SPELLCASTING: 
			return getMiscString("epilogue_demigod_spellcasting");
			break;
		case SK_CONJURATIONS: // Veh
			mprf("With your skill with destructive magic, you best Vehumet, becoming the new God of Conjurations.");
			break;
		case SK_HEXES: // Ru
			mprf("With your clever maledictions, you beguile even Ru, becoming the God of Hexes.");
			break;
		case SK_SUMMONINGS: // Makh
			mprf("With your skill at calling otherworldly beings, you supplant Makhleb, becoming the God of Bindings.");
			break;
		case SK_NECROMANCY: // Kiku, Yred, Tartarus
			mprf("With your mastery over death, you supplant Kikubaaqudgha, becoming the new God of Death.");
			//mprf("With your mastery over death, you supplant Yredelemnul, becoming the new God of Death.");
			//mprf("With your mastery over death, you claim dominion over Tartarus, becoming the new God of Death.");
			break;
		case SK_TRANSLOCATIONS: // Lugonu
			mprf("With your mastery over the weave of space, you slip past Lugonu's defenses, mastering the Abyss and becoming God of the Warp.");
			break;
		case SK_TRANSMUTATIONS: // Zin, Jivya (Xom?)
			mprf("With your ever-changing form, you corrupt the purity of Zin, becoming the God of Shapeshifting.");
			// mprf("With your ever-changing form, you supplant Jivya, becoming the God of Shapeshifting.");
			break;
		case SK_FIRE_MAGIC: // Ignis, Gehenna
			mprf("With the Orb's power, you shed your mortal half and ascend to Godhood, snuffing out Ignis to become the new God of Fire.");
			//mprf("With flames wreathing you, you claim the throne of Gehenna, becoming the new God of Fire.");
			break;
		case SK_ICE_MAGIC:
			mprf("With your mastery of the bitter cold, you claim dominion of Cocytus, becoming the God of Ice.");
			break;
		case SK_AIR_MAGIC:
			mprf("With your mastery of the air, you supplant Qazlal Stormbringer, becoming the new God of Storms.");
			break;
		case SK_EARTH_MAGIC:
			return "With the very firmament supporting you, you ascend to the throne of The Iron City of Dis, becoming the God of Earth.";
		case SK_POISON_MAGIC: // jivya or fedhas
			return "With powerful poisons, you take Jivya's place and become the God of Venom.";
			//mprf("With powerful poisons, you supplant Fedhas Madash and become the God of Venom.");                        
		// invocations (impossible since demigods can't train it)
		// evocations
		case SK_EVOCATIONS: // Nemelex Xobeh, Gozag
			return "With your trusty magical devices, you supplant Nemelex Xobeh, becoming the new God of Evocations.";
			//mprf("With your trove of magical devices, you supplant Gozag Ym Sagoz, becoming the God of Evocations.");
		default:
			return "You shed your mortal half and ascend to godhood.";
		}
}

