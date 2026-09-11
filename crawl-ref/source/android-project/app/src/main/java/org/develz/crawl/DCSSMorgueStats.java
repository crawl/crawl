package org.develz.crawl;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class DCSSMorgueStats {
    private final List<DCSSMorgueGame> newestFirst;
    private final int games;
    private final int wins;
    private final long totalScore;
    private final long bestScore;
    private final int highestXl;
    private final long totalTurns;
    private final long totalDurationSeconds;
    private final int totalWinRunes;
    private final int bestRunes;
    private final int currentWinStreak;
    private final int bestWinStreak;
    private final DCSSMorgueGame bestScoreGame;
    private final DCSSMorgueGame fastestWinByTurns;
    private final DCSSMorgueGame fastestWinByDuration;
    private final List<DCSSMorgueBreakdown> speciesBreakdowns;
    private final List<DCSSMorgueBreakdown> backgroundBreakdowns;
    private final List<DCSSMorgueBreakdown> godBreakdowns;
    private final List<DCSSMorgueBreakdown> comboBreakdowns;
    private final List<DCSSMorgueBreakdown> deathBreakdowns;
    private final List<DCSSMorgueBreakdown> deathPlaceBreakdowns;
    private final List<DCSSMorgueBreakdown> versionBreakdowns;
    private final List<DCSSMorgueBreakdown> monthBreakdowns;
    private final int distinctSpeciesPlayed;
    private final int playableSpeciesCount;
    private final int playableBackgroundCount;
    private final int availableGodCount;
    private final int playableComboCount;
    private final int distinctGodsWon;
    private final int distinctSpeciesWon;
    private final int distinctBackgroundsPlayed;
    private final int distinctBackgroundsWon;
    private final int distinctCombosPlayed;
    private final int distinctCombosWon;

    public DCSSMorgueStats(List<DCSSMorgueGame> games) {
        this(games, new DCSSMorgueRoster(0, 0, 0, 0));
    }

    public DCSSMorgueStats(List<DCSSMorgueGame> games, DCSSMorgueRoster roster) {
        newestFirst = new ArrayList<>(games);
        Collections.sort(newestFirst, new Comparator<DCSSMorgueGame>() {
            @Override
            public int compare(DCSSMorgueGame left, DCSSMorgueGame right) {
                return right.getTimestamp().compareTo(left.getTimestamp());
            }
        });
        this.games = newestFirst.size();
        long score = 0;
        long turns = 0;
        long duration = 0;
        long best = 0;
        int bestXl = 0;
        int winCount = 0;
        int winRunes = 0;
        int runesBest = 0;
        DCSSMorgueGame scoreRecord = null;
        DCSSMorgueGame turnRecord = null;
        DCSSMorgueGame durationRecord = null;
        Map<String, Counter> species = new HashMap<>();
        Map<String, Counter> backgrounds = new HashMap<>();
        Map<String, Counter> gods = new HashMap<>();
        Map<String, Counter> combos = new HashMap<>();
        Map<String, Counter> deaths = new HashMap<>();
        Map<String, Counter> deathPlaces = new HashMap<>();
        Map<String, Counter> versions = new HashMap<>();
        Map<String, Counter> months = new HashMap<>();
        Set<String> playedSpecies = new HashSet<>();
        Set<String> wonSpecies = new HashSet<>();
        Set<String> playedBackgrounds = new HashSet<>();
        Set<String> wonBackgrounds = new HashSet<>();
        Set<String> playedCombos = new HashSet<>();
        Set<String> wonCombos = new HashSet<>();
        Set<String> wonGods = new HashSet<>();

        for (DCSSMorgueGame game : newestFirst) {
            String combo = game.getSpecies() + " " + game.getBackground();
            boolean win = game.getOutcome() == DCSSMorgueGame.Outcome.WIN;
            score += game.getScore();
            turns += game.getTurns();
            duration += game.getDurationSeconds();
            best = Math.max(best, game.getScore());
            bestXl = Math.max(bestXl, game.getXl());
            if (scoreRecord == null || game.getScore() > scoreRecord.getScore()) {
                scoreRecord = game;
            }
            if (win) {
                winCount++;
                winRunes += game.getRunes();
                runesBest = Math.max(runesBest, game.getRunes());
                if (turnRecord == null || game.getTurns() < turnRecord.getTurns()) {
                    turnRecord = game;
                }
                if (durationRecord == null || game.getDurationSeconds() < durationRecord.getDurationSeconds()) {
                    durationRecord = game;
                }
                wonSpecies.add(game.getSpecies());
                wonBackgrounds.add(game.getBackground());
                wonCombos.add(combo);
                if (!"Atheist".equals(game.getGod())) {
                    wonGods.add(game.getGod());
                }
            }
            playedSpecies.add(game.getSpecies());
            playedBackgrounds.add(game.getBackground());
            playedCombos.add(combo);
            add(species, game.getSpecies(), game, win);
            add(backgrounds, game.getBackground(), game, win);
            add(gods, game.getGod(), game, win);
            add(combos, combo, game, win);
            add(versions, game.getVersion(), game, win);
            add(months, game.getTimestamp().substring(0, 4) + "-"
                    + game.getTimestamp().substring(4, 6), game, win);
            if (game.getOutcome() == DCSSMorgueGame.Outcome.LOSS) {
                add(deaths, game.getEndText(), game, false);
                add(deathPlaces, game.getPlace(), game, false);
            }
        }

        totalScore = score;
        bestScore = best;
        highestXl = bestXl;
        totalTurns = turns;
        totalDurationSeconds = duration;
        wins = winCount;
        totalWinRunes = winRunes;
        bestRunes = runesBest;
        bestScoreGame = scoreRecord;
        fastestWinByTurns = turnRecord;
        fastestWinByDuration = durationRecord;
        distinctSpeciesPlayed = playedSpecies.size();
        playableSpeciesCount = roster.getPlayableSpecies();
        playableBackgroundCount = roster.getPlayableBackgrounds();
        availableGodCount = roster.getAvailableGods();
        playableComboCount = roster.getPlayableCombos();
        distinctGodsWon = wonGods.size();
        distinctSpeciesWon = wonSpecies.size();
        distinctBackgroundsPlayed = playedBackgrounds.size();
        distinctBackgroundsWon = wonBackgrounds.size();
        distinctCombosPlayed = playedCombos.size();
        distinctCombosWon = wonCombos.size();

        List<DCSSMorgueGame> oldestFirst = new ArrayList<>(newestFirst);
        Collections.reverse(oldestFirst);
        int streak = 0;
        int longest = 0;
        for (DCSSMorgueGame game : oldestFirst) {
            if (game.getOutcome() == DCSSMorgueGame.Outcome.WIN) {
                streak++;
                longest = Math.max(longest, streak);
            } else {
                streak = 0;
            }
        }
        currentWinStreak = streak;
        bestWinStreak = longest;
        speciesBreakdowns = breakdowns(species);
        backgroundBreakdowns = breakdowns(backgrounds);
        godBreakdowns = breakdowns(gods);
        comboBreakdowns = breakdowns(combos);
        deathBreakdowns = breakdowns(deaths);
        deathPlaceBreakdowns = breakdowns(deathPlaces);
        versionBreakdowns = breakdowns(versions);
        monthBreakdowns = breakdowns(months);
    }

    public List<DCSSMorgueGame> getNewestFirst() { return Collections.unmodifiableList(newestFirst); }
    public int getGames() { return games; }
    public int getWins() { return wins; }
    public double getWinRate() { return games == 0 ? 0.0 : 100.0 * wins / games; }
    public long getTotalScore() { return totalScore; }
    public long getBestScore() { return bestScore; }
    public int getHighestXl() { return highestXl; }
    public long getTotalTurns() { return totalTurns; }
    public long getTotalDurationSeconds() { return totalDurationSeconds; }
    public int getTotalWinRunes() { return totalWinRunes; }
    public int getBestRunes() { return bestRunes; }
    public int getCurrentWinStreak() { return currentWinStreak; }
    public int getBestWinStreak() { return bestWinStreak; }
    public DCSSMorgueGame getBestScoreGame() { return bestScoreGame; }
    public DCSSMorgueGame getFastestWinByTurns() { return fastestWinByTurns; }
    public DCSSMorgueGame getFastestWinByDuration() { return fastestWinByDuration; }
    public List<DCSSMorgueBreakdown> getSpeciesBreakdowns() { return speciesBreakdowns; }
    public List<DCSSMorgueBreakdown> getBackgroundBreakdowns() { return backgroundBreakdowns; }
    public List<DCSSMorgueBreakdown> getGodBreakdowns() { return godBreakdowns; }
    public List<DCSSMorgueBreakdown> getComboBreakdowns() { return comboBreakdowns; }
    public List<DCSSMorgueBreakdown> getDeathBreakdowns() { return deathBreakdowns; }
    public List<DCSSMorgueBreakdown> getDeathPlaceBreakdowns() { return deathPlaceBreakdowns; }
    public List<DCSSMorgueBreakdown> getVersionBreakdowns() { return versionBreakdowns; }
    public List<DCSSMorgueBreakdown> getMonthBreakdowns() { return monthBreakdowns; }
    public int getDistinctSpeciesPlayed() { return distinctSpeciesPlayed; }
    public int getPlayableSpeciesCount() { return playableSpeciesCount; }
    public int getPlayableBackgroundCount() { return playableBackgroundCount; }
    public int getAvailableGodCount() { return availableGodCount; }
    public int getPlayableComboCount() { return playableComboCount; }
    public int getDistinctGodsWon() { return distinctGodsWon; }
    public int getDistinctSpeciesWon() { return distinctSpeciesWon; }
    public int getDistinctBackgroundsPlayed() { return distinctBackgroundsPlayed; }
    public int getDistinctBackgroundsWon() { return distinctBackgroundsWon; }
    public int getDistinctCombosPlayed() { return distinctCombosPlayed; }
    public int getDistinctCombosWon() { return distinctCombosWon; }

    private static void add(Map<String, Counter> counters, String label,
                            DCSSMorgueGame game, boolean win) {
        Counter counter = counters.get(label);
        if (counter == null) {
            counter = new Counter(label);
            counters.put(label, counter);
        }
        counter.games++;
        if (win) {
            counter.wins++;
        }
        counter.bestScore = Math.max(counter.bestScore, game.getScore());
        counter.bestXl = Math.max(counter.bestXl, game.getXl());
    }

    private static List<DCSSMorgueBreakdown> breakdowns(Map<String, Counter> counters) {
        List<DCSSMorgueBreakdown> rows = new ArrayList<>();
        for (Counter counter : counters.values()) {
            rows.add(new DCSSMorgueBreakdown(counter.label, counter.wins, counter.games,
                    counter.bestScore, counter.bestXl));
        }
        Collections.sort(rows, new Comparator<DCSSMorgueBreakdown>() {
            @Override
            public int compare(DCSSMorgueBreakdown left, DCSSMorgueBreakdown right) {
                int compare = Integer.compare(right.getWins(), left.getWins());
                if (compare == 0)
                    compare = Integer.compare(right.getGames(), left.getGames());
                return compare == 0 ? left.getLabel().compareTo(right.getLabel()) : compare;
            }
        });
        return Collections.unmodifiableList(rows);
    }

    private static final class Counter {
        final String label;
        int wins;
        int games;
        long bestScore;
        int bestXl;
        Counter(String label) { this.label = label; }
    }
}
