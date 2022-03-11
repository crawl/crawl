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
			return "You spread the dominion of slime across the world, returning all beings to the shapeless ooze from which we all sprang.";
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
			return "You spread the word of Zin far and wide.";
		case GOD_NO_GOD: // hmm
		default:
			return "Your name is spread far and wide as the retriever of the Orb.";
			break;
}
}

std::string win_messages_demigod(skill_type sk){
	switch(sk){
		// fighting and melee weapons
		case SK_FIGHTING:
			return "You shed your mortal half and ascend to Godhood, wresting victory over Okawaru to become the new God of Battle.";
		case SK_SHORT_BLADES:
			return "You shed your mortal half and ascend to Godhood, backstabbing Dithmenos to become the new God of Knives.";
		case SK_LONG_BLADES:
			return "With your skill with the sword, you best Okawaru in a duel, becoming the new God of Blades.";
		case SK_AXES:
			return "With your mighty axe, you slaughter Trog, becoming the new God of Axes.";
		case SK_MACES_FLAILS:
			return "With your skill with the mace, you crush Okawaru, becoming the God of Maces.";
		case SK_POLEARMS:
			return "With your skill with the polearm, you skewer Okawaru from a distance, becoming the God of Polearms.";
		case SK_STAVES:
			return "With your skill with staves, you batter Okawaru into submission, becoming the God of Staves.";
		case SK_UNARMED_COMBAT: // Usk, Wu Jian
			if(you.strength() >= you.dex()){
				return "With your strong body, you outdance even Uskayaw, becoming the God of Wrestling.";
			}
			else{
				return "With the skills of your body, you crush the Wu Jian Council, becoming the God of Martial Arts.";
			}
			break;
		// ranged weapons
		case SK_SLINGS:
			return "With your singular sling, you smite down Trog, becoming the God of Slings.";
		/*
		case SK_BOWS:
			mprf("With your trusty bow, you snipe down Okawaru, becoming the God of Bows.");
			break;
		case SK_CROSSBOWS:
			mprf("With your skill with the Crossbow, you pierce Okawaru's defenses, becoming the God of Crossbows.");
			break;
		case SK_THROWING:
			mprf("With your mighty projectiles, you cast down Okawaru, becoming the new God of Throwing.");
			break;
		// defensive skills
		case SK_ARMOUR: // Chei? Ash?
			mprf("With your insurmountable defenses, you gradually unseat Cheibriados, becoming the God of Armour.");
			//mprf("With your mastery of sartorial defenses, you supplant Ashenzari, becoming the God of Armour.");
			//TSO?
			break;
		case SK_DODGING: // Usk?
			mprf("With your untouchable moves, you outdance even Uskayaw, becoming the God of Dodging.");
			// wu jian?
			break;
		case SK_STEALTH:
			mprf("You slip into the darkness and dispatch Dithmenos, becoming the new God of Shadows.");
			break;
		case SK_SHIELDS: // Chei, TSO?
			mprf("With your unparallelled mastery of the shield, you block the Shining One's efforts to thwart you, and become the God of Shields.");
			break;
		// spellcasting skills
		case SK_SPELLCASTING: // Sif
			mprf("With your overwhelming knowledge of spells, your magical knowledge surpasses even Sif Muna, and you become the new God of Magic.");
			break;
		case SK_CONJURATIONS: // Veh
			mprf("With your skill with destructive magic, you best Vehumet, becoming the new God of Conjurations.");
			break;
		case SK_HEXES: // Ru
			mprf("With your clever maledictions, you beguile Ru, becoming the God of Hexes.");
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
			break;*/
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

