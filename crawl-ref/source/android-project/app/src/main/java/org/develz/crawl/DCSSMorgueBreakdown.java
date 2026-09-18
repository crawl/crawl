package org.develz.crawl;

public final class DCSSMorgueBreakdown {
    private final String label;
    private final int wins;
    private final int games;
    private final long bestScore;
    private final int bestXl;

    public DCSSMorgueBreakdown(String label, int wins, int games, long bestScore, int bestXl) {
        this.label = label;
        this.wins = wins;
        this.games = games;
        this.bestScore = bestScore;
        this.bestXl = bestXl;
    }

    public String getLabel() { return label; }
    public int getWins() { return wins; }
    public int getGames() { return games; }
    public double getWinRate() { return games == 0 ? 0.0 : 100.0 * wins / games; }
    public long getBestScore() { return bestScore; }
    public int getBestXl() { return bestXl; }
}
