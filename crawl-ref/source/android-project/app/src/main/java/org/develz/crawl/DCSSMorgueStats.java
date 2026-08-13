package org.develz.crawl;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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
    private final List<DCSSMorgueBreakdown> speciesBreakdowns;
    private final List<DCSSMorgueBreakdown> backgroundBreakdowns;
    private final List<DCSSMorgueBreakdown> godBreakdowns;
    private final List<DCSSMorgueBreakdown> comboBreakdowns;

    public DCSSMorgueStats(List<DCSSMorgueGame> games) {
        newestFirst = new ArrayList<>(games);
        newestFirst.sort(Comparator.comparing(DCSSMorgueGame::getTimestamp).reversed());
        this.games = newestFirst.size();

        long score = 0;
        long turns = 0;
        long duration = 0;
        long best = 0;
        int bestXl = 0;
        int winCount = 0;
        int winRunes = 0;
        int runesBest = 0;
        Map<String, Counter> species = new HashMap<>();
        Map<String, Counter> backgrounds = new HashMap<>();
        Map<String, Counter> gods = new HashMap<>();
        Map<String, Counter> combos = new HashMap<>();

        for (DCSSMorgueGame game : newestFirst) {
            score += game.getScore();
            turns += game.getTurns();
            duration += game.getDurationSeconds();
            best = Math.max(best, game.getScore());
            bestXl = Math.max(bestXl, game.getXl());
            boolean win = game.getOutcome() == DCSSMorgueGame.Outcome.WIN;
            if (win) {
                winCount++;
                winRunes += game.getRunes();
                runesBest = Math.max(runesBest, game.getRunes());
            }
            add(species, game.getSpecies(), game, win);
            add(backgrounds, game.getBackground(), game, win);
            add(gods, game.getGod(), game, win);
            add(combos, game.getSpecies() + " " + game.getBackground(), game, win);
        }

        totalScore = score;
        bestScore = best;
        highestXl = bestXl;
        totalTurns = turns;
        totalDurationSeconds = duration;
        wins = winCount;
        totalWinRunes = winRunes;
        bestRunes = runesBest;

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
    public List<DCSSMorgueBreakdown> getSpeciesBreakdowns() { return speciesBreakdowns; }
    public List<DCSSMorgueBreakdown> getBackgroundBreakdowns() { return backgroundBreakdowns; }
    public List<DCSSMorgueBreakdown> getGodBreakdowns() { return godBreakdowns; }
    public List<DCSSMorgueBreakdown> getComboBreakdowns() { return comboBreakdowns; }

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
        rows.sort(Comparator.comparingInt(DCSSMorgueBreakdown::getWins).reversed()
                .thenComparing(Comparator.comparingInt(DCSSMorgueBreakdown::getGames).reversed())
                .thenComparing(DCSSMorgueBreakdown::getLabel));
        return Collections.unmodifiableList(rows);
    }

    private static final class Counter {
        final String label;
        int wins;
        int games;
        long bestScore;
        int bestXl;

        Counter(String label) {
            this.label = label;
        }
    }
}
