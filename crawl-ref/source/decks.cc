/**
 * @file
 * @brief Functions with decks of cards.
**/

#include "AppHdr.h"

#include "decks.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <unordered_set>

#include "ability.h"
#include "abyss.h"
#include "act-iter.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "evoke.h"
#include "food.h"
#include "ghost.h"
#include "god-passive.h" // passive_t::no_haste
#include "god-wrath.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "maps.h"
#include "macro.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-project.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "scroller.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-monench.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-wpnench.h"
#include "skill-menu.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "uncancel.h"
#include "unicode.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

using namespace ui;

typedef map<card_type, int> deck_archetype;

deck_archetype deck_of_escape =
{
    { CARD_TOMB,       5 },
    { CARD_EXILE,      1 },
    { CARD_ELIXIR,     5 },
    { CARD_CLOUD,      5 },
    { CARD_VELOCITY,   5 },
    { CARD_SHAFT,      5 },
    { CARD_PORTAL,     5 },
    { CARD_HELM,       5 },
};

deck_archetype deck_of_destruction =
{
    { CARD_VITRIOL,    5 },
    { CARD_PAIN,       5 },
    { CARD_ORB,        5 },
    { CARD_DEGEN,      3 },
    { CARD_WILD_MAGIC, 5 },
    { CARD_STORM,      5 },
};

deck_archetype deck_of_summoning =
{
    { CARD_ELEMENTS,        5 },
    { CARD_SUMMON_DEMON,    5 },
    { CARD_SUMMON_WEAPON,   5 },
    { CARD_SUMMON_FLYING,   5 },
    { CARD_RANGERS,         5 },
    { CARD_ILLUSION,        5 },
};

deck_archetype deck_of_wonder =
{
    { CARD_MERCENARY,    5 },
    { CARD_ALCHEMIST,    5 },
    { CARD_BARGAIN,      5 },
    { CARD_SAGE,         5 },
    { CARD_TROWEL,       5 },
    { CARD_EXPERIENCE,   5 },
    { CARD_SHUFFLE,      1 },
};

deck_archetype deck_of_punishment =
{
    { CARD_WRAITH,     5 },
    { CARD_WRATH,      5 },
    { CARD_FAMINE,     5 },
    { CARD_SWINE,      5 },
    { CARD_TORMENT,    5 },
    { CARD_XOM,        5 },
};

struct deck_type_data
{
    string name;
    string flavour;
    /// The list of cards this deck contains.
    deck_archetype cards;
    int deck_max;
};

static map<deck_type, deck_type_data> all_decks =
{
    { DECK_OF_ESCAPE, {
        "escape", "mainly dealing with various forms of escape.",
        deck_of_escape,
        26,
    } },
    { DECK_OF_DESTRUCTION, {
        "destruction", "most of which hurl death and destruction "
            "at one's foes (or, if unlucky, at oneself).",
        deck_of_destruction,
        26,
    } },
    { DECK_OF_SUMMONING, {
        "summoning", "depicting a range of weird and wonderful creatures.",
        deck_of_summoning,
        26,
    } },
    { DECK_OF_WONDER, {
        "wonder", "A deck of highly mysterious and magical cards, which can "
        "alter the drawer's physical and mental condition, for better or worse.",
        deck_of_wonder,
        26,
    } },
    { DECK_OF_PUNISHMENT, {
        "punishment", "which wreak havoc on the user.", deck_of_punishment,
        0, // Not a user deck
    } },
};

vector<ability_type> deck_ability = {
    ABIL_NEMELEX_DRAW_ESCAPE,
    ABIL_NEMELEX_DRAW_DESTRUCTION,
    ABIL_NEMELEX_DRAW_SUMMONING,
    ABIL_NEMELEX_DRAW_WONDER,
    ABIL_NON_ABILITY,
    ABIL_NEMELEX_DRAW_STACK
};

const char* card_name(card_type card)
{
    switch (card)
    {
    case CARD_VELOCITY:        return "Velocity";
    case CARD_EXILE:           return "Exile";
    case CARD_ELIXIR:          return "the Elixir";
    case CARD_STAIRS:          return "the Stairs";
    case CARD_TOMB:            return "the Tomb";
    case CARD_WILD_MAGIC:      return "Wild Magic";
    case CARD_ELEMENTS:        return "the Elements";
    case CARD_SUMMON_DEMON:    return "the Pentagram";
    case CARD_SUMMON_WEAPON:   return "the Dance";
    case CARD_SUMMON_FLYING:   return "Foxfire";
    case CARD_RANGERS:         return "the Rangers";
    case CARD_SHAFT:           return "the Shaft";
    case CARD_VITRIOL:         return "Vitriol";
    case CARD_CLOUD:           return "the Cloud";
    case CARD_STORM:           return "the Storm";
    case CARD_PAIN:            return "Pain";
    case CARD_TORMENT:         return "Torment";
    case CARD_WRATH:           return "Wrath";
    case CARD_WRAITH:          return "the Wraith";
    case CARD_SWINE:           return "the Swine";
    case CARD_ORB:             return "the Orb";
    case CARD_ILLUSION:        return "the Illusion";
    case CARD_DEGEN:           return "Degeneration";
    case CARD_FAMINE:          return "Famine";
    case CARD_XOM:             return "Xom";
    case CARD_MERCENARY:       return "the Mercenary";
    case CARD_ALCHEMIST:       return "the Alchemist";
    case CARD_BARGAIN:         return "the Bargain";
    case CARD_SAGE:            return "the Sage";
    case CARD_PORTAL:          return "the Portal";
    case CARD_TROWEL:          return "the Trowel";
    case CARD_EXPERIENCE:      return "Experience";
    case CARD_HELM:            return "the Helm";
    case CARD_SHUFFLE:         return "Shuffle";
    case NUM_CARDS:            return "a buggy card";
    }
    return "a very buggy card";
}

card_type name_to_card(string name)
{
    for (int i = 0; i < NUM_CARDS; i++)
    {
        if (card_name(static_cast<card_type>(i)) == name)
            return static_cast<card_type>(i);
    }
    return NUM_CARDS;
}

static const deck_archetype _cards_in_deck(deck_type deck)
{
    deck_type_data *deck_data = map_find(all_decks, deck);

    if (deck_data)
        return deck_data->cards;

#ifdef ASSERTS
    die("No cards found for %u", unsigned(deck));
#endif
    return {};
}

const string stack_contents()
{
    const auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();

    string output = "";
    output += comma_separated_fn(
                reverse_iterator<CrawlVector::const_iterator>(stack.end()),
                reverse_iterator<CrawlVector::const_iterator>(stack.begin()),
              [](const CrawlStoreValue& card) { return card_name((card_type)card.get_int()); });
    if (!stack.empty())
        output += ".";

    return output;
}

const string stack_top()
{
    const auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();
    if (stack.empty())
        return "none";
    else
        return card_name((card_type) stack[stack.size() - 1].get_int());
}

const string deck_contents(deck_type deck)
{
    if (deck == DECK_STACK)
        return "Remaining cards: " + stack_contents();

    string output = "It may contain the following cards: ";

    // This way of doing things is intended to prevent a card
    // that appears in multiple subdecks from showing up twice in the
    // output.
    set<card_type> cards;
    const deck_archetype &pdeck =_cards_in_deck(deck);
    for (const auto& cww : pdeck)
        cards.insert(cww.first);

    output += comma_separated_fn(cards.begin(), cards.end(), card_name);
    output += ".";

    return output;
}

const string deck_flavour(deck_type deck)
{
    if (deck == DECK_STACK)
        return "set aside for later.";

    deck_type_data* deck_data = map_find(all_decks, deck);

    if (deck_data)
        return deck_data->flavour;

    return "";
}

static card_type _random_card(deck_type deck)
{
    const deck_archetype &pdeck = _cards_in_deck(deck);
    return *random_choose_weighted(pdeck);
}

int deck_cards(deck_type deck)
{
    return deck == DECK_STACK ? you.props[NEMELEX_STACK_KEY].get_vector().size()
                              : you.props[deck_name(deck)].get_int();
}

static void _get_pure_deck_weights(int weights[])
{
    weights[NEM_GIFT_ESCAPE] = you.sacrifice_value[OBJ_ARMOUR] + 1;
    weights[NEM_GIFT_DESTRUCTION] = you.sacrifice_value[OBJ_WEAPONS]
        + you.sacrifice_value[OBJ_STAVES]
        + you.sacrifice_value[OBJ_RODS]
        + you.sacrifice_value[OBJ_MISSILES] + 1;
    weights[NEM_GIFT_SUMMONING] = you.sacrifice_value[OBJ_CORPSES] / 2;
    weights[NEM_GIFT_WONDERS] = you.sacrifice_value[OBJ_POTIONS]
        + you.sacrifice_value[OBJ_SCROLLS]
        + you.sacrifice_value[OBJ_WANDS]
        + you.sacrifice_value[OBJ_FOOD]
        + you.sacrifice_value[OBJ_MISCELLANY]
        + you.sacrifice_value[OBJ_JEWELLERY]
        + you.sacrifice_value[OBJ_BOOKS];
}

static void _update_sacrifice_weights(int which)
{
    //(0.96)^8 = 0.72
    switch (which)
    {
    case NEM_GIFT_ESCAPE:
        you.sacrifice_value[OBJ_ARMOUR] *= 24;
        you.sacrifice_value[OBJ_ARMOUR] /= 25;
        break;
    case NEM_GIFT_DESTRUCTION:
        you.sacrifice_value[OBJ_WEAPONS] *= 24;
        you.sacrifice_value[OBJ_STAVES] *= 24;
        you.sacrifice_value[OBJ_RODS] *= 24;
        you.sacrifice_value[OBJ_MISSILES] *= 24;
        you.sacrifice_value[OBJ_WEAPONS] /= 25;
        you.sacrifice_value[OBJ_STAVES] /= 25;
        you.sacrifice_value[OBJ_RODS] /= 25;
        you.sacrifice_value[OBJ_MISSILES] /= 25;
        break;
    case NEM_GIFT_SUMMONING:
        you.sacrifice_value[OBJ_CORPSES] *= 24;
        you.sacrifice_value[OBJ_CORPSES] /= 25;
        break;
    case NEM_GIFT_WONDERS:
        you.sacrifice_value[OBJ_POTIONS] *= 24;
        you.sacrifice_value[OBJ_SCROLLS] *= 24;
        you.sacrifice_value[OBJ_WANDS] *= 24;
        you.sacrifice_value[OBJ_FOOD] *= 24;
        you.sacrifice_value[OBJ_MISCELLANY] *= 24;
        you.sacrifice_value[OBJ_JEWELLERY] *= 24;
        you.sacrifice_value[OBJ_BOOKS] *= 24;
        you.sacrifice_value[OBJ_POTIONS] /= 25;
        you.sacrifice_value[OBJ_SCROLLS] /= 25;
        you.sacrifice_value[OBJ_WANDS] /= 25;
        you.sacrifice_value[OBJ_FOOD] /= 25;
        you.sacrifice_value[OBJ_MISCELLANY] /= 25;
        you.sacrifice_value[OBJ_JEWELLERY] /= 25;
        you.sacrifice_value[OBJ_BOOKS] /= 25;
        break;
    }
}

#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
static void _show_pure_deck_chances()
{
    int weights[4];

    _get_pure_deck_weights(weights);

    float total = 0;
    for (int i = 0; i < NUM_NEMELEX_GIFT_TYPES; ++i)
        total += (float)weights[i];

    mprf(MSGCH_DIAGNOSTICS, "Pure cards chances: "
        "escape %0.2f%%, destruction %0.2f%%, "
        "summoning %0.2f%%, wonders %0.2f%%",
        (float)weights[0] / total * 100.0,
        (float)weights[1] / total * 100.0,
        (float)weights[2] / total * 100.0,
        (float)weights[3] / total * 100.0);
}
#endif

static deck_type _gift_type_to_deck(int gift)
{
    switch (gift)
    {
    case NEM_GIFT_ESCAPE:      return DECK_OF_ESCAPE;
    case NEM_GIFT_DESTRUCTION: return DECK_OF_DESTRUCTION;
    case NEM_GIFT_SUMMONING:   return DECK_OF_SUMMONING;
    case NEM_GIFT_WONDERS:     return DECK_OF_WONDER;
    }
    die("invalid gift card type");
}

bool gift_cards()
{
    const int deal = random_range(MIN_GIFT_CARDS, MAX_GIFT_CARDS);
    bool dealt_cards = false;
    for (int i = 0; i < deal; i++)
    {
        int weights[4];
        _get_pure_deck_weights(weights);
        const int choice_type = choose_random_weighted(weights, weights + 4);

        deck_type choice = _gift_type_to_deck(choice_type);

        if (deck_cards(choice) < all_decks[choice].deck_max)
        {
            _update_sacrifice_weights(choice_type);
            you.props[deck_name(choice)]++;
            dealt_cards = true;
        }

    }
#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS) || defined(DEBUG_SACRIFICE)
    _show_pure_deck_chances();
#endif
    return dealt_cards;
}


void reset_cards()
{
    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; i++)
        you.props[deck_name((deck_type) i)] = 0;
    you.props[NEMELEX_STACK_KEY].get_vector().clear();
}

string deck_summary()
{
    vector<string> stats;
    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; i++)
    {
        int cards = deck_cards((deck_type) i);
        const deck_type_data *deck_data = map_find(all_decks, (deck_type) i);
        const string name = deck_data ? deck_data->name : "bugginess";
        if (cards)
        {
            stats.push_back(make_stringf("%d %s card%s", cards,
               name.c_str(), cards == 1 ? "" : "s"));
        }
    }
    return comma_separated_line(stats.begin(), stats.end());
}

string which_decks(card_type card)
{
    vector<string> decks;
    string output = "\n";
    bool punishment = false;
    for (auto &deck_data : all_decks)
    {
        if (!deck_data.second.cards.count(card))
            continue;

        if (deck_data.first == DECK_OF_PUNISHMENT)
            punishment = true;
        else
            decks.push_back(deck_data.second.name);
    }

    if (!decks.empty())
    {
        output += "It is found in decks of "
               +  comma_separated_line(decks.begin(), decks.end());
        if (punishment)
            output += ", or in Nemelex Xobeh's deck of punishment";
        output += ".";
    }
    else if (punishment)
    {
        output += "It is only found in Nemelex Xobeh's deck of "
                  "punishment.";
    }
    else
        output += "It is normally not part of any deck.";

    return output;
}

static void _describe_cards(CrawlVector& cards)
{
    ASSERT(!cards.empty());

    auto scroller = make_shared<Scroller>();
    auto vbox = make_shared<Box>(Widget::VERT);

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_open_array("cards");
#endif
    bool seen[NUM_CARDS] = {0};
    ostringstream data;
    bool first = true;
    for (auto& val : cards)
    {
        card_type card = (card_type) val.get_int();

        if (seen[card])
            continue;
        seen[card] = true;

        string name = card_name(card);
        string desc = getLongDescription(name + " card");
        if (desc.empty())
            desc = "No description found.\n";
        string decks = which_decks(card);

        name = uppercase_first(name);
        desc = desc + decks;

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
        auto icon = make_shared<Image>();
        icon->set_tile(tile_def(TILEG_NEMELEX_CARD));
        title_hbox->add_child(move(icon));
#endif
        auto title = make_shared<Text>(formatted_string(name, WHITE));
        title->set_margin_for_sdl(0, 0, 0, 10);
        title_hbox->add_child(move(title));
        title_hbox->set_cross_alignment(Widget::CENTER);
        title_hbox->set_margin_for_crt(first ? 0 : 1, 0);
        title_hbox->set_margin_for_sdl(first ? 0 : 20, 0);
        vbox->add_child(move(title_hbox));

        auto text = make_shared<Text>(desc);
        text->set_wrap_text(true);
        vbox->add_child(move(text));

#ifdef USE_TILE_WEB
        tiles.json_open_object();
        tiles.json_write_string("name", name);
        tiles.json_write_string("desc", desc);
        tiles.json_close_object();
#endif
        first = false;
    }

#ifdef USE_TILE_LOCAL
    vbox->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif

    scroller->set_child(move(vbox));
    auto popup = make_shared<ui::Popup>(scroller);

    bool done = false;
    popup->on_keydown_event([&done, &scroller](const KeyEvent& ev) {
        done = !scroller->on_event(ev);
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_close_array();
    tiles.push_ui_layout("describe-cards", 0);
#endif

    ui::run_layout(move(popup), done);

#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif
}

string deck_status(deck_type deck)
{
    const string name = deck_name(deck);
    const int cards   = deck_cards(deck);

    ostringstream desc;

    desc << chop_string(deck_name(deck), 24)
         << to_string(cards);

    return trimmed_string(desc.str());
}

string deck_description(deck_type deck)
{
    ostringstream desc;

    desc << "A deck of magical cards, ";
    desc << deck_flavour(deck) << "\n\n";
    desc << deck_contents(deck) << "\n";

    if (deck != DECK_STACK)
    {
        const int cards = deck_cards(deck);
        desc << "\n";

        if (cards > 1)
            desc << make_stringf("It currently has %d cards ", cards);
        else if (cards == 1)
            desc << "It currently has 1 card ";
        else
            desc << "It is currently empty ";

        desc << make_stringf("and can contain up to %d cards.",
                             all_decks[deck].deck_max);
        desc << "\n";
    }

    return desc.str();
}

/**
 * The deck a given ability uses. Asserts if called on an ability that does not
 * use decks.
 *
 * @param abil the ability
 *
 * @return the deck
 */
deck_type ability_deck(ability_type abil)
{
    auto deck = find(deck_ability.begin(), deck_ability.end(), abil);

    ASSERT(deck != deck_ability.end());
    return (deck_type) distance(deck_ability.begin(), deck);
}

// This will assert if the player doesn't have the ability to draw from the
// deck passed.
static char _deck_hotkey(deck_type deck)
{
    return get_talent(deck_ability[deck], false).hotkey;
}

static deck_type _choose_deck(const string title = "Draw")
{
    ToggleableMenu deck_menu(MF_SINGLESELECT
            | MF_NO_WRAP_ROWS | MF_TOGGLE_ACTION | MF_ALWAYS_SHOW_MORE);
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(make_stringf("%s which deck?        "
                                    "Cards available", title.c_str()),
                                    "Describe which deck?    "
                                    "Cards available",
                                    MEL_TITLE);
        deck_menu.set_title(me, true, true);
    }
    deck_menu.set_tag("deck");
    deck_menu.add_toggle_key('!');
    deck_menu.add_toggle_key('?');
    deck_menu.menu_action = Menu::ACT_EXECUTE;

    deck_menu.set_more(formatted_string::parse_string(
                       "Press '<w>!</w>' or '<w>?</w>' to toggle "
                       "between deck selection and description."));

    int numbers[NUM_DECKS];

    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; i++)
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(deck_status(static_cast<deck_type>(i)),
                    deck_status(static_cast<deck_type>(i)),
                    MEL_ITEM, 1, _deck_hotkey(static_cast<deck_type>(i)));
        numbers[i] = i;
        me->data = &numbers[i];
        if (!deck_cards((deck_type)i))
            me->colour = COL_USELESS;

#ifdef USE_TILE
        me->add_tile(tile_def(TILEG_NEMELEX_DECK + i - FIRST_PLAYER_DECK + 1));
#endif
        deck_menu.add_entry(me);
    }

    int ret = NUM_DECKS;
    deck_menu.on_single_selection = [&deck_menu, &ret](const MenuEntry& sel)
    {
        ASSERT(sel.hotkeys.size() == 1);
        int selected = *(static_cast<int*>(sel.data));

        if (deck_menu.menu_action == Menu::ACT_EXAMINE)
            describe_deck((deck_type) selected);
        else
            ret = *(static_cast<int*>(sel.data));
        return deck_menu.menu_action == Menu::ACT_EXAMINE;
    };
    deck_menu.show(false);
    if (!crawl_state.doing_prev_cmd_again)
        redraw_screen();
    return (deck_type) ret;
}

/**
 * Printed when a deck is exhausted
 *
 * @return          A message to print;
 *                  e.g. "the deck of cards disappears without a trace."
 */
static string _empty_deck_msg()
{
    string message = random_choose("disappears without a trace.",
        "glows slightly and disappears.",
        "glows with a rainbow of weird colours and disappears.");
    return "The deck of cards " + message;
}

static bool _evoke_deck(deck_type deck, bool dealt = false)
{
    if (deck_cards(deck) <= 0) {
        mprf("This deck is empty");
        return false;
    }

    mprf("You %s a card...", dealt ? "deal" : "draw");

    if (deck == DECK_STACK)
    {
        auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();
        card_type card = (card_type) stack[stack.size() - 1].get_int();
        stack.pop_back();
        card_effect(card, dealt);
    }
    else
    {
        --you.props[deck_name(deck)];
        card_effect(_random_card(deck), dealt);
    }

    if (!deck_cards(deck))
        mpr(_empty_deck_msg());
    return true;
}

// Draw one card from a deck, prompting the user for a choice
bool deck_draw(deck_type deck)
{
    if (!deck_cards(deck))
    {
        mpr("That deck is empty!");
        return false;
    }

    return _evoke_deck(deck);
}

bool deck_stack()
{
    int total_cards = 0;

    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; ++i)
        total_cards += deck_cards((deck_type) i);

    if (deck_cards(DECK_STACK) && !yesno("Replace your current stack?",
                                          false, 0))
    {
        return false;
    }

    if (!total_cards)
    {
        mpr("You are out of cards!");
        return false;
    }

    if (total_cards < 5 && !yesno("You have fewer than five cards, "
                                  "stack them anyway?", false, 0))
    {
        canned_msg(MSG_OK);
        return false;
    }

    you.props[NEMELEX_STACK_KEY].get_vector().clear();
    run_uncancel(UNC_STACK_FIVE, min(total_cards, 5));
    return true;
}

class StackFiveMenu : public Menu
{
    virtual bool process_key(int keyin) override;
    CrawlVector& draws;
public:
    StackFiveMenu(CrawlVector& d)
        : Menu(MF_NOSELECT | MF_UNCANCEL | MF_ALWAYS_SHOW_MORE), draws(d) {};
};

bool StackFiveMenu::process_key(int keyin)
{
    if (keyin == CK_ENTER)
    {
        formatted_string old_more = more;
        set_more(formatted_string::parse_string(
                "Are you done? (press y or Y to confirm)"));
        if (yesno(nullptr, true, 'n', false, false, true))
            return false;
        set_more(old_more);
    }
    else if (keyin == '?')
        _describe_cards(draws);
    else if (keyin >= '1' && keyin <= '0' + static_cast<int>(draws.size()))
    {
        const unsigned int i = keyin - '1';
        for (unsigned int j = 0; j < items.size(); j++)
            if (items[j]->selected())
            {
                swap(draws[i], draws[j]);
                swap(items[i]->text, items[j]->text);
                items[j]->colour = LIGHTGREY;
                select_item_index(i, 0, false); // this also updates the item
                select_item_index(j, 0, false);
                return true;
            }
        items[i]->colour = WHITE;
        select_item_index(i, 1, false);
    }
    else
        Menu::process_key(keyin);
    return true;
}

static void _draw_stack(int to_stack)
{
    ToggleableMenu deck_menu(MF_SINGLESELECT | MF_UNCANCEL
            | MF_NO_WRAP_ROWS | MF_TOGGLE_ACTION | MF_ALWAYS_SHOW_MORE);
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry("Draw which deck?        "
                                    "Cards available",
                                    "Describe which deck?    "
                                    "Cards available",
                                    MEL_TITLE);
        deck_menu.set_title(me, true, true);
    }
    deck_menu.set_tag("deck");
    deck_menu.add_toggle_key('!');
    deck_menu.add_toggle_key('?');
    deck_menu.menu_action = Menu::ACT_EXECUTE;

    deck_menu.set_more(formatted_string::parse_string(
                       "Press '<w>!</w>' or '<w>?</w>' to toggle "
                       "between deck selection and description."));

    int numbers[NUM_DECKS];

    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; i++)
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(deck_status((deck_type)i),
                    deck_status((deck_type)i),
                    MEL_ITEM, 1, _deck_hotkey((deck_type)i));
        numbers[i] = i;
        me->data = &numbers[i];
        if (!deck_cards((deck_type)i))
            me->colour = COL_USELESS;

#ifdef USE_TILE
        me->add_tile(tile_def(TILEG_NEMELEX_DECK + i - FIRST_PLAYER_DECK + 1));
#endif
        deck_menu.add_entry(me);
    }

    auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();
    deck_menu.on_single_selection = [&deck_menu, &stack, to_stack](const MenuEntry& sel)
    {
        ASSERT(sel.hotkeys.size() == 1);
        deck_type selected = (deck_type) *(static_cast<int*>(sel.data));
        // Need non-const access to the selection.
        ToggleableMenuEntry* me =
            static_cast<ToggleableMenuEntry*>(deck_menu.selected_entries()[0]);

        if (deck_menu.menu_action == Menu::ACT_EXAMINE)
            describe_deck(selected);
        else if (you.props[deck_name(selected)].get_int() > 0)
        {
            you.props[deck_name(selected)]--;
            me->text = deck_status(selected);
            me->alt_text = deck_status(selected);

            card_type draw = _random_card(selected);
            stack.push_back(draw);
            string status = "Drawn so far: " + stack_contents();
            deck_menu.set_more(formatted_string::parse_string(
                       status + "\n" +
                       "Press '<w>!</w>' or '<w>?</w>' to toggle "
                       "between deck selection and description."));
        }
        return stack.size() < to_stack
               || deck_menu.menu_action == Menu::ACT_EXAMINE;
    };
    deck_menu.show(false);
}

bool stack_five(int to_stack)
{
    auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();

    while (stack.size() < to_stack)
    {
        if (crawl_state.seen_hups)
            return false;

        _draw_stack(to_stack);
    }

    StackFiveMenu menu(stack);
    MenuEntry *const title = new MenuEntry("Select two cards to swap them:", MEL_TITLE);
    menu.set_title(title);
    for (unsigned int i = 0; i < stack.size(); i++)
    {
        MenuEntry * const entry =
            new MenuEntry(card_name((card_type)stack[i].get_int()),
                          MEL_ITEM, 1, '1'+i);
#ifdef USE_TILE
        entry->add_tile(tile_def(TILEG_NEMELEX_CARD));
#endif
        menu.add_entry(entry);
    }
    menu.set_more(formatted_string::parse_string(
                "<lightgrey>Press <w>?</w> for the card descriptions"
                " or <w>Enter</w> to accept."));
    menu.show();

    std::reverse(stack.begin(), stack.end());

    return true;
}

// Draw the top four cards of an deck and play them all.
// Return false if the operation was failed/aborted along the way.
bool deck_deal()
{
    deck_type choice = _choose_deck("Deal");

    if (choice == NUM_DECKS)
        return false;

    int num_cards = deck_cards(choice);

    if (!num_cards)
    {
        mpr("That deck is empty!");
        return false;
    }

    const int num_to_deal = min(num_cards, 4);

    for (int i = 0; i < num_to_deal; ++i)
        _evoke_deck(choice, true);

    return true;
}

// Draw the next three cards, discard two and pick one.
bool deck_triple_draw()
{
    deck_type choice = _choose_deck();

    if (choice == NUM_DECKS)
        return false;

    int num_cards = deck_cards(choice);

    if (!num_cards)
    {
        mpr("That deck is empty!");
        return false;
    }

    if (num_cards < 3 && !yesno("There's fewer than three cards, "
                                "still triple draw?", false, 0))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (num_cards == 1)
    {
        // Only one card to draw, so just draw it.
        mpr("There's only one card left!");
        return _evoke_deck(choice);
    }

    const int num_to_draw = min(num_cards, 3);

    you.props[deck_name(choice)] = deck_cards(choice) - num_to_draw;

    auto& draw = you.props[NEMELEX_TRIPLE_DRAW_KEY].get_vector();
    draw.clear();

    for (int i = 0; i < num_to_draw; ++i)
        draw.push_back(_random_card(choice));

    run_uncancel(UNC_DRAW_THREE, 0);
    return true;
}

bool draw_three()
{
    auto& draws = you.props[NEMELEX_TRIPLE_DRAW_KEY].get_vector();

    int selected = -1;
    bool need_prompt_redraw = true;
    while (true)
    {
        if (need_prompt_redraw)
        {
            mpr("You draw... (choose one card, ? for their descriptions)");
            for (int i = 0; i < draws.size(); ++i)
            {
                msg::streams(MSGCH_PROMPT)
                    << msg::nocap << (static_cast<char>(i + 'a')) << " - "
                    << card_name((card_type)draws[i].get_int()) << endl;
            }
            need_prompt_redraw = false;
        }
        const int keyin = toalower(get_ch());

        if (crawl_state.seen_hups)
            return false;

        if (keyin == '?')
        {
            _describe_cards(draws);
            redraw_screen();
            need_prompt_redraw = true;
        }
        else if (keyin >= 'a' && keyin < 'a' + draws.size())
        {
            selected = keyin - 'a';
            break;
        }
        else
            canned_msg(MSG_HUH);
    }

    card_effect((card_type) draws[selected].get_int());

    return true;
}

// This is Nemelex retribution. If deal is true, use the word "deal"
// rather than "draw" (for the Deal Four out-of-cards situation).
void draw_from_deck_of_punishment(bool deal)
{
    card_type card = _random_card(DECK_OF_PUNISHMENT);

    mprf("You %s a card...", deal ? "deal" : "draw");
    card_effect(card, deal, true);
}

static int _get_power_level(int power)
{
    int power_level = x_chance_in_y(power, 900) + x_chance_in_y(power, 2700);

    // other functions in this file will break if this assertion is violated
    ASSERT(power_level >= 0 && power_level <= 2);
    return power_level;
}

// Actual card implementations follow.

static void _velocity_card(int power)
{

    const int power_level = _get_power_level(power);
    bool did_something = false;

    if (you.duration[DUR_SLOW] && x_chance_in_y(power_level, 2))
    {
        you.duration[DUR_SLOW] = 1;
        did_something = true;
    }

    if (!apply_visible_monsters([=](monster& mon)
          {
              bool affected = false;
              if (!mons_immune_magic(mon))
              {
                  const bool hostile = !mon.wont_attack();
                  const bool haste_immune = (mon.stasis()
                                             || mons_is_immotile(mon));

                  bool did_haste = false;

                  if (hostile)
                  {
                      if (x_chance_in_y(1 + power_level, 3))
                      {
                          do_slow_monster(mon, &you);
                          affected = true;
                      }
                  }
                  else //allies
                  {
                      if (!haste_immune && x_chance_in_y(power_level, 2))
                      {
                          mon.add_ench(ENCH_HASTE);
                          affected = true;
                          did_haste = true;
                      }
                  }

                  if (did_haste)
                      simple_monster_message(mon, " seems to speed up.");
              }
              return affected;
          })
        && !did_something)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
}

static void _exile_card(int power)
{
    if (player_in_branch(BRANCH_ABYSS))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // Calculate how many extra banishments you get.
    const int power_level = _get_power_level(power);
    int extra_targets = power_level + random2(1 + power_level);

    for (int i = 0; i < 1 + extra_targets; ++i)
    {
        // Pick a random monster nearby to banish (or yourself).
        monster* mon_to_banish = choose_random_nearby_monster(1);

        // Bonus banishments only banish monsters.
        if (i != 0 && !mon_to_banish)
            continue;

        if (!mon_to_banish) // Banish yourself!
        {
            banished("drawing a card");
            break;              // Don't banish anything else.
        }
        else
            mon_to_banish->banish(&you);
    }
}

static void _shaft_card(int power)
{
    const int power_level = _get_power_level(power);
    bool did_something = false;

    if (is_valid_shaft_level())
    {
        if (grd(you.pos()) == DNGN_FLOOR)
        {
            place_specific_trap(you.pos(), TRAP_SHAFT);
            trap_at(you.pos())->reveal();
            mpr("A shaft materialises beneath you!");
            did_something = true;
        }

        did_something = apply_visible_monsters([=](monster& mons)
        {
            return !mons.wont_attack()
                   && mons_is_threatening(mons)
                   && x_chance_in_y(power_level, 3)
                   && mons.do_shaft();
        }) || did_something;
    }

    if (!did_something)
        canned_msg(MSG_NOTHING_HAPPENS);
}

static int stair_draw_count = 0;

// This does not describe an actual card. Instead, it only exists to test
// the stair movement effect in wizard mode ("&c stairs").
static void _stairs_card(int /*power*/)
{
    you.duration[DUR_REPEL_STAIRS_MOVE]  = 0;
    you.duration[DUR_REPEL_STAIRS_CLIMB] = 0;

    if (feat_stair_direction(grd(you.pos())) == CMD_NO_CMD)
        you.duration[DUR_REPEL_STAIRS_MOVE]  = 1000;
    else
        you.duration[DUR_REPEL_STAIRS_CLIMB] =  500; // more annoying

    vector<coord_def> stairs_avail;

    for (radius_iterator ri(you.pos(), LOS_DEFAULT, true); ri; ++ri)
    {
        dungeon_feature_type feat = grd(*ri);
        if (feat_stair_direction(feat) != CMD_NO_CMD
            && feat != DNGN_ENTER_SHOP)
        {
            stairs_avail.push_back(*ri);
        }
    }

    if (stairs_avail.empty())
    {
        mpr("No stairs available to move.");
        return;
    }

    shuffle_array(stairs_avail);

    for (coord_def stair : stairs_avail)
        move_stair(stair, stair_draw_count % 2, false);

    stair_draw_count++;
}

static monster* _friendly(monster_type mt, int dur)
{
    return create_monster(mgen_data(mt, BEH_FRIENDLY, you.pos(), MHITYOU,
                                    MG_AUTOFOE)
                          .set_summoned(&you, dur, 0));
}

static void _damaging_card(card_type card, int power,
                           bool dealt = false)
{
    const int power_level = _get_power_level(power);
    const char *participle = dealt ? "dealt" : "drawn";

    bool done_prompt = false;
    string prompt = make_stringf("You have %s %s.", participle,
                                 card_name(card));

    dist target;
    zap_type ztype = ZAP_DEBUGGING_RAY;
    const zap_type painzaps[2] = { ZAP_AGONY, ZAP_BOLT_OF_DRAINING };
    const zap_type acidzaps[3] = { ZAP_BREATHE_ACID, ZAP_CORROSIVE_BOLT,
                                   ZAP_CORROSIVE_BOLT };
    const zap_type orbzaps[3]  = { ZAP_ISKENDERUNS_MYSTIC_BLAST, ZAP_IOOD,
                                   ZAP_IOOD };

    switch (card)
    {
    case CARD_VITRIOL:
        if (power_level == 2)
        {
            done_prompt = true;
            mpr(prompt);
            mpr("You radiate a wave of entropy!");
            apply_visible_monsters([](monster& mons)
            {
                return !mons.wont_attack()
                       && mons_is_threatening(mons)
                       && coinflip()
                       && mons.corrode_equipment();
            });
        }
        ztype = acidzaps[power_level];
        break;

    case CARD_ORB:
        ztype = orbzaps[power_level];
        break;

    case CARD_PAIN:
        if (power_level == 2)
        {
            mpr(prompt);

            if (monster *ghost = _friendly(MONS_FLAYED_GHOST, 3))
            {
                apply_visible_monsters([&, ghost](monster& mons)
                {
                    if (mons.wont_attack()
                        || !(mons.holiness() & MH_NATURAL))
                    {
                        return false;
                    }


                    flay(*ghost, mons, mons.hit_points * 2 / 5);
                    return true;
                }, ghost->pos());

                ghost->foe = MHITYOU; // follow you around (XXX: rethink)
                return;
            }
            // else, fallback to level 1
        }

        ztype = painzaps[min(power_level, (int)ARRAYSZ(painzaps)-1)];
        break;

    default:
        break;
    }

    bolt beam;
    beam.range = LOS_RADIUS;

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    if (!done_prompt)
        args.top_prompt = prompt;

    // Confirm aborts as they waste the card.
    prompt = make_stringf("Aiming: %s", card_name(card));
    while (!(spell_direction(target, beam, &args)
            && player_tracer(ZAP_DEBUGGING_RAY, power/6, beam)))
    {
        if (crawl_state.seen_hups
            || yesno("Really abort (and waste the card)?", false, 0))
        {
            canned_msg(MSG_OK);
            return;
        }
        args.top_prompt = prompt;
    }

    if (ztype == ZAP_IOOD)
    {
        if (power_level == 1)
        {
            cast_iood(&you, power/6, &beam, 0, 0,
                      mgrd(beam.target), false, false);
        }
        else
            cast_iood_burst(power/6, beam.target);
    }
    else
        zapping(ztype, power/6, beam);
}

static void _elixir_card(int power)
{
    int power_level = _get_power_level(power);

    you.duration[DUR_ELIXIR_HEALTH] = 0;
    you.duration[DUR_ELIXIR_MAGIC] = 0;

    switch (power_level)
    {
    case 0:
        if (coinflip())
            you.set_duration(DUR_ELIXIR_HEALTH, 1 + random2(3));
        else
            you.set_duration(DUR_ELIXIR_MAGIC, 3 + random2(5));
        break;
    case 1:
        if (you.hp * 2 < you.hp_max)
            you.set_duration(DUR_ELIXIR_HEALTH, 3 + random2(3));
        else
            you.set_duration(DUR_ELIXIR_MAGIC, 10);
        break;
    default:
        you.set_duration(DUR_ELIXIR_HEALTH, 10);
        you.set_duration(DUR_ELIXIR_MAGIC, 10);
    }

    if (you.duration[DUR_ELIXIR_HEALTH] && you.duration[DUR_ELIXIR_MAGIC])
        mpr("You begin rapidly regenerating health and magic.");
    else if (you.duration[DUR_ELIXIR_HEALTH])
        mpr("You begin rapidly regenerating.");
    else
        mpr("You begin rapidly regenerating magic.");

    apply_visible_monsters([=](monster& mon)
    {
        if (mon.wont_attack())
        {
            const int hp = mon.max_hit_points / (4 - power_level);
            if (mon.heal(hp + random2avg(hp, 2)))
               simple_monster_message(mon, " is healed.");
        }
        return true;
    });
}

// Special case for *your* god, maybe?
static void _godly_wrath()
{
    for (int tries = 0; tries < 100; tries++)
    {
        god_type god = random_god();

        // Don't recursively make player draw from the Deck of Punishment.
        if (god != GOD_NEMELEX_XOBEH && divine_retribution(god))
            return; // Stop once we find a god willing to punish the player.
    }

    mpr("You somehow manage to escape divine attention...");
}

static void _summon_demon_card(int power)
{
    const int power_level = _get_power_level(power);
    // one demon (potentially hostile), and one other demonic creature (always
    // friendly)
    monster_type dct, dct2;
    switch (power_level)
    {
    case 0:
        dct = random_demon_by_tier(4);
        dct2 = MONS_HELL_HOUND;
        break;
    case 1:
        dct = random_demon_by_tier(3);
        dct2 = MONS_RAKSHASA;
        break;
    default:
        dct = random_demon_by_tier(2);
        dct2 = MONS_PANDEMONIUM_LORD;
    }

    // FIXME: The manual testing for message printing is there because
    // we can't rely on create_monster() to do it for us. This is
    // because if you are completely surrounded by walls, create_monster()
    // will never manage to give a position which isn't (-1,-1)
    // and thus not print the message.
    // This hack appears later in this file as well.

    const bool hostile = one_chance_in(power_level + 4);

    if (!create_monster(mgen_data(dct, hostile ? BEH_HOSTILE : BEH_FRIENDLY,
                                  you.pos(), MHITYOU, MG_AUTOFOE)
                        .set_summoned(&you, 5 - power_level, 0)))
    {
        mpr("You see a puff of smoke.");
    }
    else if (hostile
             && mons_class_flag(dct, M_INVIS)
             && !you.can_see_invisible())
    {
        mpr("You sense the presence of something unfriendly.");
    }

    _friendly(dct2, 5 - power_level);
}

static void _elements_card(int power)
{

    const int power_level = _get_power_level(power);
    const monster_type element_list[][3] =
    {
        {MONS_RAIJU, MONS_WIND_DRAKE, MONS_SHOCK_SERPENT},
        {MONS_BASILISK, MONS_CATOBLEPAS, MONS_IRON_GOLEM},
        {MONS_FIRE_VORTEX, MONS_MOLTEN_GARGOYLE, MONS_FIRE_DRAGON},
        {MONS_ICE_BEAST, MONS_POLAR_BEAR, MONS_ICE_DRAGON}
    };

    int start = random2(ARRAYSZ(element_list));
    for (int i = 0; i < 3; ++i)
    {
        _friendly(element_list[start % ARRAYSZ(element_list)][power_level],
                  power_level + 2);
        start++;
    }

}

static void _summon_dancing_weapon(int power)
{
    const int power_level = _get_power_level(power);

    monster *mon =
        create_monster(
            mgen_data(MONS_DANCING_WEAPON, BEH_FRIENDLY, you.pos(), MHITYOU,
                      MG_AUTOFOE).set_summoned(&you, power_level + 2, 0),
            false);

    if (!mon)
    {
        mpr("You see a puff of smoke.");
        return;
    }

    // Override the weapon.
    ASSERT(mon->weapon() != nullptr);
    item_def& wpn(*mon->weapon());

    switch (power_level)
    {
    case 0:
        // Wimpy, negative-enchantment weapon.
        wpn.plus = random2(3) - 2;
        wpn.sub_type = random_choose(WPN_QUARTERSTAFF, WPN_HAND_AXE);

        set_item_ego_type(wpn, OBJ_WEAPONS,
                          random_choose(SPWPN_VENOM, SPWPN_NORMAL));
        break;
    case 1:
        // This is getting good.
        wpn.plus = random2(4) - 1;
        wpn.sub_type = random_choose(WPN_LONG_SWORD, WPN_TRIDENT);

        if (coinflip())
        {
            set_item_ego_type(wpn, OBJ_WEAPONS,
                              random_choose(SPWPN_FLAMING, SPWPN_FREEZING));
        }
        else
            set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_NORMAL);
        break;
    default:
        // Rare and powerful.
        wpn.plus = random2(4) + 2;
        wpn.sub_type = random_choose(WPN_DEMON_TRIDENT, WPN_EXECUTIONERS_AXE);

        set_item_ego_type(wpn, OBJ_WEAPONS,
                          random_choose(SPWPN_SPEED, SPWPN_ELECTROCUTION));
    }

    item_colour(wpn); // this is probably not needed

    // sometimes give a randart instead
    if (one_chance_in(3))
    {
        make_item_randart(wpn, true);
        set_ident_flags(wpn, ISFLAG_KNOW_PROPERTIES| ISFLAG_KNOW_TYPE);
    }

    // Don't leave a trail of weapons behind. (Especially not randarts!)
    mon->flags |= MF_HARD_RESET;

    ghost_demon newstats;
    newstats.init_dancing_weapon(wpn, power / 4);

    mon->set_ghost(newstats);
    mon->ghost_demon_init();
}

static void _summon_flying(int power)
{
    const int power_level = _get_power_level(power);

    const monster_type flytypes[] =
    {
        MONS_INSUBSTANTIAL_WISP, MONS_WYVERN, MONS_KILLER_BEE,
        MONS_VAMPIRE_MOSQUITO, MONS_HORNET
    };
    const int num_flytypes = ARRAYSZ(flytypes);

    // Choose what kind of monster.
    monster_type result;
    const int how_many = 2 + random2(3) + power_level * 3;
    bool hostile_invis = false;

    do
    {
        result = flytypes[random2(num_flytypes - 2) + power_level];
    }
    while (is_good_god(you.religion) && result == MONS_VAMPIRE_MOSQUITO);

    for (int i = 0; i < how_many; ++i)
    {
        const bool hostile = one_chance_in(power_level + 4);

        create_monster(
            mgen_data(result,
                      hostile ? BEH_HOSTILE : BEH_FRIENDLY, you.pos(), MHITYOU,
                      MG_AUTOFOE).set_summoned(&you, 3, 0));

        if (hostile && mons_class_flag(result, M_INVIS) && !you.can_see_invisible())
            hostile_invis = true;
    }

    if (hostile_invis)
        mpr("You sense the presence of something unfriendly.");
}

static void _summon_rangers(int power)
{
    const int power_level = _get_power_level(power);
    const monster_type dctr  = random_choose(MONS_CENTAUR, MONS_YAKTAUR),
                       dctr2 = random_choose(MONS_CENTAUR_WARRIOR, MONS_FAUN),
                       dctr3 = random_choose(MONS_YAKTAUR_CAPTAIN,
                                             MONS_NAGA_SHARPSHOOTER),
                       dctr4 = random_choose(MONS_SATYR,
                                             MONS_MERFOLK_JAVELINEER,
                                             MONS_DEEP_ELF_MASTER_ARCHER);

    const monster_type base_choice = power_level == 2 ? dctr2 :
                                                        dctr;
    monster_type placed_choice  = power_level == 2 ? dctr3 :
                                  power_level == 1 ? dctr2 :
                                                     dctr;
    const bool extra_monster = coinflip();

    if (!extra_monster && power_level > 0)
        placed_choice = power_level == 2 ? dctr4 : dctr3;

    for (int i = 0; i < 1 + extra_monster; ++i)
        _friendly(base_choice, 5 - power_level);

    _friendly(placed_choice, 5 - power_level);
}

static void _cloud_card(int power)
{
    const int power_level = _get_power_level(power);
    bool something_happened = false;

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);
        cloud_type cloudy;

        switch (power_level)
        {
            case 0: cloudy = !one_chance_in(5) ? CLOUD_MEPHITIC : CLOUD_POISON;
                    break;

            case 1: cloudy = coinflip() ? CLOUD_COLD : CLOUD_FIRE;
                    break;

            case 2: cloudy = coinflip() ? CLOUD_ACID: CLOUD_MIASMA;
                    break;

            default: cloudy = CLOUD_DEBUGGING;
        }

        if (!mons || mons->wont_attack() || !mons_is_threatening(*mons))
            continue;

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            // don't place clouds on the player or monsters
            if (*ai == you.pos() || monster_at(*ai))
                continue;

            if (grd(*ai) == DNGN_FLOOR && !cloud_at(*ai))
            {
                const int cloud_power = 5 + random2((power_level + 1) * 3);
                place_cloud(cloudy, *ai, cloud_power, &you);

                if (you.see_cell(*ai))
                    something_happened = true;
            }
        }
    }

    if (something_happened)
        mpr("Clouds appear around you!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _storm_card(int power)
{
    const int power_level = _get_power_level(power);

    wind_blast(&you, (power_level + 1) * 66, coord_def(), true);
    redraw_screen(); // Update monster positions

    // 1-3, 4-6, 7-9
    const int max_explosions = random_range((power_level * 3) + 1, (power_level + 1) * 3);
    // Select targets based on simultaneously running max_explosions resivoir
    // samples from the radius iterator over valid targets.
    //
    // Once the possible targets are drawn, the result is deduplicated into a
    // set of targets.
    vector<coord_def> target_draws (max_explosions, you.pos());
    int valid_targets = 0;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS, true); ri; ++ri)
    {
        if (grid_distance(*ri, you.pos()) > 3 && !cell_is_solid(*ri))
        {
            ++valid_targets;
            for (int i = 0; i < max_explosions; ++i)
            {
                if (one_chance_in(valid_targets))
                    target_draws[i] = *ri;
            }
        }
    }

    unordered_set<coord_def> targets (target_draws.begin(), target_draws.end());
    targets.erase(you.pos());

    bool heard = false;
    for (auto p : targets)
    {
        bolt beam;
        beam.is_tracer         = false;
        beam.is_explosion      = true;
        beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
        beam.name              = "electrical discharge";
        beam.aux_source        = "the storm";
        beam.explode_noise_msg = "You hear a clap of thunder!";
        beam.real_flavour      = beam.flavour;
        beam.colour            = LIGHTCYAN;
        beam.source_id         = MID_PLAYER;
        beam.thrower           = KILL_YOU;
        beam.is_explosion      = true;
        beam.ex_size           = 3;
        beam.damage            = dice_def(3, 9 + 9 * power_level);
        beam.source = p;
        beam.target = p;
        beam.explode();
        heard = heard || player_can_hear(p);
    }
    // Lots of loud bangs, even if everything is silenced get a message.
    // Thunder comes after the animation runs.
    if (targets.size() > 0)
    {
        vector<string> thunder_adjectives = { "mighty",
                                              "violent",
                                              "cataclysmic" };
        mprf("You %s %s%s peal%s of thunder!",
              heard ? "hear" : "feel",
              targets.size() > 1 ? "" : "a ",
              thunder_adjectives[power_level].c_str(),
              targets.size() > 1 ? "s" : "");
    }
}

static void _illusion_card(int power)
{
    const int power_level = _get_power_level(power);
    monster* mon = get_free_monster();

    if (!mon || monster_at(you.pos()))
        return;

    mon->type = MONS_PLAYER;
    mon->behaviour = BEH_SEEK;
    mon->attitude = ATT_FRIENDLY;
    mon->set_position(you.pos());
    mon->mid = MID_PLAYER;
    mgrd(you.pos()) = mon->mindex();

    mons_summon_illusion_from(mon, (actor *)&you, SPELL_NO_SPELL, power_level);
    mon->reset();
}

static void _degeneration_card(int power)
{
    const int power_level = _get_power_level(power);

    if (!apply_visible_monsters([power_level](monster& mons)
           {
               if (mons.wont_attack() || !mons_is_threatening(mons))
                   return false;

               if (!x_chance_in_y((power_level + 1) * 5 + random2(5),
                                  mons.get_hit_dice()))
               {
                   return false;
               }

               if (mons.can_polymorph())
               {
                   mons.polymorph(PPT_LESS);
                   mons.malmutate("");
               }
               else
               {
                   const int daze_time = (5 + 5 * power_level) * BASELINE_DELAY;
                   mons.add_ench(mon_enchant(ENCH_DAZED, 0, &you, daze_time));
                   simple_monster_message(mons,
                                          " is dazed by the mutagenic energy.");
               }
               return true;
           }))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
}

static void _wild_magic_card(int power)
{
    const int power_level = _get_power_level(power);
    int num_affected = 0;

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);

        if (!mons || mons->wont_attack() || !mons_is_threatening(*mons))
            continue;

        if (x_chance_in_y((power_level + 1) * 5 + random2(5),
                           mons->get_hit_dice()))
        {
            spschool type = random_choose(spschool::conjuration,
                                          spschool::fire,
                                          spschool::ice,
                                          spschool::earth,
                                          spschool::air,
                                          spschool::poison);

            MiscastEffect(mons, actor_by_mid(MID_YOU_FAULTLESS),
                          {miscast_source::deck}, type,
                          random2(power/15) + 5, random2(power),
                          "a card of wild magic");

            num_affected++;
        }
    }

    if (num_affected > 0)
    {
        int mp = 0;

        for (int i = 0; i < num_affected; ++i)
            mp += random2(5);

        mpr("You feel a surge of magic.");
        if (mp && you.magic_points < you.max_magic_points)
        {
            inc_mp(mp);
            canned_msg(MSG_GAIN_MAGIC);
        }
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _torment_card()
{
    if (you.undead_or_demonic())
        holy_word_player(HOLY_WORD_CARD);
    else
        torment_player(&you, TORMENT_CARDS);
}

static void _mercenary_card(int power)
{
    const int power_level = _get_power_level(power);
    const monster_type merctypes[] =
    {
        MONS_BIG_KOBOLD, MONS_MERFOLK, MONS_NAGA,
        MONS_TENGU, MONS_DEEP_ELF_MAGE, MONS_ORC_KNIGHT,
        RANDOM_BASE_DEMONSPAWN, MONS_OGRE_MAGE, MONS_MINOTAUR,
        RANDOM_BASE_DRACONIAN, MONS_DEEP_ELF_BLADEMASTER,
    };

    int merc;
    monster* mon;
    bool hated = you.get_mutation_level(MUT_NO_LOVE);

    while (1)
    {
        merc = power_level + random2(3 * (power_level + 1));
        ASSERT(merc < (int)ARRAYSZ(merctypes));

        mgen_data mg(merctypes[merc], BEH_HOSTILE,
            you.pos(), MHITYOU, MG_FORCE_BEH, you.religion);

        mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

        // This is a bit of a hack to use give_monster_proper_name to feed
        // the mgen_data, but it gets the job done.
        monster tempmon;
        tempmon.type = merctypes[merc];
        if (give_monster_proper_name(tempmon, false))
            mg.mname = tempmon.mname;
        else
            mg.mname = make_name();
        // This is used for giving the merc better stuff in mon-gear.
        mg.props[MERCENARY_FLAG] = true;
        mg.props["mercenary items"] = true;

        mon = create_monster(mg);

        if (!mon)
        {
            mpr("You see a puff of smoke.");
            return;
        }

        // always hostile, don't try to find a good one
        if (hated)
            break;
        if (player_will_anger_monster(*mon))
        {
            dprf("God %s doesn't like %s, retrying.",
                god_name(you.religion).c_str(), mon->name(DESC_THE).c_str());
            monster_die(*mon, KILL_RESET, NON_MONSTER);
            continue;
        }
        else
            break;
    }

    mon->props["dbname"].get_string() = mons_class_name(merctypes[merc]);

    redraw_screen(); // We want to see the monster while it's asking to be paid.

    if (hated)
    {
        simple_monster_message(*mon, " is unwilling to work for you!");
        return;
    }

    const int fee = fuzz_value(exper_value(*mon), 15, 15);
    if (fee > you.gold)
    {
        mprf("You cannot afford %s fee of %d gold!",
            mon->name(DESC_ITS).c_str(), fee);
        simple_monster_message(*mon, " attacks!");
        return;
    }

    mon->props["mercenary_fee"] = fee;
    run_uncancel(UNC_MERCENARY, mon->mid);
}

bool recruit_mercenary(int mid)
{
    monster* mon = monster_by_mid(mid);
    if (!mon)
        return true; // wut?

    int fee = mon->props["mercenary_fee"].get_int();
    const string prompt = make_stringf("Pay %s fee of %d gold?",
        mon->name(DESC_ITS).c_str(), fee);
    bool paid = yesno(prompt.c_str(), false, 0);
    const string message = make_stringf("Hired %s for %d gold.",
        mon->full_name(DESC_A).c_str(), fee);
    if (crawl_state.seen_hups)
        return false;

    mon->props.erase("mercenary_fee");
    if (!paid)
    {
        simple_monster_message(*mon, " attacks!");
        return true;
    }

    simple_monster_message(*mon, " joins your ranks!");
    for (mon_inv_iterator ii(*mon); ii; ++ii)
        ii->flags &= ~ISFLAG_SUMMONED;
    mon->flags &= ~MF_HARD_RESET;
    mon->attitude = ATT_FRIENDLY;
    mons_att_changed(mon);
    take_note(Note(NOTE_MESSAGE, 0, 0, message), true);
    you.del_gold(fee);
    return true;
}

static void _alchemist_card(int power)
{
    const int power_level = _get_power_level(power);
    const int gold_max = min(you.gold, random2avg(100, 2) * (1 + power_level));
    int gold_used = 0;

    dprf("%d gold available to spend.", gold_max);

    // Spend some gold to regain health.
    int hp = min((gold_max - gold_used) / 3, you.hp_max - you.hp);
    if (hp > 0)
    {
        you.del_gold(hp * 2);
        inc_hp(hp);
        gold_used += hp * 2;
        canned_msg(MSG_GAIN_HEALTH);
        dprf("Gained %d health, %d gold remaining.", hp, gold_max - gold_used);
    }
    // Maybe spend some more gold to regain magic.
    int mp = min((gold_max - gold_used) / 5,
        you.max_magic_points - you.magic_points);
    if (mp > 0 && x_chance_in_y(power_level + 1, 5))
    {
        you.del_gold(mp * 5);
        inc_mp(mp);
        gold_used += mp * 5;
        canned_msg(MSG_GAIN_MAGIC);
        dprf("Gained %d magic, %d gold remaining.", mp, gold_max - gold_used);
    }

    if (gold_used > 0)
        mprf("%d of your gold pieces vanish!", gold_used);
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _sage_card(int power)
{
    const int power_level = _get_power_level(power);
    int c;                      // how much to weight your skills
    if (power_level == 0)
        c = 0;
    else if (power_level == 1)
        c = random2(10) + 1;
    else
        c = 10;

    // FIXME: yet another reproduction of random_choose_weighted
    // Ah for Python:
    // skill = random_choice([x*(40-x)*c/10 for x in skill_levels])
    int totalweight = 0;
    skill_type result = SK_NONE;
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type s = static_cast<skill_type>(i);
        if (skill_name(s) == NULL || is_useless_skill(s))
            continue;

        if (you.skills[s] < MAX_SKILL_LEVEL)
        {
            // Choosing a skill is likelier if you are somewhat skilled in it.
            const int curweight = 1 + you.skills[s] * (40 - you.skills[s]) * c;
            totalweight += curweight;
            if (x_chance_in_y(curweight, totalweight))
                result = s;
        }
    }

    if (result == SK_NONE)
        mpr("You feel omnipotent.");  // All skills maxed.
    else
    {
        int xp = exp_needed(min<int>(you.max_level, 27) + 1)
            - exp_needed(min<int>(you.max_level, 27));
        xp = xp / 10 + random2(xp / 4);

        // There may be concurrent sages for the same skill, with different
        // bonus multipliers.
        you.sage_skills.push_back(result);
        you.sage_xp.push_back(xp);
        you.sage_bonus.push_back(power / 25);
        mprf(MSGCH_PLAIN, "You feel studious about %s.", skill_name(result));
        dprf("Will redirect %d xp, bonus = %d%%\n", xp, (power / 25) * 2);
    }
}


// Actual card implementations follow.
static void _portal_card(int power)
{
    const int control_level = _get_power_level(power);
    bool controlled = false;

    if (x_chance_in_y(control_level, 2))
        controlled = true;

    int threshold = 9;
    const bool was_controlled = you.duration[DUR_CONTROL_TELEPORT] > 0;
    const bool short_control = (you.duration[DUR_CONTROL_TELEPORT] > 0
        && you.duration[DUR_CONTROL_TELEPORT]
        < threshold * BASELINE_DELAY);

    if (controlled && (!was_controlled || short_control))
        you.set_duration(DUR_CONTROL_TELEPORT, threshold); // Long enough to kick in.

    if (x_chance_in_y(control_level, 2))
        you.blink(true);

    you_teleport();
}

static void _trowel_card(int)
{
    if (!crawl_state.game_standard_levelgen())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    coord_def p;
    for (distance_iterator di(you.pos(), true, false); di; ++di)
    {
        if (cell_is_solid(*di) || feat_is_critical(grd(*di)))
            continue;
        p = *di;
        break;
    }

    if (p.origin()) // can't happen outside wizmode
        return mpr("The dungeon trembles momentarily.");

    // Vetoes are done too late, should not pass random_map_for_tag()
    // at all.  Thus, allow retries.
    int tries;
    for (tries = 100; tries > 0; tries--)
    {
        // Generate a portal to something.
        const map_def* map = random_map_for_tag("trowel_portal", true, true);

        if (!map)
            break;

        {
            no_messages n;
            if (dgn_safe_place_map(map, true, true, p))
            {
                tries = -1; // hrm no_messages
                break;
            }
        }
    }
    if (tries > -1)
        mpr("A portal flickers into view, then vanishes.");
    else
        mpr("A mystic portal forms.");
}

static void _experience_card(int power)
{
    const int power_level = _get_power_level(power);

    if (you.experience_level < 27)
        mpr("You feel more experienced.");
    else
        mpr("You feel knowledgeable.");

    skill_menu(SKMF_EXPERIENCE, 200 + power * 50);

    // After level 27, boosts you get don't get increased (matters for
    // charging V:$ with no rN+++ and for felids).
    const int xp_cap = exp_needed(1 + you.experience_level)
        - exp_needed(you.experience_level);

    // power_level 2 means automatic level gain.
    if (power_level == 2 && you.experience_level < 27)
        adjust_level(1);
    else
    {
        // Likely to give a level gain (power of ~500 is reasonable
        // at high levels even for non-Nemelexites, so 50,000 XP.)
        // But not guaranteed.
        // Overrides archmagi effect, like potions of experience.
        you.experience += min(xp_cap, power * 100);
        level_change();
    }
}

static void _helm_card(int power)
{
    const int power_level = _get_power_level(power);
    bool do_phaseshift = false;
    bool do_armour = false;
    bool do_shield = false;
    bool do_resistance = false;

    // Chances are cumulative.
    if (power_level >= 2)
    {
        if (coinflip()) do_phaseshift = true;
        if (coinflip()) do_armour = true;
        if (coinflip()) do_shield = true;
        do_resistance = true;
    }
    if (power_level >= 1)
    {
        if (coinflip()) do_phaseshift = true;
        if (coinflip()) do_armour = true;
        if (coinflip()) do_shield = true;
    }
    if (power_level >= 0)
    {
        if (coinflip())
            do_phaseshift = true;
        else
            do_armour = true;
    }

    if (do_phaseshift)
        cast_phase_shift(random2(power / 4), false);
    if (do_armour)
    {
        int pow = random2(power / 4);
        if (you.duration[DUR_MAGIC_ARMOUR] == 0)
            mpr("You gain magical protection.");
        you.increase_duration(DUR_MAGIC_ARMOUR,
            10 + random2(pow) + random2(pow), 50);
        you.props[MAGIC_ARMOUR_KEY] = pow;
        you.redraw_armour_class = true;
    }
    if (do_resistance)
    {
        mpr("You feel resistant.");
        you.increase_duration(DUR_RESISTANCE, random2(power / 7) + 1);
    }
    if (do_shield)
    {
        int pow = random2(power / 4);
        if (you.duration[DUR_MAGIC_SHIELD] == 0)
            mpr("A magical shield forms in front of you.");
        you.props[MAGIC_SHIELD_KEY] = pow;
        you.increase_duration(DUR_MAGIC_SHIELD, random2(power / 6) + 1);
        you.redraw_armour_class = true;
    }
}

static string _god_wrath_stat_check(string cause_orig)
{
    string cause = cause_orig;

    if (crawl_state.is_god_acting())
    {
        god_type which_god = crawl_state.which_god_acting();
        if (crawl_state.is_god_retribution())
            cause = "the wrath of " + god_name(which_god);
        else if (which_god == GOD_XOM)
            cause = "the capriciousness of Xom";
        else
            cause = "the 'helpfulness' of " + god_name(which_god);
    }

    return cause;
}

static void _shuffle_card(int)
{
    int perm[] = { 0, 1, 2 };
    COMPILE_CHECK(ARRAYSZ(perm) == NUM_STATS);
    shuffle_array(perm, NUM_STATS);

    FixedVector<int8_t, NUM_STATS> new_base;
    for (int i = 0; i < NUM_STATS; ++i)
        new_base[perm[i]] = you.base_stats[i];

    const string cause = _god_wrath_stat_check("the Shuffle card");

    for (int i = 0; i < NUM_STATS; ++i)
    {
        modify_stat(static_cast<stat_type>(i),
            new_base[i] - you.base_stats[i],
            true);
    }

    char buf[128];
    snprintf(buf, sizeof(buf),
        "Shuffle card: Str %d[%d], Int %d[%d], Dex %d[%d]",
        you.base_stats[STAT_STR], you.strength(false),
        you.base_stats[STAT_INT], you.intel(false),
        you.base_stats[STAT_DEX], you.dex(false));
    take_note(Note(NOTE_MESSAGE, 0, 0, buf));
}

// Punishment cards don't have their power adjusted depending on Nemelex piety,
// and are based on experience level instead of invocations skill.
// Max power = 200 * (2700+2500) / 2700 + 243 + 300 = 928
// Min power = 1 * 2501 / 2700 + 1 + 0 = 2
static int _card_power(bool punishment)
{
    if (punishment)
        return you.experience_level * 18;

    int result = you.piety;
    result *= you.skill(SK_EVOCATIONS, 100) + 2500;
    result /= 2700;
    result += you.skill(SK_EVOCATIONS, 9);
    result += (you.piety * 3) / 2;

    return result;
}

void card_effect(card_type which_card,
                 bool dealt,
                 bool punishment, bool tell_card)
{
    const char *participle = dealt ? "dealt" : "drawn";
    const int power = _card_power(punishment);

    dprf("Card power: %d", power);

    if (tell_card)
    {
        // These card types will usually give this message in the targeting
        // prompt, and the cases where they don't are handled specially.
        if (which_card != CARD_VITRIOL
            && which_card != CARD_PAIN
            && which_card != CARD_ORB)
        {
            mprf("You have %s %s.", participle, card_name(which_card));
        }
    }

    switch (which_card)
    {
    case CARD_VELOCITY:         _velocity_card(power); break;
    case CARD_EXILE:            _exile_card(power); break;
    case CARD_ELIXIR:           _elixir_card(power); break;
    case CARD_STAIRS:           _stairs_card(power); break;
    case CARD_SHAFT:            _shaft_card(power); break;
    case CARD_TOMB:             entomb(10 + power/20 + random2(power/4)); break;
    case CARD_WRAITH:           drain_player(power / 4, false, true); break;
    case CARD_WRATH:            _godly_wrath(); break;
    case CARD_SUMMON_DEMON:     _summon_demon_card(power); break;
    case CARD_ELEMENTS:         _elements_card(power); break;
    case CARD_RANGERS:          _summon_rangers(power); break;
    case CARD_SUMMON_WEAPON:    _summon_dancing_weapon(power); break;
    case CARD_SUMMON_FLYING:    _summon_flying(power); break;
    case CARD_TORMENT:          _torment_card(); break;
    case CARD_CLOUD:            _cloud_card(power); break;
    case CARD_STORM:            _storm_card(power); break;
    case CARD_ILLUSION:         _illusion_card(power); break;
    case CARD_DEGEN:            _degeneration_card(power); break;
    case CARD_WILD_MAGIC:       _wild_magic_card(power); break;
    case CARD_XOM:              xom_acts(5 + random2(power / 10)); break;
    case CARD_MERCENARY:        _mercenary_card(power); break;
    case CARD_ALCHEMIST:        _alchemist_card(power); break;
    case CARD_BARGAIN:          you.increase_duration(DUR_BARGAIN, random2(power) + random2(power) + 2); break;
    case CARD_SAGE:             _sage_card(power); break;
    case CARD_PORTAL:           _portal_card(power); break;
    case CARD_TROWEL:           _trowel_card(power); break;
    case CARD_EXPERIENCE:       _experience_card(power); break;
    case CARD_HELM:             _helm_card(power); break;
    case CARD_SHUFFLE:          _shuffle_card(power); break;
    case CARD_VITRIOL:
    case CARD_PAIN:
    case CARD_ORB:
        _damaging_card(which_card, power, dealt);
        break;

    case CARD_FAMINE:
        if (you_foodless())
            mpr("You feel rather smug.");
        else
            set_hunger(min(you.hunger, HUNGER_STARVING / 2), true);
        break;

    case CARD_SWINE:
        if (transform(5 + power/10 + random2(power/10), transformation::pig, true))
            you.transform_uncancellable = true;
        else
            mpr("You feel a momentary urge to oink.");
        break;


    case NUM_CARDS:
        // The compiler will complain if any card remains unhandled.
        mprf("You have %s a buggy card!", participle);
        break;
    }
}

/**
 * Return the appropriate name for a known deck of the given type.
 *
 * @param sub_type  The type of deck in question.
 * @return          A name, e.g. "deck of destruction".
 *                  If the given type isn't a deck, return "deck of bugginess".
 */
string deck_name(deck_type deck)
{
    if (deck == DECK_STACK)
        return "stacked deck";
    const deck_type_data *deck_data = map_find(all_decks, deck);
    const string name = deck_data ? deck_data->name : "bugginess";
    return "deck of " + name;
}

#if TAG_MAJOR_VERSION == 34
bool is_deck_type(uint8_t sub_type)
{
    return (MISC_FIRST_DECK <= sub_type && sub_type <= MISC_LAST_DECK)
        || sub_type == MISC_DECK_OF_ODDITIES
        || sub_type == MISC_DECK_UNKNOWN;
}

bool is_deck(const item_def &item)
{
    return item.base_type == OBJ_MISCELLANY
           && is_deck_type(item.sub_type);
}

void reclaim_decks_on_level()
{
    for (auto &item : mitm)
        if (item.defined() && is_deck(item))
            destroy_item(item.index());
}

static void _reclaim_inventory_decks()
{
    for (auto &item : you.inv)
        if (item.defined() && is_deck(item))
            dec_inv_item_quantity(item.link, 1);
}

void reclaim_decks()
{
    add_daction(DACT_RECLAIM_DECKS);
    _reclaim_inventory_decks();
}
#endif
